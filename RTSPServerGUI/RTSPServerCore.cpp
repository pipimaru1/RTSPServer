#define _WIN32_WINNT 0x0600  // inet_ntop を利用するために必要な場合がある(Windows Vista以降)

#include "stdafx.h"

#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <conio.h>  // getch() / _getch()
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <vector>
#include <mutex>
#include <filesystem>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspurl.h>  // gst_rtsp_url_parse

#include "RTSPServer.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gstrtsp-1.0.lib")      // または "libgstrtsp-1.0.lib"
#pragma comment(lib, "gstrtspserver-1.0.lib") // すでに入っていれば不要

gboolean disable_rtcp;// = DEFAULT_DISABLE_RTCP;


//////////////////////////////////////////////////////////
//
// 2025/12/16
// 追加：受信状態の通知
// 
///////////////////////////////////////////////////////////
#ifdef _WIN32
HWND g_gui_hwnd = nullptr;
std::atomic<bool> g_rx{ false };

// ★追加：最後に受信した時刻（μs）
std::atomic<gint64> g_last_rx_us{ 0 };
// ★追加：受信が途切れたとみなす時間（udpsrc timeout と同じ 5秒推奨）
const gint64 g_rx_timeout_us = 5 * G_USEC_PER_SEC;
// ★追加：監視タイマID（1ch想定）
guint g_rx_watch_id = 0;

void SetGuiNotifyHwnd(HWND hwnd)
{
    g_gui_hwnd = hwnd;
}

void NotifyRx(bool receiving)
{
    bool prev = g_rx.exchange(receiving);
    if (prev == receiving) return;

    if (g_gui_hwnd)
    {
        PostMessageW(g_gui_hwnd, WM_APP_RX_STATUS, receiving ? 1 : 0, 0);
    }
}

// ★追加：一定時間受信が無ければOFFにする監視
gboolean rx_watch_cb(gpointer)
{
    if (!g_rx.load())
        return G_SOURCE_CONTINUE; // 既にOFFなら何もしない

    const gint64 now = g_get_monotonic_time();
    const gint64 last = g_last_rx_us.load();

    if (last != 0 && (now - last) > g_rx_timeout_us)
    {
        NotifyRx(false);
    }
    return G_SOURCE_CONTINUE;
}
#endif

