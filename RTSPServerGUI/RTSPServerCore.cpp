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

// 受信中かどうか
std::atomic<bool> g_rx{ false };
//std::atomic<bool> gRx{ false };

// ★追加：最後に受信した時刻（μs）
//std::atomic<gint64> g_last_rx_us{ 0 };
std::atomic<gint64> g_last_rx_us{ 0 };

// ★追加：受信が途切れたとみなす時間（udpsrc timeout と同じ 5秒推奨）
const gint64 g_rx_timeout_us = 5 * G_USEC_PER_SEC;

// ★追加：監視タイマID（1ch想定）
guint g_rx_watch_id = 0;


////////////////////////////////////////////////////////
//
// GUI（ダイアログ）の HWND を RTSPServerCore 側に教える関数
// 
// OpenRTSPServer() や probe / bus watch は 別スレッドや GMainLoop の文脈で動きます
// そこから GUI のチェックボックスを直接触るのは危険（Win32 の UI は基本 GUI スレッド専用）
// なので「あとで Windows メッセージで GUI スレッドに通知する」方式にしている
// そのために、GUI起動時に SetGuiNotifyHwnd(hDlg) を1回呼ぶ
// Core 側は g_gui_hwnd に覚える
// 以後 NotifyRx() が PostMessage() でその HWND に通知できる
// …という“通知先登録”が SetGuiNotifyHwnd です。
// 
////////////////////////////////////////////////////////
void SetGuiNotifyHwnd(HWND hwnd)
{
    g_gui_hwnd = hwnd;
}

////////////////////////////////////////////////////////
// 
// 受信状態の通知
// _ch: チャンネル番号（0～MAXCH-1）
// 受信状態（受信中 / 受信なし）を GUI に通知する関数
// 
////////////////////////////////////////////////////////
void NotifyRx(bool receiving, UINT _ch)
{
    bool prev = g_rx.exchange(receiving);
    if (prev == receiving) return;

    if (g_gui_hwnd)
    {
        PostMessageW(g_gui_hwnd, WM_APP_RX_STATUS + _ch, receiving ? 1 : 0, 0);
    }
}

////////////////////////////////////////////////////////
// 
// 受信状態の通知
// _ch: チャンネル番号（0～MAXCH-1）
// 受信状態（受信中 / 受信なし）を GUI に通知する関数
// 拡張版
// 
////////////////////////////////////////////////////////
void NotifyRxEx(bool receiving, RTSPCtrl* _rctrl)
{
	if (_rctrl)
    {
        UINT _ch = _rctrl->ch;
        bool prev = _rctrl->g_rx.exchange(receiving);

        if (prev == receiving) 
            return;

        if (g_gui_hwnd)
        {
            PostMessageW(g_gui_hwnd, WM_APP_RX_STATUS + _ch, receiving ? 1 : 0, 0);
        }
    }
}

//////////////////////////////////////////////////////////
// 
// ★追加：一定時間受信が無ければOFFにする
// 
///////////////////////////////////////////////////////////
gboolean CALLBK_RxWatch(gpointer)
{
	int ch = 0; // 1ch想定

    if (!g_rx.load())
        return G_SOURCE_CONTINUE; // 既にOFFなら何もしない

    const gint64 now = g_get_monotonic_time();
    const gint64 last = g_last_rx_us.load();

    if (last != 0 && (now - last) > g_rx_timeout_us)
    {
        NotifyRx(false, ch);
    }
    return G_SOURCE_CONTINUE;
}

///////////////////////////////////////////////////////////
//
// 拡張版：チャンネル番号を user_data で受け取る
// 
///////////////////////////////////////////////////////////
gboolean CALLBK_RxWatchEx(gpointer user_data)
{
    RTSPCtrl* _rctrl;

    if (user_data)
    {
        _rctrl = static_cast<RTSPCtrl*>(user_data);
        //ch = _rctrl->ch;
        //int ch = 0; // 1ch想定

        if (!_rctrl->g_rx.load())
            return G_SOURCE_CONTINUE; // 既にOFFなら何もしない


        const gint64 now = g_get_monotonic_time();
        const gint64 last = _rctrl->g_last_rx_us.load();

        if (last != 0 && (now - last) > g_rx_timeout_us)
        {
            NotifyRxEx(false, _rctrl);
        }
    }
    return G_SOURCE_CONTINUE;
}


