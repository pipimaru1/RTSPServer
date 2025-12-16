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

constexpr std::array<std::wstring_view, 32> SERVERNAME = {
    L"CH01",L"CH02",L"CH03",L"CH04",L"CH05",L"CH06",L"CH07",L"CH08",
    L"CH09",L"CH10",L"CH11",L"CH12",L"CH13",L"CH14",L"CH15",L"CH16",
    L"CH17",L"CH18",L"CH19",L"CH20",L"CH21",L"CH22",L"CH23",L"CH24",
    L"CH25",L"CH26",L"CH27",L"CH28",L"CH29",L"CH30",L"CH31",L"CH32"
};

constexpr std::array<UINT, 32> __IDC_EDIT_PORTIN = {
    IDC_EDIT_PORTIN1,IDC_EDIT_PORTIN2,IDC_EDIT_PORTIN3,IDC_EDIT_PORTIN4,
    IDC_EDIT_PORTIN5,IDC_EDIT_PORTIN6,IDC_EDIT_PORTIN7,IDC_EDIT_PORTIN8,
    IDC_EDIT_PORTIN9,IDC_EDIT_PORTIN10,IDC_EDIT_PORTIN11,IDC_EDIT_PORTIN12,
    IDC_EDIT_PORTIN13,IDC_EDIT_PORTIN14,IDC_EDIT_PORTIN15,IDC_EDIT_PORTIN16,
    IDC_EDIT_PORTIN17,IDC_EDIT_PORTIN18,IDC_EDIT_PORTIN19,IDC_EDIT_PORTIN20,
    IDC_EDIT_PORTIN21,IDC_EDIT_PORTIN22,IDC_EDIT_PORTIN23,IDC_EDIT_PORTIN24,
    IDC_EDIT_PORTIN25,IDC_EDIT_PORTIN26,IDC_EDIT_PORTIN27,IDC_EDIT_PORTIN28,
    IDC_EDIT_PORTIN29,IDC_EDIT_PORTIN30,IDC_EDIT_PORTIN31,IDC_EDIT_PORTIN32
};

constexpr std::array<UINT, 32> __IDC_EDIT_PORTOUT = {
    IDC_EDIT_PORTOUT1,IDC_EDIT_PORTOUT2,IDC_EDIT_PORTOUT3,IDC_EDIT_PORTOUT4,
    IDC_EDIT_PORTOUT5,IDC_EDIT_PORTOUT6,IDC_EDIT_PORTOUT7,IDC_EDIT_PORTOUT8,
    IDC_EDIT_PORTOUT9,IDC_EDIT_PORTOUT10,IDC_EDIT_PORTOUT11,IDC_EDIT_PORTOUT12,
    IDC_EDIT_PORTOUT13,IDC_EDIT_PORTOUT14,IDC_EDIT_PORTOUT15,IDC_EDIT_PORTOUT16,
    IDC_EDIT_PORTOUT17,IDC_EDIT_PORTOUT18,IDC_EDIT_PORTOUT19,IDC_EDIT_PORTOUT20,
    IDC_EDIT_PORTOUT21,IDC_EDIT_PORTOUT22,IDC_EDIT_PORTOUT23,IDC_EDIT_PORTOUT24,
    IDC_EDIT_PORTOUT25,IDC_EDIT_PORTOUT26,IDC_EDIT_PORTOUT27,IDC_EDIT_PORTOUT28,
    IDC_EDIT_PORTOUT29,IDC_EDIT_PORTOUT30,IDC_EDIT_PORTOUT31,IDC_EDIT_PORTOUT32
};

constexpr std::array<UINT, 32> __IDC_EDIT_PORTNAME = {
    IDC_EDIT_PORTNAME1,IDC_EDIT_PORTNAME2,IDC_EDIT_PORTNAME3,IDC_EDIT_PORTNAME4,
    IDC_EDIT_PORTNAME5,IDC_EDIT_PORTNAME6,IDC_EDIT_PORTNAME7,IDC_EDIT_PORTNAME8,
    IDC_EDIT_PORTNAME9,IDC_EDIT_PORTNAME10,IDC_EDIT_PORTNAME11,IDC_EDIT_PORTNAME12,
    IDC_EDIT_PORTNAME13,IDC_EDIT_PORTNAME14,IDC_EDIT_PORTNAME15,IDC_EDIT_PORTNAME16,
    IDC_EDIT_PORTNAME17,IDC_EDIT_PORTNAME18,IDC_EDIT_PORTNAME19,IDC_EDIT_PORTNAME20,
    IDC_EDIT_PORTNAME21,IDC_EDIT_PORTNAME22,IDC_EDIT_PORTNAME23,IDC_EDIT_PORTNAME24,
    IDC_EDIT_PORTNAME25,IDC_EDIT_PORTNAME26,IDC_EDIT_PORTNAME27,IDC_EDIT_PORTNAME28,
    IDC_EDIT_PORTNAME29,IDC_EDIT_PORTNAME30,IDC_EDIT_PORTNAME31,IDC_EDIT_PORTNAME32
};



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