GstPadProbeReturn udpsrc_buffer_probe(GstPad*, GstPadProbeInfo* info, gpointer)
{
#ifdef _WIN32
    // ★追加：最後に受信した時刻を更新
    g_last_rx_us.store(g_get_monotonic_time()); 
    // バッファが流れた＝受信できている
    NotifyRx(true);
#endif
    return GST_PAD_PROBE_OK;
}
//////////////////////////////////////////////////////////
//異常があった時のコールバック関数
gboolean bus_watch_callback_old(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            GError* err = NULL;
            gchar* debug = NULL;
            gst_message_parse_error(msg, &err, &debug);
            g_printerr("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            break;
        default:
            break;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////
//異常があった時のコールバック関数
gboolean bus_watch_callback(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    MediaCtx* ctx = static_cast<MediaCtx*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            g_printerr("Error: %s\n", err->message);
            g_error_free(err); g_free(dbg);
            // エラーでもリスタート
            g_idle_add(restart_pipeline_idle, ctx->pipeline);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream -> restart\n");
            g_idle_add(restart_pipeline_idle, ctx->pipeline);
            break;

        case GST_MESSAGE_ELEMENT: {
            // udpsrc の無信号タイムアウトも拾ってリスタート
            const GstStructure* s = gst_message_get_structure(msg);
            if (s && gst_structure_has_name(s, "GstUDPSrcTimeout")) {
                g_print("UDPSRC timeout -> restart\n");
#ifdef _WIN32
                NotifyRx(false); // ★無信号 通知
#endif
                g_idle_add(restart_pipeline_idle, ctx->pipeline);
            }
            break;
        }

        default: break;
    }
    return TRUE;
}


#ifdef USE_DUMMY_CLIENT
//////////////////////////////////////////////////////////
// パイプラインの取得
void media_configure_cb_2(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
{
    // RTSP が内部生成したメディア(=パイプライン)を取得
    GstElement* element = gst_rtsp_media_get_element(media);
    if (!element) 
        return;

    // バスを取得
    GstBus* bus = gst_element_get_bus(element);
    if (bus) {
        // bus_watch_callback() を登録してメインループで監視
        gst_bus_add_watch(bus, bus_watch_callback, user_data);
        gst_object_unref(bus);
    }
}

//////////////////////////////////////////////////////////
//
void media_configure_cb(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
{
    // user_data は g_signal_connect で渡した GMainLoop* として受け取る
    GMainLoop* loop = static_cast<GMainLoop*>(user_data);

    GstElement* element = gst_rtsp_media_get_element(media);
    if (!element) 
        return;

    // 2025/12/16
    // ★受信検出用：udpsrc(src0) の src pad に probe を付ける
    {
        GstElement* src = gst_bin_get_by_name(GST_BIN(element), "src0");
        if (src)
        {
            // 二重登録防止（src に印を付ける）
            if (!g_object_get_data(G_OBJECT(src), "rx-probe-installed"))
            {
                GstPad* pad = gst_element_get_static_pad(src, "src");
                if (pad)
                {
                    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, udpsrc_buffer_probe, nullptr, nullptr);
                    gst_object_unref(pad);
                    g_object_set_data(G_OBJECT(src), "rx-probe-installed", GINT_TO_POINTER(1));
                }
            }
            gst_object_unref(src);
        }
    }
#ifdef _WIN32
    // 受信監視タイマ開始（まだなら開始）
    if (g_rx_watch_id == 0)
    {
        g_rx_watch_id = g_timeout_add(500, rx_watch_cb, nullptr);
    }
#endif

    // bus_watch_callback が期待する MediaCtx* をここで作る
    MediaCtx* ctx = g_new0(MediaCtx, 1);
    ctx->loop = loop;
    ctx->pipeline = GST_ELEMENT(gst_object_ref(element)); // パイプライン参照を保持

    if (GstBus* bus = gst_element_get_bus(element)) {
        gst_bus_add_watch(bus, bus_watch_callback, ctx);  // ← MediaCtx* を渡す
        gst_object_unref(bus);
    }
    gst_object_unref(element);

    // media 破棄と同時に ctx を解放（保持していた pipeline も unref）
    g_object_set_data_full(G_OBJECT(media), "media-ctx", ctx, [](gpointer p) {
        MediaCtx* c = static_cast<MediaCtx*>(p);
        if (c->pipeline) 
            gst_object_unref(c->pipeline);
        g_free(c);
        });
}

#else
//////////////////////////////////////////////////////////
// パイプラインの取得
void media_configure_cb_1(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
{
    GstElement* element = gst_rtsp_media_get_element(media);
    if (!element) return;

    // コンテキストを作る
    MediaCtx* ctx = g_new0(MediaCtx, 1);
    ctx->loop = (GMainLoop*)user_data; // 既存の引数（loop）
    ctx->pipeline = element;

    // バスに登録（user_data に ctx を渡す）
    GstBus* bus = gst_element_get_bus(element);
    gst_bus_add_watch(bus, bus_watch_callback, ctx);
    gst_object_unref(bus);

    // media が破棄されたら ctx も破棄
    g_object_set_data_full(G_OBJECT(media), "media-ctx", ctx, (GDestroyNotify)g_free);
}

//////////////////////////////////////////////////////////
// パイプラインの取得
void media_configure_cb(GstRTSPMediaFactory* factory,
    GstRTSPMedia* media,
    gpointer user_data)
{
    ServerCtx* sctx = static_cast<ServerCtx*>(user_data);

    GstElement* element = gst_rtsp_media_get_element(media);
    if (!element) return;

    // bus_watch_callback は MediaCtx* を受ける設計なら、ここで MediaCtx を作って渡す
    MediaCtx* mctx = g_new0(MediaCtx, 1);
    mctx->loop = sctx->loop;
    mctx->pipeline = element; // 必要なら gst_object_ref(element) しておく

    if (GstBus* bus = gst_element_get_bus(element)) {
        gst_bus_add_watch(bus, bus_watch_callback, mctx);
        gst_object_unref(bus);
    }
    gst_object_unref(element);

    // media 破棄時に mctx も解放
    g_object_set_data_full(G_OBJECT(media), "media-ctx", mctx, (GDestroyNotify)g_free);
}
#endif

//////////////////////////////////////////////////////////
// 1回キックして数秒で切断するダミークライアント
void KickRtspOnceAndDisconnect(GMainLoop* loop, int out_port, const std::string& channel, int timer) 
{
    std::ostringstream p;
    p << "playbin uri=rtsp://127.0.0.1:" << out_port << "/" << channel
        << " video-sink=fakesink audio-sink=fakesink";

    GError* err = nullptr;
    GstElement* pipe = gst_parse_launch(p.str().c_str(), &err);
    if (!pipe) {
        g_printerr("kick parse error: %s\n", err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }

    // 状態遷移・エラー監視（任意）
    GstBus* bus = gst_element_get_bus(pipe);
    gst_bus_add_watch(bus, bus_watch_callback, loop);
    gst_object_unref(bus);

    gst_element_set_state(pipe, GST_STATE_PLAYING);

    // ★ 2秒後に自動で切断（TEARDOWN 相当）
    //g_timeout_add_seconds(30, [](gpointer data) -> gboolean {
    g_timeout_add_seconds(timer, [](gpointer data) -> gboolean {
        GstElement* p = static_cast<GstElement*>(data);
        gst_element_set_state(p, GST_STATE_NULL);
        gst_object_unref(p);
        g_print("Dummy RTSP kick disconnected.\n");
        return FALSE; // 1回だけ
        }, pipe);
}

//////////////////////////////////////////////////////////
//ダミーRTSPクライアントの起動
void StartDummyRtspClient_old(GMainLoop* loop, int out_port, const std::string& channel) {
    std::ostringstream p;
    p << "playbin uri=rtsp://127.0.0.1:" << out_port << "/" << channel
        << " video-sink=fakesink audio-sink=fakesink";
    GError* err = nullptr;
    GstElement* pipe = gst_parse_launch(p.str().c_str(), &err);
    if (!pipe) { g_printerr("kick client parse error: %s\n", err ? err->message : "unknown"); if (err) g_error_free(err); return; }
    GstBus* bus = gst_element_get_bus(pipe);
    gst_bus_add_watch(bus, bus_watch_callback, loop);
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_PLAYING);
}

//////////////////////////////////////////////////////////
//ダミーRTSPクライアントの起動
gboolean dummy_bus_cb(GstBus* bus, GstMessage* msg, gpointer) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError* e = nullptr; gchar* d = nullptr;
        gst_message_parse_error(msg, &e, &d);
        g_printerr("[dummy] Error: %s\n", e->message);
        g_error_free(e); g_free(d);
    }
    return TRUE;
}
void StartDummyRtspClient(GMainLoop* loop, int out_port, const std::string& channel) {
    std::ostringstream p;
    p << "playbin uri=rtsp://127.0.0.1:" << out_port << "/" << channel
        << " video-sink=fakesink audio-sink=fakesink";
    GError* err = nullptr;
    GstElement* pipe = gst_parse_launch(p.str().c_str(), &err);
    if (!pipe) 
    { 
        g_printerr("[dummy] parse error: %s\n", err ? err->message : "unknown"); 
        if (err) 
            g_error_free(err); 
        return; 
    }
    GstBus* bus = gst_element_get_bus(pipe);
    gst_bus_add_watch(bus, dummy_bus_cb, nullptr); // ← 監視のみ
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_PLAYING);
}

