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

// 好きな色にするための値（必要なら後で動的に変更してOK）
COLORREF ONAIR_GRAY = RGB(128, 128, 128);
COLORREF ONAIR_RED = RGB(240, 30, 0);
bool g_onair[32] = {};
HBRUSH g_hbrOnAirRed = CreateSolidBrush(ONAIR_RED);
HBRUSH g_hbrOnAirGray = CreateSolidBrush(ONAIR_GRAY);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    hInst = hInstance;

    //アイコンの登録
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ROBOT128));
    if (hIcon)
    {
        // アイコンを登録
        SendMessage(HWND_BROADCAST, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(HWND_BROADCAST, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

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
////////////////////////////////////////////////////////////////////////////////
//cmmbno box に URL をセット
//    FillUrlCombo(hDlg, IDC_CMB_BCFRASE_1, int out_port, const std::string & channel_name)

void IDC_SET_CMBBOX_URL(
    HWND _hDlg,
    UINT _IDC_CMB_BCFRASE,
    UINT _IDC_EDIT_PORTOUT,
    UINT _IDC_EDIT_PORTNAME
) {
    int outPort = 0;
    if (!GetIntFromEdit(_hDlg, _IDC_EDIT_PORTOUT, outPort) ||
        outPort < 1 ||
        outPort > 65535)
    {
        MessageBoxW(_hDlg, L"ポート番号が不正です。(1～65535)", L"入力エラー", MB_ICONERROR);
        return;
    }

    wchar_t nameW[256]{};
    GetDlgItemTextW(_hDlg, _IDC_EDIT_PORTNAME, nameW, 256);
    std::wstring channelW = nameW;
    if (channelW.empty())
    {
        MessageBoxW(_hDlg, L"ストリーム名（Channel）が空です。", L"入力エラー", MB_ICONERROR);
        return;
    }

    FillUrlCombo(
        _hDlg,
        _IDC_CMB_BCFRASE,
        outPort,
        WideToUtf8(channelW)
    );

}

////////////////////////////////////////////////////////////////////////////////
bool IDC_BTN_START(
    HWND _hDlg,
    RtspServerController& _g_server,
    UINT _ch,
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
    std::string channelUtf8 = WideToUtf8(channelW);

#if 0    
    bool _ret = _g_server.Start(inPort, outPort, channelUtf8, _hDlg);
#else

    //bool _ret = _g_server.StartEx(inPort, outPort, _ch, channelUtf8, _hDlg);
	_g_server.rCtrl.ch = _ch; // チャンネル番号をセット
	_g_server.rCtrl.in_port = inPort;
	_g_server.rCtrl.out_port = outPort;
	_g_server.rCtrl.channel_name = channelUtf8;

    bool _ret = _g_server.StartExx(_hDlg);
#endif

    if (!_ret)
    {
        MessageBoxW(_hDlg, L"既に起動中です。", L"情報", MB_OK | MB_ICONINFORMATION);
        return true;
    }

    SetRunningUi(_hDlg, true,
        _IDC_BTN_START,
        _IDC_BTN_STOP,
        _IDC_EDIT_PORTIN,
        _IDC_EDIT_PORTOUT,
        _IDC_EDIT_PORTNAME
    );
    return true;
}

///////////////////////////////////////////////////////////////////////////////
bool IDC_BTN_START(
    HWND _hDlg,
    APP_SETTINGS& _GAPP,
    UINT _ch
){
//	gGstSv[_ch].rCtrl.ch = _ch; // チャンネル番号をセット

//URLコンボボックス更新
    IDC_SET_CMBBOX_URL(
        _hDlg,
        GAPP.IDC_CMB_BCFRASE[_ch],
        GAPP.IDC_EDIT_PORTOUT[_ch],
        GAPP.IDC_EDIT_PORTNAME[_ch]
    );

    return IDC_BTN_START(
        _hDlg,
		gGstSv[_ch],
        _ch,
        //以下は初期値配列なのでクラスとは別レイヤとして考えること
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

	//Onair表示を灰色に戻す パイプラインから通知が来ないので手動で再描画をコール
	//g_onair[_ch] = false;
 //   for (int ch = 0; ch < MAXCH; ++ch) {
 //       //g_onair[ch] = false;
 //       HWND hCtl = GetDlgItem(_hDlg, IDC_ONAIR_1 + ch);
 //       InvalidateRect(hCtl, nullptr, TRUE);
 //   }
 //   UpdateWindow(_hDlg); // まとめて更新
}

inline std::wstring string2wstring(const std::string& s)
{
    //int requiredSize = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);

    int requiredSize = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
    std::wstring result(requiredSize - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &result[0], requiredSize);

    //MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], requiredSize);

    return result;
}


void WM_APPRXSTATUS(
    HWND _hDlg,
    WPARAM wParam,
    APP_SETTINGS& _GAPP,
	UINT _IDC_CHK,
    UINT _IDC_SRCIP,
    UINT _IDC_STC,
	int _ch
){
    const bool receiving = (wParam != 0);
    CheckDlgButton(_hDlg, _IDC_CHK, receiving ? BST_CHECKED : BST_UNCHECKED);

	//gGstSv[_ch].rCtrl.br_kbps IDC_BITRATE_1に表示
    int _bitrate = gGstSv[_ch].rCtrl.br_kbps;
	wchar_t buf[64]{};
	_snwprintf_s(buf, _TRUNCATE, L"%d kbps", _bitrate);
	SetDlgItemTextW(_hDlg, IDC_BITRATE_1 + _ch, buf);

	//色を変える 変えるのはWindowProcのWM_CTLCOLORSTATIC内
    g_onair[_ch] = receiving; // ★ここで状態を保存する
    HWND hCtl = GetDlgItem(_hDlg, IDC_ONAIR_1 + _ch);
    InvalidateRect(hCtl, nullptr, TRUE);

    // sender ip:port 表示
    std::string ip;
    uint16_t port = 0;
    {
        std::lock_guard<std::mutex> lk(gGstSv[_ch].rCtrl.src_mtx);
        ip = gGstSv[_ch].rCtrl.src_ip;
        port = gGstSv[_ch].rCtrl.src_port;
    }

    if (!ip.empty() && port != 0)
    {
		std::wstring wip = string2wstring(ip);
        wchar_t sbuf[128]{};
        _snwprintf_s(sbuf, _TRUNCATE, L"%s:%u", wip.c_str(), (unsigned)port);

        SetDlgItemTextW(_hDlg, _IDC_SRCIP, sbuf);
    }

}

INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
			// アイコンの設定
            HICON hIconBig = (HICON)LoadImageW(
                GetModuleHandleW(nullptr),
                MAKEINTRESOURCEW(IDI_ROBOT128),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXICON),
                GetSystemMetrics(SM_CYICON),
                LR_DEFAULTCOLOR);

            HICON hIconSmall = (HICON)LoadImageW(
                GetModuleHandleW(nullptr),
                MAKEINTRESOURCEW(IDI_ROBOT64),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON),
                GetSystemMetrics(SM_CYSMICON),
                LR_DEFAULTCOLOR);

            // 大アイコン（Alt+Tab、タスクバー等）
            SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
            // 小アイコン（タイトルバー）
            SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            //アイコンの設定 ここまで

            LoadSettings(hDlg, GAPP);
            SetRunningUi(hDlg, false, GAPP);
            // タイトルを設定（不要なら消してOK）
            //SetWindowTextW(hDlg, szTitle);

            for (int i = 0; i < GAPP.IDC_CMB_BCFRASE.size(); ++i)
            {
                IDC_SET_CMBBOX_URL(
                    hDlg,
                    GAPP.IDC_CMB_BCFRASE[i],
                    GAPP.IDC_EDIT_PORTOUT[i],
                    GAPP.IDC_EDIT_PORTNAME[i]
                );
            }


            return (INT_PTR)TRUE;
        }break;
/*
        case WM_APP_RX_STATUS:
        {
            const bool receiving = (wParam != 0);
            CheckDlgButton(hDlg, IDC_CHK1, receiving ? BST_CHECKED : BST_UNCHECKED);
            return (INT_PTR)TRUE;
        }
        case WM_APP_RX_STATUS+1:
        {
            const bool receiving = (wParam != 0);
            CheckDlgButton(hDlg, IDC_CHK2, receiving ? BST_CHECKED : BST_UNCHECKED);

            //同じ意味
            //if( wParam) {
            //    CheckDlgButton(hDlg, IDC_CHK2, BST_CHECKED);
            //}else {
            //    CheckDlgButton(hDlg, IDC_CHK2, BST_UNCHECKED);
            //}
            return (INT_PTR)TRUE;
        }break;
*/
        case WM_APP_RX_STATUS + 0: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK1, IDC_SRCIP_1, IDC_BITRATE_1, 0); }  break;
		case WM_APP_RX_STATUS + 1: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK2, IDC_SRCIP_2, IDC_BITRATE_2, 1); }  break;
		case WM_APP_RX_STATUS + 2: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK3, IDC_SRCIP_3, IDC_BITRATE_3, 2); }  break;
		case WM_APP_RX_STATUS + 3: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK4, IDC_SRCIP_4, IDC_BITRATE_4, 3); }  break;
		case WM_APP_RX_STATUS + 4: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK5, IDC_SRCIP_5, IDC_BITRATE_5, 4); }  break;
		case WM_APP_RX_STATUS + 5: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK6, IDC_SRCIP_6, IDC_BITRATE_6, 5); }  break;
		case WM_APP_RX_STATUS + 6: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK7, IDC_SRCIP_7, IDC_BITRATE_7, 6); }  break;
		case WM_APP_RX_STATUS + 7: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK8, IDC_SRCIP_8, IDC_BITRATE_8, 7); }  break;
        case WM_APP_RX_STATUS + 8: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK9, IDC_SRCIP_9, IDC_BITRATE_9, 8); }  break;
		case WM_APP_RX_STATUS + 9: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK10, IDC_SRCIP_10, IDC_BITRATE_10, 9); }  break;
		case WM_APP_RX_STATUS + 10: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK11, IDC_SRCIP_11, IDC_BITRATE_11, 10); }  break;
		case WM_APP_RX_STATUS + 11: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK12, IDC_SRCIP_12, IDC_BITRATE_12, 11); }  break;
		case WM_APP_RX_STATUS + 12: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK13, IDC_SRCIP_13, IDC_BITRATE_13, 12); }  break;
		case WM_APP_RX_STATUS + 13: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK14, IDC_SRCIP_14, IDC_BITRATE_14, 13); }  break;
		case WM_APP_RX_STATUS + 14: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK15, IDC_SRCIP_15, IDC_BITRATE_15, 14); }  break;
		case WM_APP_RX_STATUS + 15: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK16, IDC_SRCIP_16, IDC_BITRATE_16, 15); }  break;
		case WM_APP_RX_STATUS + 16: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK17, IDC_SRCIP_17, IDC_BITRATE_17, 16); }  break;
		case WM_APP_RX_STATUS + 17: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK18, IDC_SRCIP_18, IDC_BITRATE_18, 17); }  break;
		case WM_APP_RX_STATUS + 18: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK19, IDC_SRCIP_19, IDC_BITRATE_19, 18); }  break;
		case WM_APP_RX_STATUS + 19: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK20, IDC_SRCIP_20, IDC_BITRATE_20, 19); }  break;
		case WM_APP_RX_STATUS + 20: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK21, IDC_SRCIP_21, IDC_BITRATE_21, 20); }  break;
		case WM_APP_RX_STATUS + 21: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK22, IDC_SRCIP_22, IDC_BITRATE_22, 21); }  break;
		case WM_APP_RX_STATUS + 22: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK23, IDC_SRCIP_23, IDC_BITRATE_23, 22); }  break;
		case WM_APP_RX_STATUS + 23: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK24, IDC_SRCIP_24, IDC_BITRATE_24, 23); }  break;
		case WM_APP_RX_STATUS + 24: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK25, IDC_SRCIP_25, IDC_BITRATE_25, 24); }  break;
		case WM_APP_RX_STATUS + 25: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK26, IDC_SRCIP_26, IDC_BITRATE_26, 25); }  break;
		case WM_APP_RX_STATUS + 26: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK27, IDC_SRCIP_27, IDC_BITRATE_27, 26); }  break;
		case WM_APP_RX_STATUS + 27: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK28, IDC_SRCIP_28, IDC_BITRATE_28, 27); }  break;
		case WM_APP_RX_STATUS + 28: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK29, IDC_SRCIP_29, IDC_BITRATE_29, 28); }  break;
		case WM_APP_RX_STATUS + 29: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK30, IDC_SRCIP_30, IDC_BITRATE_30, 29); }  break;
		case WM_APP_RX_STATUS + 30: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK31, IDC_SRCIP_31, IDC_BITRATE_31, 30); }  break;
		case WM_APP_RX_STATUS + 31: { WM_APPRXSTATUS(hDlg, wParam, GAPP, IDC_CHK32, IDC_SRCIP_32, IDC_BITRATE_32, 31); }  break;
        
        case WM_CTLCOLORSTATIC: {
            HDC  hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            const int id = GetDlgCtrlID(hCtl);
            if (id >= IDC_ONAIR_1 && id < IDC_ONAIR_1 + 32) {
                const int ch = id - IDC_ONAIR_1;
                if (g_onair[ch]) {
                    SetTextColor(hdc, RGB(255, 255, 255)); // 文字色
                    SetBkColor(hdc, ONAIR_RED);            // 背景色（必ず同じ色）
                    SetBkMode(hdc, OPAQUE);
                    return (INT_PTR)g_hbrOnAirRed;         // ★このブラシが背景になる
                }
                else {
                    SetTextColor(hdc, RGB(0, 0, 0));
                    SetBkColor(hdc, ONAIR_GRAY);
                    SetBkMode(hdc, OPAQUE);
                    return (INT_PTR)g_hbrOnAirGray;
                }
            }
            break;
        }break;
                              
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

            if (code == CBN_SELCHANGE) {
                // 32chぶんまとめて処理
                if (id >= IDC_CMB_BCFRASE_1 && id < (IDC_CMB_BCFRASE_1 + MAXCH)) {
                    CopySelectedComboTextToClipboard(hDlg, (int)id);
                    return (INT_PTR)TRUE;
                }
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

                case IDC_BTN_START_ALL:
                {
                    // 全チャンネル開始
                    for (UINT ch = 0; ch < GAPP.size; ++ch) {
                        if (gGstSv.size() > 0) {
                            if (!gGstSv[ch].IsRunning()) {
                                IDC_BTN_START(hDlg, GAPP, ch);
                            }
                        }
                    }
				}break;
                case IDC_BTN_STOP_ALL:
                {
                    // 全チャンネル停止
                    for (UINT ch = 0; ch < GAPP.size; ++ch) {
                        if (gGstSv.size() > 0) {
                            if (gGstSv[ch].IsRunning()) {
                                BTN_STOP(hDlg, GAPP, ch);
                            }
                        }
                    }
				}break;

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