bool IDC_BTN_START(
    HWND _hDlg, 
    UINT _IDC_BTN_START,
    UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
	UINT _IDC_EDIT_PORTNAME,
	UINT _IDC_CHK
)
{
    int inPort = 0, outPort = 0;
    if (!GetIntFromEdit(_hDlg, _IDC_EDIT_PORTIN, inPort) ||
        !GetIntFromEdit(_hDlg, _IDC_EDIT_PORTOUT, outPort) ||
        inPort < 1 || inPort > 65535 ||
        outPort < 1 || outPort > 65535)
    {
        MessageBoxW(_hDlg, L"ポート番号が不正です。(1～65535)", L"入力エラー", MB_ICONERROR);
        return true;
    }

    wchar_t nameW[256]{};
    GetDlgItemTextW(_hDlg, _IDC_EDIT_PORTNAME, nameW, 256);
    std::wstring channelW = nameW;
    if (channelW.empty())
    {
        MessageBoxW(_hDlg, L"ストリーム名（Channel）が空です。", L"入力エラー", MB_ICONERROR);
        return true;
    }

    // ★「Start handler の直前」＝ g_server.Start() を呼ぶ直前はココ
    SetGuiNotifyHwnd(_hDlg);     // ★通知先を登録
    CheckDlgButton(_hDlg, _IDC_CHK, BST_UNCHECKED); // ★開始直後はOFF（まだ受信してない）

    // 起動前に設定保存（再起動時にそのまま）
	SaveSettings(_hDlg, _IDC_EDIT_PORTIN, _IDC_EDIT_PORTOUT, _IDC_EDIT_PORTNAME);

    std::string channelUtf8 = WideToUtf8(channelW);

    if (!g_server.Start(inPort, outPort, channelUtf8, _hDlg))
    {
        MessageBoxW(_hDlg, L"既に起動中です。", L"情報", MB_OK | MB_ICONINFORMATION);
        return true;
    }

    SetRunningUi(_hDlg, true ,
        _IDC_BTN_START,
        _IDC_BTN_STOP,
        _IDC_EDIT_PORTIN,
        _IDC_EDIT_PORTOUT,
		_IDC_EDIT_PORTNAME
    );
    return true;
}

void BTN_STOP(
    HWND _hDlg,
    UINT _IDC_BTN_START,
    UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
    UINT _IDC_EDIT_PORTNAME,
    UINT _IDC_CHK
)
{
    g_server.Stop();
    SetRunningUi(_hDlg, false,
        _IDC_BTN_START,
        _IDC_BTN_STOP,
        _IDC_EDIT_PORTIN,
        _IDC_EDIT_PORTOUT,
        _IDC_EDIT_PORTNAME
        );
    CheckDlgButton(_hDlg, _IDC_CHK, BST_UNCHECKED);
}


INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {

            LoadSettings(hDlg,
                IDC_EDIT_PORTIN1,
                IDC_EDIT_PORTOUT1,
				IDC_EDIT_PORTNAME1
            );

            // チェックは表示専用にする想定（必要なら外してください）
            //EnableWindow(GetDlgItem(hDlg, IDC_CHK01), FALSE);

            SetRunningUi(hDlg, false,
                IDC_BTN_START1,
                IDC_BTN_STOP1,
                IDC_EDIT_PORTIN1,
                IDC_EDIT_PORTOUT1,
                IDC_EDIT_PORTNAME1
                );

            // タイトルを設定（不要なら消してOK）
            //SetWindowTextW(hDlg, szTitle);
            return (INT_PTR)TRUE;
        }break;

        case WM_APP_RX_STATUS:
        {
            const bool receiving = (wParam != 0);
            CheckDlgButton(hDlg, IDC_CHK1, receiving ? BST_CHECKED : BST_UNCHECKED);
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);

            switch (id)
            {
                case IDC_BTN_START1:
                {
                    IDC_BTN_START(
                        hDlg,
						IDC_BTN_START1,
						IDC_BTN_STOP1,
                        IDC_EDIT_PORTIN1,
                        IDC_EDIT_PORTOUT1,
                        IDC_EDIT_PORTNAME1,
                        IDC_CHK1
					);
#if 0
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
#endif
                    return (INT_PTR)TRUE;
                }
                case IDC_BTN_STOP1:
                {
                    //g_server.Stop();
                    //SetRunningUi(hDlg, false);
                    //CheckDlgButton(hDlg, IDC_CHK1, BST_UNCHECKED);

                    BTN_STOP(hDlg, 
                        IDC_BTN_START1,
                        IDC_BTN_STOP1,
                        IDC_EDIT_PORTIN1,
                        IDC_EDIT_PORTOUT1,
                        IDC_EDIT_PORTNAME1,
                        IDC_CHK1
                    );

                    return (INT_PTR)TRUE;
                }
                case IDOK:
                case IDCANCEL:
                {
                    // 終了時：稼働中なら止める → 設定保存 → ダイアログ終了
                    if (g_server.IsRunning())
                        g_server.Stop();

                    SaveSettings(hDlg,
                        IDC_EDIT_PORTIN1,
                        IDC_EDIT_PORTOUT1,
                        IDC_EDIT_PORTNAME1
                    );
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

            SaveSettings(
                hDlg,
                IDC_EDIT_PORTIN1,
                IDC_EDIT_PORTOUT1,
                IDC_EDIT_PORTNAME1
            );
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