//////////////////////////////////////////////////////////
//ダミーRTSPクライアントの起動　再起動版
struct DummyCtx { int port; std::string ch; GstElement* pipe = nullptr; };

static gboolean dummy_restart_cb(gpointer data) {
    DummyCtx* dc = static_cast<DummyCtx*>(data);
    if (dc->pipe) { gst_element_set_state(dc->pipe, GST_STATE_NULL); gst_object_unref(dc->pipe); dc->pipe = nullptr; }
    std::ostringstream p;
    p << "rtspsrc location=rtsp://127.0.0.1:" << dc->port << "/" << dc->ch
        << " protocols=tcp do-rtcp=true tcp-timeout=0 timeout=0 "
        << "! fakesink sync=false";
    GError* err = nullptr;
    dc->pipe = gst_parse_launch(p.str().c_str(), &err);
    if (!dc->pipe) { g_printerr("[dummy] parse error: %s\n", err ? err->message : "unknown"); if (err) g_error_free(err); return FALSE; }
    GstBus* bus = gst_element_get_bus(dc->pipe);
    gst_bus_add_watch(bus, [](GstBus*, GstMessage* m, gpointer u)->gboolean {
        DummyCtx* d = static_cast<DummyCtx*>(u);
        switch (GST_MESSAGE_TYPE(m)) {
            case GST_MESSAGE_ERROR:
            case GST_MESSAGE_EOS:
                g_timeout_add_seconds(1, dummy_restart_cb, d); // ← 1秒後に再作成
                return FALSE; // このwatchは終了
            default: return TRUE;
        }
        }, dc);
    gst_object_unref(bus);
    gst_element_set_state(dc->pipe, GST_STATE_PLAYING);
    g_print("[dummy] connected\n");
    return FALSE; // 一回だけ
}

