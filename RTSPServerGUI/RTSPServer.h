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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
//  OpenRTSPServer()
//  OpenRTSPServerEx()
//   　　┃
//   コールバック登録
// RTSP MediaFactoryシグナル監視   
// CALLBK_MediaCfg()
// CALLBK_MediaCfgEx()
//　　   ┣━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
//   タイマとして登録     　   　         Pad Probe コールバック          コールバック登録 
//  最終受信からの経過時間チェック　　　 バッファ通過イベント              bus メッセージ監視
// 　  CALLBK_RxWatch()　　               PROBE_UdpSrcBuffer()             CALLBK_BusWatch()
//     CALLBK_RxWatchEx()                 PROBE_UdpSrcBufferEx()           CALLBK_BusWatchEx()
//   　  ┃                                      ┃                              ┣━━━━━━━━━━━━┓ 
//   ファンクションコール                  ファンクションコール             コールバック            ファンクションコール  
// 　メインGUIに通知　  　　　　　　　　　 メインGUIに通知　　　　　　　パイプラインをリスタート   メインGUIに通知　  　　
// 　NotifyRx(false, ch)                 　NotifyRx(true, ch)           CALLBK_RstPipeline()       NotifyRx(false, ch)   
// 　NotifyRxEX(false, ch)               　NotifyRxEX(true, ch)         CALLBK_RstPipeline()       NotifyRxEX(false, ch)   
//     　┃                                      ┃                 　                               　    ┃                
//  PostMessageWでflase通知               PostMessageWでtrue通知                                  PostMessageWでtrue通知
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////
// 
// メディアごとのコンテキスト
// 
//////////////////////////////////////////////////////////
struct MediaCtx {
    GMainLoop* loop;
    GstElement* pipeline; // gst_rtsp_media_get_element() の返り値
};

/////////////////////////////////////////////////////////
//
// RTSPサーバーの制御クラス
//
/////////////////////////////////////////////////////////
typedef GMainLoop* PTMLOOP;   // PTMLOOP は「GMainLoop*」の別名

class RTSPCtrl
{
public:
	int in_port = 0;
	int	out_port = 0;
	std::string channel_name; //"default"など
	int ch = 0; // 0始まりのチャンネル番号

	PTMLOOP	ptLoop = nullptr;
	MediaCtx* ptctx = nullptr;
	std::atomic<bool> g_rx{ false };	// 受信中かどうか
	std::atomic<gint64> g_last_rx_us{ 0 };              // ★追加：最後に受信した時刻（μs）
	//   const gint64 g_rx_timeout_us = 5 * G_USEC_PER_SEC;  // ★追加：受信が途切れたとみなす時間（udpsrc timeout と同じ 5秒推奨）
	guint g_rx_watch_id = 0;                            // ★追加：監視タイマID（1ch想定）
};

// 非同期で安全にリスタート（メインループスレッドで実行）
static gboolean CALLBK_RstPipeline(gpointer data);

//int OpenRTSPServer(GMainLoop*& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]);
int OpenRTSPServer(PTMLOOP& loop, int in_port, int out_port, std::string& channel_name, int argc, char* argv[]);
int OpenRTSPServerEx(RTSPCtrl& _rctrl, int argc, char* argv[]);

#define DEFAULT_DISABLE_RTCP FALSE
#define GST_INTERVAL_PORTWATCH 500 // ms

#ifdef _WIN32
#include <Windows.h>
constexpr UINT WM_APP_RX_STATUS = WM_APP + 102; // wParam: 1=受信中, 0=無信号
void SetGuiNotifyHwnd(HWND hwnd);
#endif

