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

//2025年12月18日

#include "stdafx.h"
#include "framework.h"
#include "RTSPServer.h"
#include "RTSPServerGUI.h"

#define MAX_LOADSTRING 100

//RtspServerController g_server_;
//RtspServerController gGstSv[MAXCH];
//std::vector<RtspServerController > gGstSv[MAXCH];
std::array<RtspServerController, MAXCH> gGstSv;

// 通知用（Controller 側と同じ値にしてください）
constexpr UINT WM_APP_SERVER_EXITED = WM_APP + 101;

///////////////////////////////////////////////
// 
// アプリ設定のインスタンス
//
///////////////////////////////////////////////
APP_SETTINGS GAPP;

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
	RtspServerController& _g_server,
    UINT _IDC_BTN_START,
    UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
	UINT _IDC_EDIT_PORTNAME,
	UINT _IDC_CHK
){
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
#define NO_USE_STUCT_GSTRSV

#if defined(NO_USE_STUCT_GSTRSV)
    std::string channelUtf8 = WideToUtf8(channelW);
    if (!_g_server.Start(inPort, outPort, channelUtf8, _hDlg))
    {
        MessageBoxW(_hDlg, L"既に起動中です。", L"情報", MB_OK | MB_ICONINFORMATION);
        return true;
    }
#else
	GST_RTSP_SVPARAMS _gsrv;
	_gsrv.in_port = inPort;
	_gsrv.out_port = outPort;
	_gsrv.channel_name = WideToUtf8(channelW);
    if (!_g_server.Start(_gsrv, _hDlg))
    {
        MessageBoxW(_hDlg, L"既に起動中です。", L"情報", MB_OK | MB_ICONINFORMATION);
        return true;
    }

#endif
    SetRunningUi(_hDlg, true,
        _IDC_BTN_START,
        _IDC_BTN_STOP,
        _IDC_EDIT_PORTIN,
        _IDC_EDIT_PORTOUT,
        _IDC_EDIT_PORTNAME
    );
    return true;
}
bool IDC_BTN_START(
    HWND _hDlg,
    APP_SETTINGS& _GAPP,
    UINT _ch
){
    return IDC_BTN_START(
        _hDlg,
		gGstSv[_ch],
		_GAPP.IDC_BTN_START[_ch],
        _GAPP.IDC_BTN_STOP[_ch],
        _GAPP.IDC_EDIT_PORTIN[_ch],
		_GAPP.IDC_EDIT_PORTOUT[_ch],
		_GAPP.IDC_EDIT_PORTNAME[_ch],
		_GAPP.IDC_CHK[_ch]
    );
}

void BTN_STOP(
    HWND _hDlg,
    RtspServerController& _g_server,
    UINT _IDC_BTN_START,
    UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
    UINT _IDC_EDIT_PORTNAME,
    UINT _IDC_CHK
){
    _g_server.Stop();
    SetRunningUi(_hDlg, false,
        _IDC_BTN_START,
        _IDC_BTN_STOP,
        _IDC_EDIT_PORTIN,
        _IDC_EDIT_PORTOUT,
        _IDC_EDIT_PORTNAME
        );
    CheckDlgButton(_hDlg, _IDC_CHK, BST_UNCHECKED);
}
void BTN_STOP(
    HWND _hDlg,
	APP_SETTINGS& _GAPP,
	UINT _ch
    ){
    BTN_STOP(
        _hDlg,
        gGstSv[_ch],
        _GAPP.IDC_BTN_START[_ch],
        _GAPP.IDC_BTN_STOP[_ch],
        _GAPP.IDC_EDIT_PORTIN[_ch],
        _GAPP.IDC_EDIT_PORTOUT[_ch],
        _GAPP.IDC_EDIT_PORTNAME[_ch],
        _GAPP.IDC_CHK[_ch]
	);
}


INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            LoadSettings(hDlg, GAPP);
            SetRunningUi(hDlg, false, GAPP);
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
            const WORD code = HIWORD(wParam);

            // 中継状態表示用チェック チェックボックス操作を無視する
            if (code == BN_CLICKED && (id >= IDC_CHK1 && id <= IDC_CHK32)) {
                // いまの状態を元に戻す（ユーザー操作を無かったことに）
                const BOOL checked = (IsDlgButtonChecked(hDlg, id) == BST_CHECKED);
                CheckDlgButton(hDlg, id, checked ? BST_UNCHECKED : BST_CHECKED);
                return TRUE; // ここで食う
            }
			//break; breakしない

            switch (id)
            {
                case IDC_BTN_START1:  { IDC_BTN_START(hDlg, GAPP,  0); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START2:  { IDC_BTN_START(hDlg, GAPP,  1); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START3:  { IDC_BTN_START(hDlg, GAPP,  2); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START4:  { IDC_BTN_START(hDlg, GAPP,  3); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START5:  { IDC_BTN_START(hDlg, GAPP,  4); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START6:  { IDC_BTN_START(hDlg, GAPP,  5); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START7:  { IDC_BTN_START(hDlg, GAPP,  6); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START8:  { IDC_BTN_START(hDlg, GAPP,  7); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START9:  { IDC_BTN_START(hDlg, GAPP,  8); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START10: { IDC_BTN_START(hDlg, GAPP,  9); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START11: { IDC_BTN_START(hDlg, GAPP, 10); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START12: { IDC_BTN_START(hDlg, GAPP, 11); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START13: { IDC_BTN_START(hDlg, GAPP, 12); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START14: { IDC_BTN_START(hDlg, GAPP, 13); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START15: { IDC_BTN_START(hDlg, GAPP, 14); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START16: { IDC_BTN_START(hDlg, GAPP, 15); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START17: { IDC_BTN_START(hDlg, GAPP, 16); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START18: { IDC_BTN_START(hDlg, GAPP, 17); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START19: { IDC_BTN_START(hDlg, GAPP, 18); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START20: { IDC_BTN_START(hDlg, GAPP, 19); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START21: { IDC_BTN_START(hDlg, GAPP, 20); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START22: { IDC_BTN_START(hDlg, GAPP, 21); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START23: { IDC_BTN_START(hDlg, GAPP, 22); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START24: { IDC_BTN_START(hDlg, GAPP, 23); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START25: { IDC_BTN_START(hDlg, GAPP, 24); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START26: { IDC_BTN_START(hDlg, GAPP, 25); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START27: { IDC_BTN_START(hDlg, GAPP, 26); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START28: { IDC_BTN_START(hDlg, GAPP, 27); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START29: { IDC_BTN_START(hDlg, GAPP, 28); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START30: { IDC_BTN_START(hDlg, GAPP, 29); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START31: { IDC_BTN_START(hDlg, GAPP, 30); return (INT_PTR)TRUE; }break;
				case IDC_BTN_START32: { IDC_BTN_START(hDlg, GAPP, 31); return (INT_PTR)TRUE; }break;

                case IDC_BTN_STOP1:  { BTN_STOP(hDlg, GAPP,  0); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP2:  { BTN_STOP(hDlg, GAPP,  1); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP3:  { BTN_STOP(hDlg, GAPP,  2); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP4:  { BTN_STOP(hDlg, GAPP,  3); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP5:  { BTN_STOP(hDlg, GAPP,  4); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP6:  { BTN_STOP(hDlg, GAPP,  5); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP7:  { BTN_STOP(hDlg, GAPP,  6); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP8:  { BTN_STOP(hDlg, GAPP,  7); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP9:  { BTN_STOP(hDlg, GAPP,  8); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP10: { BTN_STOP(hDlg, GAPP,  9); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP11: { BTN_STOP(hDlg, GAPP, 10); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP12: { BTN_STOP(hDlg, GAPP, 11); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP13: { BTN_STOP(hDlg, GAPP, 12); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP14: { BTN_STOP(hDlg, GAPP, 13); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP15: { BTN_STOP(hDlg, GAPP, 14); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP16: { BTN_STOP(hDlg, GAPP, 15); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP17: { BTN_STOP(hDlg, GAPP, 16); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP18: { BTN_STOP(hDlg, GAPP, 17); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP19: { BTN_STOP(hDlg, GAPP, 18); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP20: { BTN_STOP(hDlg, GAPP, 19); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP21: { BTN_STOP(hDlg, GAPP, 20); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP22: { BTN_STOP(hDlg, GAPP, 21); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP23: { BTN_STOP(hDlg, GAPP, 22); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP24: { BTN_STOP(hDlg, GAPP, 23); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP25: { BTN_STOP(hDlg, GAPP, 24); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP26: { BTN_STOP(hDlg, GAPP, 25); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP27: { BTN_STOP(hDlg, GAPP, 26); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP28: { BTN_STOP(hDlg, GAPP, 27); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP29: { BTN_STOP(hDlg, GAPP, 28); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP30: { BTN_STOP(hDlg, GAPP, 29); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP31: { BTN_STOP(hDlg, GAPP, 30); return (INT_PTR)TRUE; }break;
				case IDC_BTN_STOP32: { BTN_STOP(hDlg, GAPP, 31); return (INT_PTR)TRUE; }break;


                case IDOK:
                case IDCANCEL:
                {
                    // 終了時：稼働中なら止める → 設定保存 → ダイアログ終了
                    //if (g_server.IsRunning())
                    //    g_server.Stop();

                    for(UINT ch = 0; ch < GAPP.size; ++ch){
                        if (gGstSv.size() > 0) {
                            if (gGstSv[ch].IsRunning())
                                gGstSv[ch].Stop();
                        }
					}

					SaveSettings(hDlg, GAPP);   
                    SetGuiNotifyHwnd(nullptr);
                    EndDialog(hDlg, id);
                    return (INT_PTR)TRUE;
                }break;
            }
        }break;

        case WM_CLOSE:
        {
            //if (g_server.IsRunning())
            //    g_server.Stop();
            for (UINT ch = 0; ch < GAPP.size; ++ch) {
                if (gGstSv.size() > 0) {
                    if (gGstSv[ch].IsRunning())
                        gGstSv[ch].Stop();
                }
            }

            SaveSettings(hDlg, GAPP);
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