// 起動時に一度呼ぶ
void StartManagedDummy(int port, const std::string& ch) {
    auto* dc = new DummyCtx{ port, ch, nullptr };
    g_timeout_add(0, dummy_restart_cb, dc); // 即接続、切れたら自動再接続
}


//////////////////////////////////////////////////////////
// 実行中のシステムのIPアドレス(IPv4/IPv6)を取得して返す関数
std::vector<std::string> getLocalIPAddresses()
{
    // 戻り値用のベクタ
    std::vector<std::string> addresses;

    // Winsockの初期化
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return addresses;  // 空のベクタを返す
    }

    // ホスト名(コンピュータ名)を取得
    char hostname[NI_MAXHOST];
    if (gethostname(hostname, NI_MAXHOST) == SOCKET_ERROR) {
        std::cerr << "gethostname failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return addresses;  // 空のベクタを返す
    }

    // アドレス情報を取得 (IPv4, IPv6問わず取得)
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC;    // IPv4 / IPv6 両方取得
    hints.ai_socktype = SOCK_STREAM;  // TCP(SOCK_STREAM)を指定(UDPならSOCK_DGRAMなど)
    hints.ai_flags = AI_PASSIVE;   // 「任意」フラグ(自マシンに割り当てられたIPの情報を取得)

    struct addrinfo* res = nullptr;
    result = getaddrinfo(hostname, nullptr, &hints, &res);
    if (result != 0) {
        std::cerr << "getaddrinfo failed. Error: " << result << std::endl;
        WSACleanup();
        return addresses;  // 空のベクタを返す
    }

    // 取得したアドレスリストを走査して文字列に変換し、ベクタに格納
    for (auto ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
        char ipStr[INET6_ADDRSTRLEN] = { 0 };

        if (ptr->ai_family == AF_INET) {
            // IPv4
            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof(ipStr));
            addresses.push_back(ipStr);
        }
        else if (ptr->ai_family == AF_INET6) {
            // IPv6
            sockaddr_in6* ipv6 = reinterpret_cast<sockaddr_in6*>(ptr->ai_addr);
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipStr, sizeof(ipStr));
            addresses.push_back(ipStr);
        }
    }

    // 後片付け
    freeaddrinfo(res);
    WSACleanup();

    // 取得したIPアドレス一覧を返す
    return addresses;
}

