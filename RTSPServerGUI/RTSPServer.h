#pragma once

#define _WIN32_WINNT 0x0600  // inet_ntop を利用するために必要な場合がある(Windows Vista以降)
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

#define USE_DUMMY_CLIENT

////////////////////////////////////////////////////////////
//// GST設定構造体
//struct GST_RTSP_SVPARAMS
//{
//    GMainLoop* loop = nullptr;
//
//    int in_port = 0;
//    int	out_port = 0;
//    std::string channel_name;
//
//public:
//    GST_RTSP_SVPARAMS()
//    {
//    }
//
//    ~GST_RTSP_SVPARAMS()
//    {
//	}
//};

//////////////////////////////////////////////////////////
// 
// OpenRTSPServer()
//   　　┃
//   コールバック登録
//   media_configure_cb()
//　　   ┣━━━━━━━━━━━━━━┓
//   タイマとして登録     　   Pad Probe コールバック
// 　受信有無に関係ない            イベントコール
// 　  rx_watch_cb()　　        udpsrc_buffer_probe()
//   　  ┃                            ┃
//   ファンクションコール        ファンクションコール 
// 　NotifyRx(false, ch)       　NotifyRx(true, ch)
//     　┃                            ┃
//  PostMessageWでflase通知     PostMessageWでtrue通知
// 
//////////////////////////////////////////////////////////

/* -----------------------------------------------------------
 呼び出し関係図

 OpenRTSPServer()
 ┃
 ┣ media_configure_cb() コールバック登録
 ┃ ┃
 ┃ ┣ rx_watch_cb() タイマ登録
 ┃ ┃ ┃
 ┃ ┃ ┗ NotifyRx(false, ch) ファンクションコール
 ┃ ┃     ┃
 ┃ ┃     ┗PostMessageW() false通知
 ┃ ┃
 ┃ ┣ udpsrc_buffer_probe() Pad Probe コールバック バッファ通過イベントで発火
 ┃ ┃ ┃
 ┃ ┃ ┗ NotifyRx(true, ch) ファンクションコール
 ┃ ┃     ┃
 ┃ ┃     ┗PostMessageW() true通知
 ┃ ┃
 ┃ ┣ bus_watch_callback( ) コールバック登録 MediaCtx* ctx を渡されている
 ┃ ┃ ┃
 ┃ ┃ ┣ restart_pipeline_idle( ) タイマ登録 MediaCtx*のctx->pipeline を 渡されている。
 ┃ ┃ ┃
 ┃ ┃ ┗ NotifyRx(true, ch) ファンクションコール
 ┃ ┃     ┃
 ┃ ┃     ┗PostMessageW() false通知

-------------------------------------------------------------- */  


//////////////////////////////////////////////////////////
// メディアごとのコンテキスト
struct MediaCtx {
    GMainLoop* loop;
    GstElement* pipeline; // gst_rtsp_media_get_element() の返り値
};

struct ServerCtx {
    GMainLoop* loop;
    GstRTSPServer* server;
};

// 非同期で安全にリスタート（メインループスレッドで実行）
static gboolean restart_pipeline_idle(gpointer data);
//{
//    GstElement* p = GST_ELEMENT(data);
//    g_print("[auto-restart] restarting pipeline...\n");
//    gst_element_set_state(p, GST_STATE_READY);
//    gst_element_get_state(p, nullptr, nullptr, GST_CLOCK_TIME_NONE);
//    gst_element_set_state(p, GST_STATE_PLAYING);
//    g_print("[auto-restart] done.\n");
//    return FALSE; // 一回だけ
//}

int OpenRTSPServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]);

#define DEFAULT_DISABLE_RTCP FALSE
#define GST_INTERVAL_PORTWATCH 500 // ms

#ifdef _WIN32
#include <Windows.h>
constexpr UINT WM_APP_RX_STATUS = WM_APP + 102; // wParam: 1=受信中, 0=無信号
void SetGuiNotifyHwnd(HWND hwnd);
#endif