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
// テスト コマンド
// RTSPServer.exe 5005 8555 sisi
// RTSPServer.exe 5004 8554 default 2

///////////////////////////////////////////////////////////////////
// テスト用ffmpeg例 USBカメラの映像をRTSPserverに送信する例
// 家のPC
// 標準画質
// ffmpeg -f dshow -i video="Logicool BRIO"  -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency  -f rtp rtp://127.0.0.1:5004
// HD画質
// ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5004
// ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="Logicool BRIO" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5005

// 135x "FHD Camera"
// ffmpeg -f dshow -framerate 30 -video_size 1920x1080 -i video="FHD Camera" -vcodec libx264 -pix_fmt yuv420p -preset ultrafast -tune zerolatency -f rtp rtp://127.0.0.1:5004


///////////////////////////////////////////////////////////////////
// GUI（IIS マネージャ）の手順
// 「IIS マネージャ」（inetmgr）を開く
// 左ペイン［接続］で サーバー名 > Sites > hls（あなたのサイト）をクリック
// 中央ペイン上部が ［機能ビュー］ になっていることを確認（［コンテンツビュー］ではない）
// アイコン一覧から ［MIME の種類］(アイコン) をダブルクリック
// 右側［操作］の ［追加…］ をクリックして、次の2件を追加
// 拡張子 : .m3u8 / MIME の種類 : application / vnd.apple.mpegurl
// 拡張子 : .ts / MIME の種類 : video / mp2t
// 変更は即時反映。必要ならサイト「hls」を 再起動（右側［操作］→［停止］→［開始］）
//
// MIEMEのアイコンが無い場合
// コントロールパネル「プログラムと機能」「Windowsの機能」
// IIS-WEB管理ツール-IIS管理サービス IIS管理スクリプトおよびツール を入れる
// 
///////////////////////////////////////////////////////////////////
// C:\hls\web.config を作成（または追記）すると同じ効果が出ます：
// <?xml version="1.0" encoding="utf-8"?>
// <configuration>
// <system.webServer>
// <staticContent>
// <mimeMap fileExtension = ".m3u8" mimeType = "application/vnd.apple.mpegurl" />
// <mimeMap fileExtension = ".ts"   mimeType = "video/mp2t" />
// </staticContent>
// </system.webServer>
// </configuration>
// 
// 成功
// http://140.81.145.4:8080/
///////////////////////////////////////////////////////////////////
// テスト用FFplay例
//ffplay rtsp://127.0.0.1:8554/default
//ffplay rtsp://127.0.0.1:8554/test
//ffplay rtsp://127.0.0.1:8555/sisi

//テスト用HLSURL
// http://127.0.0.1:8080/default.htm

/* 受信用　html

<video id="v" controls autoplay muted playsinline></video>
<script src="https://cdn.jsdelivr.net/npm/hls.js@latest"></script>
<script>
  const url = '/default-8554/index.m3u8';
  const video = document.getElementById('v');

  if (Hls.isSupported()) {
    const hls = new Hls({
      // ライブ用のざっくりした設定例（必要に応じて調整）
      liveSyncDurationCount: 3,        // 先頭から 3 セグメント後ろくらいを再生位置に
      liveMaxLatencyDurationCount: 10, // 最大で 10 セグメント遅れまで許容
      maxBufferLength: 30,             // 30秒分までバッファ
      maxMaxBufferLength: 60
    });

    hls.loadSource(url);
    hls.attachMedia(video);

    // マニフェスト解析後に play() を叩いておく
    hls.on(Hls.Events.MANIFEST_PARSED, function () {
      video.play().catch(err => {
        console.log('Autoplay blocked:', err);
      });
    });

    // デバッグ用：エラー内容をコンソールに出す
    hls.on(Hls.Events.ERROR, function (event, data) {
      console.log('HLS error:', data);
      if (data.fatal) {
        switch (data.type) {
          case Hls.ErrorTypes.NETWORK_ERROR:
            hls.startLoad();
            break;
          case Hls.ErrorTypes.MEDIA_ERROR:
            hls.recoverMediaError();
            break;
          default:
            hls.destroy();
            break;
        }
      }
    });
  } else if (video.canPlayType('application/vnd.apple.mpegurl')) {
    // Safari のネイティブ HLS
    video.src = url;
    video.addEventListener('loadedmetadata', () => video.play());
  } else {
    video.src = url; // 最低限のフォールバック
  }
</script>


*/

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

#include "RTSPServer.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gstrtsp-1.0.lib")      // または "libgstrtsp-1.0.lib"
#pragma comment(lib, "gstrtspserver-1.0.lib") // すでに入っていれば不要

//#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_DISABLE_RTCP FALSE

//static char* port = (char*)DEFAULT_RTSP_PORT;           
extern gboolean disable_rtcp = DEFAULT_DISABLE_RTCP;