//////////////////////////////////////////////////////////
// 事前起動（ダミー視聴者なしでパイプラインを動かす）
GstRTSPMedia* prewarm_media_without_client(GstRTSPMediaFactory* factory,
    GstRTSPServer* /*unused*/,
    int out_port,
    const std::string& channel,
    GMainLoop* loop)
{
    // URL を作る（※ URL は construct のヒント。mount には触れない）
    std::ostringstream u;
    u << "rtsp://127.0.0.1:" << out_port << "/" << channel;

    GstRTSPUrl* url = nullptr;
    if (gst_rtsp_url_parse(u.str().c_str(), &url) != GST_RTSP_OK) {
        g_printerr("rtsp url parse failed: %s\n", u.str().c_str());
        return nullptr;
    }

    GstRTSPMedia* media = gst_rtsp_media_factory_construct(factory, url);
    gst_rtsp_url_free(url);
    if (!media) {
        g_printerr("construct() failed\n");
        return nullptr;
    }
    gst_rtsp_media_set_reusable(media, TRUE);

    // prepare はスレッド不要版（thread 引数を要求する版ならスキップしてOK）
    // もしあなたのヘッダが (media, GstRTSPThread*) 版なら、ここは呼ばずに
    // 下の element->PLAYING だけで十分です（実際それで動きます）。

    // パイプラインを PLAYING に
    //if (GstElement* element = gst_rtsp_media_get_element(media)) {
    //    gst_element_set_state(element, GST_STATE_PLAYING);
    //    if (GstBus* bus = gst_element_get_bus(element)) {
    //        gst_bus_add_watch(bus, bus_watch_callback, loop);
    //        gst_object_unref(bus);
    //    }
    //    gst_object_unref(element);
    //}
    if (GstElement* element = gst_rtsp_media_get_element(media)) {
        gst_element_set_state(element, GST_STATE_PLAYING);

        if (GstBus* bus = gst_element_get_bus(element)) {
            // ★ MediaCtx を作成して正しく渡す
            MediaCtx* mctx = g_new0(MediaCtx, 1);
            mctx->loop = loop;
            mctx->pipeline = GST_ELEMENT(gst_object_ref(element)); // パイプライン参照を保持

            gst_bus_add_watch(bus, bus_watch_callback, mctx);
            gst_object_unref(bus);

            // media 破棄時に mctx も解放（pipeline も unref）
            g_object_set_data_full(G_OBJECT(media), "prewarm-ctx", mctx,
                [](gpointer p) {
                    MediaCtx* c = static_cast<MediaCtx*>(p);
                    if (c->pipeline) gst_object_unref(c->pipeline);
                    g_free(c);
                });
        }
        gst_object_unref(element);
    }
    return media;
}