#endif

//////////////////////////////////////////////////
// 
// PROBE_UdpSrcBuffer()は最後に受信した時刻を更新
// 一定時間受信が無ければ、CALLBK_RxWatch()がOFFにする
// 
///////////////////////////////////////////////////
GstPadProbeReturn PROBE_UdpSrcBuffer(GstPad*, GstPadProbeInfo* info, gpointer user_data)
{
	int ch = 0; // 1ch想定

#ifdef _WIN32
    // ★追加：最後に受信した時刻を更新
    // ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
    // マルチチャンネル化の時に配列化すること
    g_last_rx_us.store(g_get_monotonic_time()); 
    // バッファが流れた＝受信できている
    NotifyRx(true, ch);
#endif

    return GST_PAD_PROBE_OK;
}

//////////////////////////////////////////////////
// 
// PROBE_UdpSrcBufferEx()は最後に受信した時刻を更新
// 一定時間受信が無ければ、CALLBK_RxWatchEx()がOFFにする
// user_dataはnullでなければ RTSPCtrl* としてキャストする
// 
///////////////////////////////////////////////////
GstPadProbeReturn PROBE_UdpSrcBufferEx(GstPad*, GstPadProbeInfo* info, gpointer user_data)
{
    int ch = 0; // 1ch想定
    if (user_data)
    {
        RTSPCtrl* _rctrl = static_cast<RTSPCtrl*>(user_data);
        //ch = _rctrl->ch;

#ifdef _WIN32
        // ★追加：最後に受信した時刻を更新
        // ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
        // マルチチャンネル化の時に配列化すること
        _rctrl->g_last_rx_us.store(g_get_monotonic_time());
        // バッファが流れた＝受信できている
        NotifyRxEx(true, _rctrl);
#endif

    }
    return GST_PAD_PROBE_OK;
}

// 非同期で安全にリスタート（メインループスレッドで実行）
gboolean CALLBK_RstPipeline(gpointer data) {
    GstElement* p = GST_ELEMENT(data);
    g_print("[auto-restart] restarting pipeline...\n");
    gst_element_set_state(p, GST_STATE_READY);
    gst_element_get_state(p, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    gst_element_set_state(p, GST_STATE_PLAYING);
    g_print("[auto-restart] done.\n");
    return FALSE; // 一回だけ
}

//////////////////////////////////////////////////////////
//異常があった時のコールバック関数
gboolean CALLBK_BusWatch(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    MediaCtx* ctx = static_cast<MediaCtx*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            g_printerr("Error: %s\n", err->message);
            g_error_free(err); g_free(dbg);
            // エラーでもリスタート
            g_idle_add(CALLBK_RstPipeline, ctx->pipeline);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream -> restart\n");
            g_idle_add(CALLBK_RstPipeline, ctx->pipeline);
            break;

        case GST_MESSAGE_ELEMENT: {
            // udpsrc の無信号タイムアウトも拾ってリスタート
            const GstStructure* s = gst_message_get_structure(msg);
            if (s && gst_structure_has_name(s, "GstUDPSrcTimeout")) {
                g_print("UDPSRC timeout -> restart\n");
#ifdef _WIN32
                NotifyRx(false, 0); // ★無信号 通知
#endif
                g_idle_add(CALLBK_RstPipeline, ctx->pipeline);
            }
            break;
        }

        default: break;
    }
    return TRUE;
}