//windpws serverで勝手に停止したので
//開発環境では上手くいくのだけど?
int main(int argc, char* argv[])
//int main(int argc, char* argv[])
{
    int in_port;
    int out_port;
    std::string str_channel;
    int number_ob_channels = 1;

    //引数解析　
    // Gstreamer 標準のg_option_context_parseも機能はそのままとってあるが・・・ outポートを変える機能は標準には無さそうなので
    std::cout << std::filesystem::path(__FILE__).filename() << " BUILD:[" << __DATE__ << " " << __TIME__ << "]" << std::endl;
    if (argc == 1)
    {
        std::cout << "Usage : " << argv[0] << " [in_port] [out_port] [ch_name] [number of channels(defaul=1)]" << std::endl;
        std::cout << argv[0] << " : use default parametors" << std::endl;

        in_port = 5004;
        out_port = 8554;
        str_channel = "default";
    }
    else if (argc == 4)
    {
        //std::cout << argv[0] << " : use parametors" << std::endl;
        std::cout << "RTSPSever.exe" << " : use parametors" << std::endl;

        in_port = std::stoi(argv[1]);
        out_port = std::stoi(argv[2]);
        str_channel = argv[3];
        number_ob_channels = 1;
    }
    else if (argc == 5)
    {
        //std::cout << argv[0] << " : use parametors" << std::endl;
        std::cout << "RTSPSever.exe" << " : use parametors" << std::endl;

        in_port = std::stoi(argv[1]);
        out_port = std::stoi(argv[2]);
        str_channel = argv[3];
        number_ob_channels = std::stoi(argv[4]);
    }
    else
    {
        std::cout << "Usage : " << argv[0] << " [in_port] [out_port] [ch_name] [number of channels(defaul=1)]" << std::endl
            << "Example : " << argv[0] << " 5004 8554 ch01" << std::endl;
        std::cout << std::filesystem::path(__FILE__).filename() << " BUILD:[" << __DATE__ << " " << __TIME__ << "]" << std::endl;
        return -1;
    }

    std::vector<std::thread> threads;
    std::vector<GMainLoop*> _loops(number_ob_channels, nullptr); // GMainLoop ポインタのベクター

    //std::vector<GMainLoop*> _loops;
    //_loops.reserve(number_ob_channels);

    //GMainLoop* _loop = nullptr; //終了コマンドをスローするのに必要

    for (int i = 0; i < number_ob_channels; i++)
    {
        std::cout << "[in_port]  : " << in_port + i << std::endl
            << "[out_port] : " << out_port + i << std::endl
            << "[ch_name]  : " << str_channel << std::endl
            << "[number of channels]  : " << number_ob_channels << std::endl
            << "------------------------------" << std::endl
            << "Push Q,E,X,C or ESC for Stop" << std::endl;

        // スレッドを起動し、スレッド管理ベクターに追加
        //threads.emplace_back(OpenHLSServer, std::ref(_loops[i]), in_port + i, out_port + i, std::ref(str_channel), argc, argv);
        threads.emplace_back(OpenRTSPServer, std::ref(_loops[i]), in_port + i, out_port + i, std::ref(str_channel), argc, argv);
    }
    //std::thread mainLoopThread([&]() {OpenRTSPServer(_loop, in_port, out_port, str_channel, argc, argv); });

    while (true)
    {
        char c = std::cin.get();
        //int c = _getch();
        if (
            c == 'q' || c == 'Q' ||
            c == 'e' || c == 'E' ||
            c == 'x' || c == 'X' ||
            c == 'c' || c == 'C' ||
            c == 27)
        { // 'q' または 'ESC'キー
            std::cout << "Stop requested, quitting main loop..." << std::endl;
            for (int i = 0; i < number_ob_channels; i++)
                g_main_loop_quit(_loops[i]);
            break;
        }
    }
    // メインループ終了を待つ
    // 全てのスレッドを join
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    for (int i = 0; i < number_ob_channels; i++)
        g_main_loop_unref(_loops[i]);

    return 0;
}

/*
//int main(int argc, char* argv[])
int main_single(int argc, char* argv[])
{
    int in_port;
    int out_port;
    std::string str_channel;

    //引数解析　
    // Gstreamer 標準のg_option_context_parseも機能はそのままとってあるが・・・ outポートを変える機能は標準には無さそうなので
    if (argc == 1)
    {
        std::cout << argv[0] << " : use default parametors" << std::endl;

        in_port = 5004;
        out_port = 8554;
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
            << "Example : " << argv[0] << " 5004 8554 default" << std::endl;
        return -1;
    }

    std::cout << "[in_port]  : " << in_port << std::endl
        << "[out_port] : " << out_port << std::endl
        << "[ch_name]  : " << str_channel << std::endl
        << "------------------------------" << std::endl
        << "Push Q,E,X,C or ESC for Stop" << std::endl;

    GMainLoop* _loop = nullptr; //終了コマンドをスローするのに必要

    std::thread mainLoopThread([&]() {OpenRTSPServer(_loop, in_port, out_port, str_channel, argc, argv); });

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

*/