//////////////////////////////////////////////////////////
std::mutex mtx;
//////////////////////////////////////////////////////////
int OpenRTSPServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[])
{
    //   GMainLoop* loop;
    GstRTSPServer* server;
    GstRTSPMountPoints* mounts;
    GstRTSPMediaFactory* factory;
    GOptionContext* optctx;
    GError* error = NULL;

    {
        std::lock_guard<std::mutex> lock(mtx); // ロック

        bool hls_on = false;

        //HLS用のディレクトリ作成
        std::string hls_dir = "C:/hls/" + channel_name + "-" + std::to_string(out_port);
        std::filesystem::create_directories(hls_dir);


        std::ostringstream str_GOptionEntry_01;
        str_GOptionEntry_01 << "Port to listen on (default: " << out_port << ")";

        std::string str_outport = std::to_string(out_port);
        const size_t buffer_size = 32;                          // バッファのサイズ
        static char tmp_port[buffer_size];                      // 書き込み可能なバッファ
        //strncpy(tmp_port, str_outport.c_str(), buffer_size - 1);
        strcpy_s(tmp_port, buffer_size, str_outport.c_str());//値コピー

        std::ostringstream str_pipeline;
        std::ostringstream sstr_channel;

        if (0)
        {
            str_pipeline
                << "( udpsrc port=" << in_port
                << " caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" "
                << "! rtph264depay ! h264parse "
                << "! rtph264pay name=pay0 pt=96 )";
        }
        else
        {
            str_pipeline.str("");
            str_pipeline
                //<< "( udpsrc port=" << in_port            //2025/12/16
                << "( udpsrc name=src0 port=" << in_port    //2025/12/16
                << " timeout=5000000 " // 5秒無信号でタイムアウト (µs)
                << " caps=\\\"application/x-rtp,media=video,encoding-name=H264,payload=96\\\" "
                << "! rtph264depay "
                << "! h264parse config-interval=1 "          // まず全体用のparser
                << "! tee name=t "
                // ---- RTSP 枝（★リーキーqueueで詰まり回避）
                // 良く止まるので過去に無かった指令を外してみる 11/18
                << "t. ! queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 leaky=downstream "
                << "! h264parse config-interval=1 "
                << "! rtph264pay name=pay0 pt=96 "
                // ---- HLS 枝
                << "t. ! queue "
                << "! h264parse config-interval=1 "
                << "! mpegtsmux "
                << "! hlssink "
                << "location=" << hls_dir << "/seg%05d.ts "
                << "playlist-location=" << hls_dir << "/index.m3u8 "
                //<< "target-duration=2 max-files=5 "
                << "target-duration=2 "
                << "playlist-length=20 "   // プレイリストには 20 セグメント載せる（= 約40秒分）
                << "max-files=40 "         // TSファイルも 40 個まで保持
                << ")";

                //playlist - length
                //プレイリストに載せるセグメント数。デフォルトは 5 です
                //gstreamer.freedesktop.org
                //→ ここを 10〜20 くらいに増やすと、「少し遅れて再生開始しても、まだそのセグメントが残っている」状態を作れます。
                //max - files
                //実際にディスクに残す.ts の数。playlist - length と同じか、それより少し多めに。
                //target - duration = 2 はそのままでも構いませんが、
                //配信の遅延を多少増やしても良いなら、target - duration = 3 か 4 にすると、
                //「1 セグメントあたりのデータ量が増えて、バッファが安定しやすい」という面もあります。

#ifdef _DEBUG            
            std::cerr << "str_pipeline : " << str_pipeline.str() << std::endl;
#endif
            hls_on = true;
        }

        sstr_channel << "/" << channel_name;

        static GOptionEntry entries[] = {
            {"port",         'p',  0, G_OPTION_ARG_STRING, &(tmp_port),  str_GOptionEntry_01.str().c_str(),               "PORT"},
            {"disable-rtcp", '\0', 0, G_OPTION_ARG_NONE,   &disable_rtcp,"Whether RTCP should be disabled (default false)", NULL},
            {NULL}
        };

        optctx = g_option_context_new("<launch line> - Test RTSP Server, Launch\n\n"
            "Example: \"( videotestsrc ! x264enc ! rtph264pay name=pay0 pt=96 )\"");
        g_option_context_add_main_entries(optctx, entries, NULL);
        g_option_context_add_group(optctx, gst_init_get_option_group());

        if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
            g_printerr("Error parsing options: %s\n", error->message);
            g_option_context_free(optctx);
            g_clear_error(&error);
            return -1;
        }
        g_option_context_free(optctx);

        loop = g_main_loop_new(NULL, FALSE);

        /* create a server instance */
        server = gst_rtsp_server_new();
        g_object_set(server, "service", str_outport.c_str(), NULL);
        mounts = gst_rtsp_server_get_mount_points(server);


#ifdef USE_DUMMY_CLIENT
        // factory 設定
        factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, str_pipeline.str().c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
		gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp); //HLS
        gst_rtsp_media_factory_set_suspend_mode(factory, GST_RTSP_SUSPEND_MODE_NONE);

        // シグナルを接続
        g_signal_connect(factory, "media-configure",
            G_CALLBACK(media_configure_cb),
            loop /* ユーザーデータ */);

		// ADD Factory
        gst_rtsp_mount_points_add_factory(mounts, sstr_channel.str().c_str(), factory);
        g_object_unref(mounts);

        // サーバ attach
        gst_rtsp_server_attach(server, NULL);

        //ダミーRTSPクライアントの起動
        //StartDummyRtspClient(loop, out_port, channel_name);
        StartManagedDummy(out_port, channel_name);
        //機能しないような気がする
        //KickRtspOnceAndDisconnect(loop, out_port, channel_name, 20);