gboolean CALLBK_BusWatchEx(GstBus* bus, GstMessage* msg, gpointer user_data)
{
	//int _ch = 0; // 1ch想定
	RTSPCtrl* _rctrl = static_cast<RTSPCtrl*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            g_printerr("Error: %s\n", err->message);
            g_error_free(err); g_free(dbg);
            // エラーでもリスタート
            g_idle_add(CALLBK_RstPipeline, _rctrl->ptctx->pipeline);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream -> restart\n");
            g_idle_add(CALLBK_RstPipeline, _rctrl->ptctx->pipeline);
            break;

        case GST_MESSAGE_ELEMENT: {
            // udpsrc の無信号タイムアウトも拾ってリスタート
            const GstStructure* s = gst_message_get_structure(msg);
            if (s && gst_structure_has_name(s, "GstUDPSrcTimeout")) {
                g_print("UDPSRC timeout -> restart\n");
#ifdef _WIN32
                NotifyRxEx(false, _rctrl); // ★無信号 通知
#endif
                g_idle_add(CALLBK_RstPipeline, _rctrl->ptctx->pipeline);
            }
            break;
        }

        default: break;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////
//
// CALLBK_MediaCfg
// パイプラインの取得
// さらに受信検出用の probe も付ける
// コールバック
//
// ここからさらに下記の関数をコールバック登録している
// CALLBK_RxWatch()
// PROBE_UdpSrcBuffer()
// CALLBK_BusWatch()
// 
//////////////////////////////////////////////////////////
// user_dataは GMainLoop*でキャストしているので、勝手に拡張できない
// 関数自体も引数の型や数を変えられない
// ★★★★★★★★★★★★★こっちは非拡張版★★★★★★★★★★★★★★★★★ 
void CALLBK_MediaCfg(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
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
                    ///////////////////////////////////////////////
                    // probe 登録
					// PROBE_UdpSrcBuffer()は最後に受信した時刻を更新
					// 一定時間受信が無ければOFFにする
                    ///////////////////////////////////////////////
                    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, PROBE_UdpSrcBuffer, nullptr, nullptr);
                    gst_object_unref(pad);
                    g_object_set_data(G_OBJECT(src), "rx-probe-installed", GINT_TO_POINTER(1));
                }
            }
            gst_object_unref(src);
        }
    }
#ifdef _WIN32
    /////////////////////////////////////////
    // 受信監視タイマ開始（まだなら開始）
	// CALLBK_RxWatch 内で g_rx を参照しているので
	// 二重登録は問題ないが念のためチェック
	///////////////////////////////////////////
    if (g_rx_watch_id == 0)
    {
		// Add watch timer
        // CALLBK_RxWatch
		g_rx_watch_id = g_timeout_add(GST_INTERVAL_PORTWATCH, 
            CALLBK_RxWatch,     //通常版
            nullptr);           //GST_INTERVAL_PORTWATCH = 500ms
    }
#endif

    // CALLBK_BusWatch が期待する MediaCtx* をここで作る
    MediaCtx* ctx = g_new0(MediaCtx, 1);
    ctx->loop = loop;
    ctx->pipeline = GST_ELEMENT(gst_object_ref(element)); // パイプライン参照を保持

    if (GstBus* bus = gst_element_get_bus(element)) {
        gst_bus_add_watch(bus, 
        CALLBK_BusWatch, ctx);  // ← MediaCtx* を渡す

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

/////////////////////////////////////////////////////////
//
// 拡張機能版 CALLBK_MediaCfgEx
// user_dataには RTSPCtrl* を渡されるようすること
// さらにコールバック登録時に RTSPCtrl* を渡す
//
// ここからさらに下記の関数をコールバック登録している
// CALLBK_RxWatchEx()
// PROBE_UdpSrcBufferEx()
// CALLBK_BusWatchEx()
// 
/////////////////////////////////////////////////////////
void CALLBK_MediaCfgEx(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data)
{
    // user_data は g_signal_connect で渡した GMainLoop* として受け取る
    //GMainLoop* loop = static_cast<GMainLoop*>(user_data);
	RTSPCtrl* _rctrl = static_cast<RTSPCtrl*>(user_data);

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
                    ///////////////////////////////////////////////
                    // probe 登録
                    // PROBE_UdpSrcBuffer()は最後に受信した時刻を更新
                    // 一定時間受信が無ければOFFにする
                    ///////////////////////////////////////////////
                    gst_pad_add_probe(pad, 
                        GST_PAD_PROBE_TYPE_BUFFER, 
                        PROBE_UdpSrcBufferEx, 
                        _rctrl,
                        nullptr);
                    gst_object_unref(pad);
                    g_object_set_data(G_OBJECT(src), "rx-probe-installed", GINT_TO_POINTER(1));
                }
            }
            gst_object_unref(src);
        }
    }
