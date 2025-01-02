// RTSPServer.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

///////////////////////////////////////////////////////////////////
// usage
// RTCPServer.exe "( udpsrc port=5004 caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" ! rtph264depay ! h264parse ! rtph264pay name=pay0 pt=96 )"
// 各要素の意味
// udpsrc port = 5004
// UDP ソケットを開き、ポート 5004 で受信。
// ここに RTP パケットを送ってもらう設定。
// 例 : ffmpeg - f rtp rtp ://127.0.0.1:5004 などから送信してやる。
// 
// caps = "application/x-rtp,media=video,encoding-name=H264,payload=96"
// caps(ケイパビリティ)   : 受信するストリームの種類やコーデック等を指定する。
// application / x - rtp  : RTP パケットであることを示す。
// media = video          : 映像媒体であることを示す。
// encoding - name = H264 : H.264 形式でエンコードされている。
// payload = 96           : RTP のペイロードタイプ番号。通常、動的な H.264 ストリームでは 96〜127 が使われることが多い。
// 
// rtph264depay
// RTP ペイロード（H.264）をデパケット化するプラグイン。
//「RTP ペイロード → H.264 のバイトストリーム」 へ変換。
// 
// h264parse
// H.264 ビットストリームをパース（解析）・再構築する要素。
// SPS / PPS の挿入・抜き出しや、ストリームのフレーム境界の明示などを行う。
// 
// rtph264pay name = pay0 pt = 96
// H.264 を RTSP 用に再ペイロード化する。
// name = pay0   : RTSP サーバーで pay0 という名前のペイロード要素を作り、配信ストリームとして登録。
// pt = 96       : RTP のペイロードタイプを 96 で運用するよう指定。
// これが test - launch 経由で RTSP のストリームにも反映される。

///////////////////////////////////////////////////////////////////
// テスト用
// RTSPServer.exe 5005 8555 sisi

///////////////////////////////////////////////////////////////////
// テスト用ffmpeg例
// 標準画質
// ffmpeg -f dshow -i video="Logicool BRIO"  -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency  -f rtp rtp://127.0.0.1:5004
// HD画質
// ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5004
// ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5005

///////////////////////////////////////////////////////////////////
// テスト用FFplay例
//ffplay rtsp://127.0.0.1:8554/test
//ffplay rtsp://127.0.0.1:8555/sisi

#define _WIN32_WINNT 0x0600  // inet_ntop を利用するために必要な場合がある(Windows Vista以降)
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <conio.h>  // getch() / _getch()
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <vector>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#pragma comment(lib, "ws2_32.lib")

//#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_DISABLE_RTCP FALSE

//static char* port = (char*)DEFAULT_RTSP_PORT;           
static gboolean disable_rtcp = DEFAULT_DISABLE_RTCP;    

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
//異常があった時のコールバック関数
static gboolean bus_watch_callback(GstBus* bus, GstMessage* msg, gpointer user_data) 
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
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
// パイプラインの取得
static void media_configure_cb(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data) 
{
    // RTSP が内部生成したメディア(=パイプライン)を取得
    GstElement* element = gst_rtsp_media_get_element(media);
    if (!element) return;

    // バスを取得
    GstBus* bus = gst_element_get_bus(element);
    if (bus) {
        // bus_watch_callback() を登録してメインループで監視
        gst_bus_add_watch(bus, bus_watch_callback, user_data);
        gst_object_unref(bus);
    }
}

//////////////////////////////////////////////////////////
int OpenRTSPServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]) 
{
 //   GMainLoop* loop;
    GstRTSPServer* server;
    GstRTSPMountPoints* mounts;
    GstRTSPMediaFactory* factory;
    GOptionContext* optctx;
    GError* error = NULL;

    std::ostringstream str_GOptionEntry_01;
    str_GOptionEntry_01 << "Port to listen on (default: " << out_port << ")";
    
    std::string str_outport=std::to_string(out_port);
    const size_t buffer_size = 32;                          // バッファのサイズ
    static char tmp_port[buffer_size];                      // 書き込み可能なバッファ
    //strncpy(tmp_port, str_outport.c_str(), buffer_size - 1);
    strcpy_s(tmp_port, buffer_size, str_outport.c_str());//値コピー

    std::ostringstream str_pipeline;
    std::ostringstream sstr_channel;

    str_pipeline << "( udpsrc port=" << in_port << " caps=\"application/x-rtp,media=video,encoding-name=H264,payload=96\" ! rtph264depay ! h264parse ! rtph264pay name=pay0 pt=96 )";
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
    std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;

    // 結果の表示
    if (ipList.empty()) {
        std::cout << "Can not find other ip address." << std::endl;
        //std::cout << "Stream Ready at rtsp://127.0.0.1:" << str_outport << sstr_channel.str() << std::endl;
    }
    else {
        //std::cout << "取得したIPアドレス一覧:" << std::endl;
        for (const auto& ip : ipList) {
            std::cout << "Stream Ready at rtsp://"<<ip<<":" << str_outport << sstr_channel.str() << std::endl;
        }
    }
    g_main_loop_run(loop);
    //g_main_loop_quit(loop);
    //g_main_loop_unref(loop);
    //gst_element_set_state(pipeline, GST_STATE_NULL);
    //gst_object_unref(pipeline);
    //delete tmp_port;
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
    //g_main_loop_quit(loop);
    //g_main_loop_unref(loop);
    //gst_element_set_state(pipeline, GST_STATE_NULL);
    //gst_object_unref(pipeline);
    //delete tmp_port;
    return 0;
}


int main(int argc, char* argv[])
{
    int in_port;
    int out_port;
    std::string str_channel;

    //引数解析　
    // Gstreamer 標準のg_option_context_parseも機能はそのままとってあるが・・・ outポートを変える機能は標準には無さそうなので
    if (argc == 1)
    {
        std::cout << argv[0] << " : use default parametors" << std::endl;

        in_port     = 5004;
        out_port    = 8554;
        str_channel = "default";
    }
    else if (argc == 4)
    {
        std::cout << argv[0] << " : use parametors" << std::endl;

        in_port = std::stoi(argv[1]);
        out_port = std::stoi(argv[2]);
        str_channel = argv[3];
    }
    else
    {
        std::cout << "Usage : " << argv[0] << " [in_port] [out_port] [ch_name]" << std::endl
            << "Example : " << argv[0] << " 5004 8554 ch01" << std::endl;
        return -1;
    }

    std::cout << "[in_port]  : " << in_port << std::endl
        << "[out_port] : " << out_port << std::endl
        << "[ch_name]  : " << str_channel << std::endl
        << "------------------------------" << std::endl
        << "Push Q,E,X,C or ESC for Stop" << std::endl;

    GMainLoop* _loop=nullptr; //終了コマンドをスローするのに必要

    std::thread mainLoopThread([&]() {OpenRTSPServer(_loop, in_port, out_port, str_channel, argc, argv);});
 
    while (true) 
    {
        //char c = std::cin.get();
        int c = _getch();
        if (
            c == 'q' || c == 'Q' || 
            c == 'e' || c == 'E' ||
            c == 'x' || c == 'X' ||
            c == 'c' || c == 'C' ||
            c == 27)
        { // 'q' または 'ESC'キー
            std::cout << "Stop requested, quitting main loop..." << std::endl;
            g_main_loop_quit(_loop);
            break;
        }
    }
    // メインループ終了を待つ
    mainLoopThread.join();
    g_main_loop_unref(_loop);

    return 0;
}