#else //USE_DUMMY_CLIENT

        factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, str_pipeline.str().c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
        gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp); //HLS
        gst_rtsp_media_factory_set_suspend_mode(factory, GST_RTSP_SUSPEND_MODE_NONE); // ★常に

        // ★ ServerCtx を用意して media-configure に渡す
        ServerCtx* sctx = g_new0(ServerCtx, 1);
        sctx->loop = loop;   // 後で loop 代入するので一旦空でも可
        sctx->server = nullptr; // 後で server を入れる
        g_signal_connect(factory, "media-configure", G_CALLBACK(media_configure_cb), sctx);

        // ADD Factory
        gst_rtsp_mount_points_add_factory(mounts, sstr_channel.str().c_str(), factory);
        g_object_unref(mounts);

        // サーバ attach
        gst_rtsp_server_attach(server, NULL);

        // ★ ここで sctx を完成させる（loop/server を入れる）
        sctx->loop = loop;   // この時点で loop は g_main_loop_new() 済み
        sctx->server = server;

        // ★ ダミー接続は使わない
        // StartDummyRtspClient(loop, out_port, channel_name);

        // ★ パターンC：ダミー無しでメディア起動
        //static GstRTSPMedia* g_prewarmed_media = nullptr;
        //g_prewarmed_media = prewarm_media_without_client(
        //    factory, server, out_port, channel_name, loop);
        //if (!g_prewarmed_media) {
        //    g_printerr("prewarm failed; HLS may need an initial client.\n");
        //}

        //HLS
        gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp);
#endif
        /* start serving */
        //std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;

        // IPアドレス一覧を取得
        std::vector<std::string> ipList = getLocalIPAddresses();

        std::cout << "-------------------------------------" << std::endl;
        //std::cout << __FILE__ << " " << __func__ << std::endl;
        //std::cout << __func__ <<" BUILD:["<< __DATE__ << " " << __TIME__<<"]" << std::endl;
        std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;

        // 結果の表示
        if (ipList.empty()) {
            std::cout << "Can not find other ip address." << std::endl;
            //std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;
        }
        else {
            //std::cout << "取得したIPアドレス一覧:" << std::endl;
            for (const auto& ip : ipList) {
                std::cout << "Stream Ready at rtsp://" << ip << ":" << str_outport << sstr_channel.str() << std::endl;
            }
        }
        //HLSの準備完了表示
        if (hls_on)
        {
            std::cout << "Stream Ready at HLS:" << hls_dir.c_str() << std::endl;
        }
    }

    g_main_loop_run(loop);
    return 0;
}