#ifdef _WIN32
    /////////////////////////////////////////
    // 受信監視タイマ開始（まだなら開始）
    // CALLBK_RxWatch 内で g_rx を参照しているので
    // 二重登録は問題ないが念のためチェック
    ///////////////////////////////////////////
    if (_rctrl->g_rx_watch_id == 0)
    {
        ///////////////////////////////////////////
        // Add watch timer
        // CALLBK_RxWatch
        ///////////////////////////////////////////
        _rctrl->g_rx_watch_id = g_timeout_add(
            GST_INTERVAL_PORTWATCH, 
            CALLBK_RxWatchEx, 
            _rctrl); //GST_INTERVAL_PORTWATCH = 500ms
    }
#endif

    // CALLBK_BusWatch が期待する MediaCtx* をここで作る
    _rctrl->ptctx = g_new0(MediaCtx, 1);
    _rctrl->ptctx->loop = _rctrl->ptLoop;
    _rctrl->ptctx->pipeline = GST_ELEMENT(gst_object_ref(element)); // パイプライン参照を保持

    if (GstBus* bus = gst_element_get_bus(element)) {
		gst_bus_add_watch(
            bus, 
            CALLBK_BusWatchEx, 
            _rctrl);  // ← RTSPCtrlを渡す
        gst_object_unref(bus);
    }
    gst_object_unref(element);

    // media 破棄と同時に ctx を解放（保持していた pipeline も unref）
    g_object_set_data_full(G_OBJECT(media), "media-ctx", _rctrl->ptctx, [](gpointer p) {
        MediaCtx* c = static_cast<MediaCtx*>(p);
        if (c->pipeline)
            gst_object_unref(c->pipeline);
        g_free(c);
        });
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
    //        gst_bus_add_watch(bus, CALLBK_BusWatch, loop);
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

            gst_bus_add_watch(bus, CALLBK_BusWatch, mctx);
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
int OpenRTSPServer(PTMLOOP& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[])
{
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

        //if (0)
        //{
        //    str_pipeline
        //        << "( udpsrc port=" << in_port
        //        << " caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" "
        //        << "! rtph264depay ! h264parse "
        //        << "! rtph264pay name=pay0 pt=96 )";
        //}
        //else
        //{
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
//        }

        sstr_channel << "/" << channel_name;

        static GOptionEntry entries[] = {
            {"port",         'p',  0, G_OPTION_ARG_STRING, &(tmp_port),  str_GOptionEntry_01.str().c_str(),               "PORT"},
            {"disable-rtcp", '\0', 0, G_OPTION_ARG_NONE,   &disable_rtcp,"Whether RTCP should be disabled (default false)", NULL},
            {NULL}
        };

        //ここのコードがないとチェックボックスのオンオフ働かない
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


        // factory 設定
        factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, str_pipeline.str().c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
		gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp); //HLS
        gst_rtsp_media_factory_set_suspend_mode(factory, GST_RTSP_SUSPEND_MODE_NONE);

		//////////////////////////////////////////
        // シグナルを接続
		// CALLBK_MediaCfg に GMainLoop* を渡す
        // いやー、ポインタの勉強になりますね。
        //////////////////////////////////////////
#if 0
		//_rctrlはコールバック内で使うのでstaticにする
		static RTSPCtrl _rctrl;
        _rctrl.ch = 0;
		_rctrl.out_port = out_port;
		_rctrl.ptLoop = loop;
		_rctrl.channel_name = channel_name;

        g_signal_connect(
            factory,
            "media-configure",
            G_CALLBACK(CALLBK_MediaCfgEx),
            &_rctrl /* ユーザーデータ */);
#else
        g_signal_connect(
            factory, 
            "media-configure",
            G_CALLBACK(CALLBK_MediaCfg),
            loop /* ユーザーデータ */);
