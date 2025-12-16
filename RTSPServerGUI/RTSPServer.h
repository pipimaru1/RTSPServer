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
static gboolean restart_pipeline_idle(gpointer data) {
    GstElement* p = GST_ELEMENT(data);
    g_print("[auto-restart] restarting pipeline...\n");
    gst_element_set_state(p, GST_STATE_READY);
    gst_element_get_state(p, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    gst_element_set_state(p, GST_STATE_PLAYING);
    g_print("[auto-restart] done.\n");
    return FALSE; // 一回だけ
}

int OpenRTSPServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]);
int OpenHLSServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]);

//static char* port = (char*)DEFAULT_RTSP_PORT;           
#define DEFAULT_DISABLE_RTCP FALSE

//extern gboolean disable_rtcp;
//extern gboolean disable_rtcp = DEFAULT_DISABLE_RTCP;

#ifdef _WIN32
#include <Windows.h>
constexpr UINT WM_APP_RX_STATUS = WM_APP + 102; // wParam: 1=受信中, 0=無信号
void SetGuiNotifyHwnd(HWND hwnd);
#endif