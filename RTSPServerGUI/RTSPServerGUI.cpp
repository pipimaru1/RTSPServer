// RTSPServerGUI.cpp : アプリケーションのエントリ ポイントを定義します。

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
// テスト用FFplay例
//ffplay rtsp://127.0.0.1:8554/default
//ffplay rtsp://127.0.0.1:8554/test
//ffplay rtsp://127.0.0.1:8555/sisi


#include "stdafx.h"
#include "framework.h"
#include "RTSPServer.h"
#include "RTSPServerGUI.h"

#define MAX_LOADSTRING 100

RtspServerController g_server;
// 通知用（Controller 側と同じ値にしてください）
constexpr UINT WM_APP_SERVER_EXITED = WM_APP + 101;


// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト

// このコード モジュールに含まれる関数の宣言を転送します:
INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    hInst = hInstance;

    // タイトル文字列（必要ならダイアログの SetWindowText に使う）
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

    // ここが「ダイアログボックスアプリ」の本体
    // IDD_MAIN はリソースに存在している前提
    INT_PTR ret = DialogBoxParamW(hInstance,
        MAKEINTRESOURCEW(IDD_MAIN),
        nullptr,
        MainDlgProc,
        0);

    // DialogBox が失敗した場合（デバッグ用）
    // ret == -1 のとき GetLastError() で原因確認
    return (int)ret;
}

INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {

            LoadSettings(hDlg);

            // チェックは表示専用にする想定（必要なら外してください）
            //EnableWindow(GetDlgItem(hDlg, IDC_CHK01), FALSE);

            SetRunningUi(hDlg, false);

            // タイトルを設定（不要なら消してOK）
            //SetWindowTextW(hDlg, szTitle);
            return (INT_PTR)TRUE;
        }break;

        case WM_APP_RX_STATUS:
        {
            const bool receiving = (wParam != 0);
            CheckDlgButton(hDlg, IDC_CHK01, receiving ? BST_CHECKED : BST_UNCHECKED);
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);

            switch (id)
            {
                case    IDC_BTN_START01:
                {
                    int inPort = 0, outPort = 0;
                    if (!GetIntFromEdit(hDlg, IDC_EDIT_PORTIN01, inPort) ||
                        !GetIntFromEdit(hDlg, IDC_EDIT_PORTOUT01, outPort) ||
                        inPort < 1 || inPort > 65535 ||
                        outPort < 1 || outPort > 65535)
                    {
                        MessageBoxW(hDlg, L"ポート番号が不正です。(1～65535)", L"入力エラー", MB_ICONERROR);
                        return (INT_PTR)TRUE;
                    }

                    wchar_t nameW[256]{};
                    GetDlgItemTextW(hDlg, IDC_EDIT_PORTNAME01, nameW, 256);
                    std::wstring channelW = nameW;
                    if (channelW.empty())
                    {
                        MessageBoxW(hDlg, L"ストリーム名（Channel）が空です。", L"入力エラー", MB_ICONERROR);
                        return (INT_PTR)TRUE;
                    }

                    // ★「Start handler の直前」＝ g_server.Start() を呼ぶ直前はココ
                    SetGuiNotifyHwnd(hDlg);     // ★通知先を登録
                    CheckDlgButton(hDlg, IDC_CHK01, BST_UNCHECKED); // ★開始直後はOFF（まだ受信してない）

                    // 起動前に設定保存（再起動時にそのまま）
                    SaveSettings(hDlg);

                    std::string channelUtf8 = WideToUtf8(channelW);

                    if (!g_server.Start(inPort, outPort, channelUtf8, hDlg))
                    {
                        MessageBoxW(hDlg, L"既に起動中です。", L"情報", MB_OK | MB_ICONINFORMATION);
                        return (INT_PTR)TRUE;
                    }

                    SetRunningUi(hDlg, true);
                    return (INT_PTR)TRUE;
                }
                case IDC_BTN_STOP01:
                {
                    g_server.Stop();
                    SetRunningUi(hDlg, false);
                    CheckDlgButton(hDlg, IDC_CHK01, BST_UNCHECKED);
                    return (INT_PTR)TRUE;
                }
                case IDOK:
                case IDCANCEL:
                {
                    // 終了時：稼働中なら止める → 設定保存 → ダイアログ終了
                    if (g_server.IsRunning())
                        g_server.Stop();

                    SaveSettings(hDlg);
                    SetGuiNotifyHwnd(nullptr);
                    EndDialog(hDlg, id);
                    return (INT_PTR)TRUE;
                }break;
            }
        }break;

        case WM_CLOSE:
        {
            if (g_server.IsRunning())
                g_server.Stop();

            SaveSettings(hDlg);
            SetGuiNotifyHwnd(nullptr);
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
		}break;
    }

    return (INT_PTR)FALSE;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