#endif

		// ADD Factory
        gst_rtsp_mount_points_add_factory(mounts, sstr_channel.str().c_str(), factory);
        g_object_unref(mounts);

        // サーバ attach
        gst_rtsp_server_attach(server, NULL);

        //ダミーRTSPクライアントの起動
        StartManagedDummy(out_port, channel_name);

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
// 拡張版
///////////////////////////////////////////////////////////
int OpenRTSPServerEx(RTSPCtrl& _rctrl, int argc, char* argv[])
{
    GstRTSPServer* server;
    GstRTSPMountPoints* mounts;
    GstRTSPMediaFactory* factory;
    GOptionContext* optctx;
    GError* error = NULL;

    {
        std::lock_guard<std::mutex> lock(mtx); // ロック
        bool hls_on = false;

        //HLS用のディレクトリ作成
        std::string hls_dir = "C:/hls/" + _rctrl.channel_name + "-" + std::to_string(_rctrl.out_port);
        std::filesystem::create_directories(hls_dir);

        std::ostringstream str_GOptionEntry_01;
        str_GOptionEntry_01 << "Port to listen on (default: " << _rctrl.out_port << ")";

        std::string str_outport = std::to_string(_rctrl.out_port);
        const size_t buffer_size = 32;                          // バッファのサイズ
        static char tmp_port[buffer_size];                      // 書き込み可能なバッファ
        //strncpy(tmp_port, str_outport.c_str(), buffer_size - 1);
        strcpy_s(tmp_port, buffer_size, str_outport.c_str());//値コピー

        std::ostringstream str_pipeline;
        std::ostringstream sstr_channel;

        str_pipeline.str("");
        str_pipeline
            //<< "( udpsrc port=" << in_port            //2025/12/16
            << "( udpsrc name=src0 port=" << _rctrl.in_port    //2025/12/16
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
        //        }

        sstr_channel << "/" << _rctrl.channel_name;

        static GOptionEntry entries[] = {
            {"port",         'p',  0, G_OPTION_ARG_STRING, &(tmp_port),  str_GOptionEntry_01.str().c_str(),               "PORT"},
            {"disable-rtcp", '\0', 0, G_OPTION_ARG_NONE,   &disable_rtcp,"Whether RTCP should be disabled (default false)", NULL},
            {NULL}
        };

        //ここのコードがないとチェックボックスのオンオフ働かない
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

        //////////////////////////////////////////
        // 
        // 中継メインループのインスタンス作成
        // 
        //////////////////////////////////////////
        _rctrl.ptLoop = g_main_loop_new(NULL, FALSE);

        /* create a server instance */
        server = gst_rtsp_server_new();
        g_object_set(server, "service", str_outport.c_str(), NULL);
        mounts = gst_rtsp_server_get_mount_points(server);


        // factory 設定
        factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, str_pipeline.str().c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
        gst_rtsp_media_factory_set_enable_rtcp(factory, !disable_rtcp); //HLS
        gst_rtsp_media_factory_set_suspend_mode(factory, GST_RTSP_SUSPEND_MODE_NONE);

        //////////////////////////////////////////
        // シグナルを接続
        // CALLBK_MediaCfg に GMainLoop* を渡す
        // いやー、ポインタの勉強になりますね。
        //////////////////////////////////////////
//#if 1

        g_signal_connect(
            factory,
            "media-configure",
            G_CALLBACK(CALLBK_MediaCfgEx),
            &_rctrl /* ユーザーデータ */);

        // ADD Factory
        gst_rtsp_mount_points_add_factory(mounts, sstr_channel.str().c_str(), factory);
        g_object_unref(mounts);

        // サーバ attach
        gst_rtsp_server_attach(server, NULL);

        //ダミーRTSPクライアントの起動
        StartManagedDummy(_rctrl.out_port, _rctrl.channel_name);

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
        //HLSの準備完了表示
        if (hls_on)
        {
            std::cout << "Stream Ready at HLS:" << hls_dir.c_str() << std::endl;
        }
    }
    g_main_loop_run(_rctrl.ptLoop);

	// タイマ削除
    if (_rctrl.g_rx_watch_id != 0) {
        g_source_remove(_rctrl.g_rx_watch_id);
        _rctrl.g_rx_watch_id = 0;
    }
    return 0;
}