//////////////////////////////////////////////////////////
//HLS 対応 家じゃテストできない
//////////////////////////////////////////////////////////
int OpenHLSServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[])
{
    //   GMainLoop* loop;
    GstRTSPServer* server;
    GstRTSPMountPoints* mounts;
    GstRTSPMediaFactory* factory;
    GOptionContext* optctx;
    GError* error = NULL;

    std::ostringstream str_GOptionEntry_01;
    str_GOptionEntry_01 << "Port to listen on (default: " << out_port << ")";

    std::string str_outport = std::to_string(out_port);
    const size_t buffer_size = 32;                          // バッファのサイズ
    static char tmp_port[buffer_size];                      // 書き込み可能なバッファ
    //strncpy(tmp_port, str_outport.c_str(), buffer_size - 1);
    strcpy_s(tmp_port, buffer_size, str_outport.c_str());//値コピー

    std::ostringstream str_pipeline;
    std::ostringstream sstr_channel;

    //str_pipeline << "( udpsrc port=" << in_port << " caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" ! rtph264depay ! h264parse ! rtph264pay name=pay0 pt=96 )";
    str_pipeline << "( udpsrc port=" << in_port << " caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" ! rtph264depay ! h264parse ! mpegtsmux ! hlssink \
                        location=/path/to/hls/segment%05d.ts \
                        playlist-location=/path/to/hls/playlist.m3u8 \
                        target-duration=2 \
                        max-files=5 )";
    sstr_channel << "/" << channel_name;

    static GOptionEntry entries[] = {
        {"port",         'p',  0, G_OPTION_ARG_STRING, &(tmp_port),  str_GOptionEntry_01.str().c_str(),               "PORT"},
        {"disable-rtcp", '\0', 0, G_OPTION_ARG_NONE,   &disable_rtcp,"Whether RTCP should be disabled (default false)", NULL},
        {NULL}
    };

    optctx = g_option_context_new("<launch line> - Test RTSP Server, Launch\n\n"
        "Example: \"( videotestsrc ! x264enc ! rtph264pay name=pay0 pt=96 )\"");
    g_option_context_add_main_entries(optctx, entries, NULL);
    g_option_context_add_group(optctx, gst_init_get_option_group());

    if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
        g_printerr("Error parsing options: %s\n", error->message);
        g_option_context_free(optctx);
        g_clear_error(&error);
        return -1;
    }
    g_option_context_free(optctx);

    loop = g_main_loop_new(NULL, FALSE);

    /* create a server instance */
    server = gst_rtsp_server_new();

    //g_object_set(server, "service", port, NULL);
    g_object_set(server, "service", str_outport.c_str(), NULL);

    /* get the mount points for this server, every server has a default object
        * that be used to map uri mount points to media factories */
    mounts = gst_rtsp_server_get_mount_points(server);

    /* make a media factory for a test stream. The default media factory can use
        * gst-launch syntax to create pipelines.
        * any launch line works as long as it contains elements named pay%d. Each
        * element with pay%d names will be a stream */
    factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, str_pipeline.str().c_str());

    // シグナルを接続
    g_signal_connect(factory, "media-configure",
        G_CALLBACK(media_configure_cb),
        loop /* ユーザーデータ */);

    gst_rtsp_media_factory_set_shared(factory, TRUE);
    gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp);

    /* attach the test factory to the /test url */
    gst_rtsp_mount_points_add_factory(mounts, sstr_channel.str().c_str(), factory);

    /* don't need the ref to the mapper anymore */
    g_object_unref(mounts);

    /* attach the server to the default maincontext */
    gst_rtsp_server_attach(server, NULL);

    /* start serving */
    //std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;

    // IPアドレス一覧を取得
    std::vector<std::string> ipList = getLocalIPAddresses();
    std::cout << "-------------------------------------" << std::endl;
    std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;

    // 結果の表示
    if (ipList.empty()) {
        std::cout << "Can not find other ip address." << std::endl;
        //std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;
    }
    else {
        //std::cout << "取得したIPアドレス一覧:" << std::endl;
        for (const auto& ip : ipList) {
            std::cout << "Stream Ready at rtsp://" << ip << ":" << str_outport << sstr_channel.str() << std::endl;
        }
    }
    g_main_loop_run(loop);
    return 0;
}

