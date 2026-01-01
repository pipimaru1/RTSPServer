    #include "stdafx.h"
#include "framework.h"
#include "RTSPServer.h"
#include "RTSPServerGUI.h"


#define DEFAULT_STREAMING_SUBNAME L"default"
// 通知用（GUI側と同じ値にしてください）
constexpr UINT WM_APP_SERVER_EXITED = WM_APP + 101;

RtspServerController::RtspServerController() {}
RtspServerController::~RtspServerController()
{
    Stop();
}

// RTSPサーバーを開始
// WinProcのIDC_BTN_START から呼ばれるのはこれ
// RtspServerControllerは　gGstSv[ch]の形で複数インスタンがある。
#ifdef USE_OLD_VERSION
bool RtspServerController::Start(int inPort, int outPort, const std::string& channelUtf8, HWND hwndNotify)
{
    if (running_.load()) 
        return false;

    hwndNotify_ = hwndNotify;
    rCtrl.ptLoop = nullptr;

    running_.store(true);

    th_ = std::thread(&RtspServerController::ThreadMain, this, inPort, outPort, channelUtf8);
    //th_ = std::thread(&RtspServerController::ThreadMainEx, this, inPort, outPort, channelUtf8);

    return true;
}
#endif

bool RtspServerController::StartEx(HWND hwndNotify, bool _test_pattern)
{
	// すでに起動中なら何もしない
    if (running_.load())
        return false;

	// hwndNotify を保存 パイプラインからの通知を受けるウィンドウハンドル
    hwndNotify_ = hwndNotify;
    running_.store(true);

	// スレッド開始 パイプライン起動
	if (_test_pattern)
		th_ = std::thread(&RtspServerController::ThreadMainExTest, this);
	else
        th_ = std::thread(&RtspServerController::ThreadMainEx, this);
    return true;
}

// RTSPサーバーを停止
void RtspServerController::Stop()
{
    if (!running_.load())
        return;

    // loop_ が生成済みなら quit
    if (rCtrl.ptLoop != nullptr)
    {
        g_main_loop_quit(rCtrl.ptLoop);
        // wakeup を入れておくと止まりが良いことが多い
        if (GMainContext* ctx = g_main_loop_get_context(rCtrl.ptLoop))
        {
            g_main_context_wakeup(ctx);
        }
    }
	//rCtrl.g_rx_watch_id が0になるまで待つ
    while (rCtrl.g_rx_watch_id != 0)
    {
        //0.1秒
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

    //Onair表示を灰色に戻す パイプラインから通知が来ないので手動で再描画をコール
	g_onair[rCtrl.ch] = false;
    HWND hCtl = GetDlgItem(hwndNotify_, IDC_ONAIR_1 + rCtrl.ch);
    InvalidateRect(hCtl, nullptr, TRUE);
    UpdateWindow(hwndNotify_); // まとめて更新

    if (th_.joinable())
        th_.join();

    // OpenRTSPServer 側では unref していないのでここで unref
    if (rCtrl.ptLoop != nullptr)
    {
        g_main_loop_unref(rCtrl.ptLoop);
        rCtrl.ptLoop = nullptr;
    }

    running_.store(false);
}

//////////////////////////////////////////////////////////
// RTSPサーバースレッドのメイン
//////////////////////////////////////////////////////////
#ifdef USE_OLD_VERSION
void RtspServerController::ThreadMain(int inPort, int outPort, std::string channelUtf8)
{
    // OpenRTSPServer が g_option_context_parse を呼ぶので、ダミー argv を用意
    char arg0[] = "RTSPServerGUI";
    char* argv[] = { arg0, nullptr };
    int argc = 1;

    // loop_ は OpenRTSPServer 内で g_main_loop_new されます
    int r = OpenRTSPServer(rCtrl.ptLoop, inPort, outPort, channelUtf8, argc, argv);
    (void)r;

    // 予期せぬ終了や Stop 後の終了も含め、GUIへ通知
    if (hwndNotify_ != nullptr)
    {
        PostMessageW(hwndNotify_, WM_APP_SERVER_EXITED, 0, 0);
    }

    // running_ は Stop() 側で最終確定します（ここでは触らない）
}
#endif
//////////////////////////////////////////////////////////
// 
// RTSPサーバースレッドのメイン
// 
//////////////////////////////////////////////////////////
void RtspServerController::ThreadMainEx()
{
    //「ThreadMain() が動いている間」は arg0 などのローカル変数はキープされています。
    // なのでパイプラインが動いている間は基本的には有効。

    // OpenRTSPServer が g_option_context_parse を呼ぶので、ダミー argv を用意
    char arg0[] = "RTSPServerGUI";
    char* argv[] = { arg0, nullptr };
    int argc = 1;

    // loop_ は OpenRTSPServer 内で g_main_loop_new されます
    //int r = OpenRTSPServer(loop_, inPort, outPort, channelUtf8, argc, argv);
    int r = OpenRTSPServerEx(rCtrl, argc, argv);

    (void)r;

    // 予期せぬ終了や Stop 後の終了も含め、GUIへ通知
    if (hwndNotify_ != nullptr)
    {
        PostMessageW(hwndNotify_, WM_APP_SERVER_EXITED, 0, 0);
    }

    // running_ は Stop() 側で最終確定します（ここでは触らない）
}
//上の関数のテスト用
void RtspServerController::ThreadMainExTest()
{
    //「ThreadMain() が動いている間」は arg0 などのローカル変数はキープされています。
    // なのでパイプラインが動いている間は基本的には有効。

    // OpenRTSPServer が g_option_context_parse を呼ぶので、ダミー argv を用意
    char arg0[] = "RTSPServerGUI";
    char* argv[] = { arg0, nullptr };
    int argc = 1;

    // loop_ は OpenRTSPServer 内で g_main_loop_new されます
    //int r = OpenRTSPServer(loop_, inPort, outPort, channelUtf8, argc, argv);
    int r = OpenRTSPServerEx(rCtrl, argc, argv, true);

    (void)r;

    // 予期せぬ終了や Stop 後の終了も含め、GUIへ通知
    if (hwndNotify_ != nullptr)
    {
        PostMessageW(hwndNotify_, WM_APP_SERVER_EXITED, 0, 0);
    }

    // running_ は Stop() 側で最終確定します（ここでは触らない）
}
//////////////////////////////////////////////////////////

std::wstring GetIniPath()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    //std::wstring p = path;
    //auto pos = p.find_last_of(L"\\/");
    //if (pos != std::wstring::npos)
    //    p = p.substr(0, pos + 1);

    //p += L"settings.ini";
    //return p;

    std::filesystem::path exe(path);
    return (exe.parent_path() / L"settings.ini").wstring();
}

void SaveSettings(
    HWND hDlg, 
    UINT _IDC_EDIT_PORTIN, 
    UINT _IDC_EDIT_PORTOUT, 
    UINT _IDC_EDIT_PORTNAME
)
{
    const std::wstring ini = GetIniPath();

    wchar_t buf[256]{};

    GetDlgItemTextW(hDlg, _IDC_EDIT_PORTIN, buf, 256);
    WritePrivateProfileStringW(L"Server", L"InPort", buf, ini.c_str());
    GetDlgItemTextW(hDlg, _IDC_EDIT_PORTOUT, buf, 256);
    WritePrivateProfileStringW(L"Server", L"OutPort", buf, ini.c_str());
    GetDlgItemTextW(hDlg, _IDC_EDIT_PORTNAME, buf, 256);
    WritePrivateProfileStringW(L"Server", L"Channel", buf, ini.c_str());
}

void SaveSettings(
    HWND hDlg,
	APP_SETTINGS& _GAPP
)
{
    const std::wstring ini = GetIniPath();

    wchar_t buf[256]{};
	for (int i = 0; i < _GAPP.size; ++i)
    {       
        GetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTIN[i], buf, 256);
        WritePrivateProfileStringW(_GAPP.SVNAME[i].c_str(), L"InPort", buf, ini.c_str());
        GetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTOUT[i], buf, 256);
        WritePrivateProfileStringW(_GAPP.SVNAME[i].c_str(), L"OutPort", buf, ini.c_str());
        GetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTNAME[i], buf, 256);
        WritePrivateProfileStringW(_GAPP.SVNAME[i].c_str(), L"Channel", buf, ini.c_str());
    }
	//HTTP_PORTとHLS_STRを保存
    GetDlgItemTextW(hDlg, IDC_EDIT_HTTPPORT, buf, 256);
    WritePrivateProfileStringW(L"HTTP", L"Port", buf, ini.c_str());
	GetDlgItemTextW(hDlg, IDC_EDIT_HLS, buf, 256);
	WritePrivateProfileStringW(L"HTTP", L"HLS", buf, ini.c_str());
}

void LoadSettings(HWND hDlg, UINT _IDC_EDIT_PORTIN, UINT _IDC_EDIT_PORTOUT, UINT _IDC_EDIT_PORTNAME, UINT _IDC_ )
{
    const std::wstring ini = GetIniPath();

    int inPort = GetPrivateProfileIntW(L"Server", L"InPort", 5004, ini.c_str());
    int outPort = GetPrivateProfileIntW(L"Server", L"OutPort", 8554, ini.c_str());

    wchar_t channel[256]{};
    GetPrivateProfileStringW(L"Server", L"Channel", DEFAULT_STREAMING_SUBNAME, channel, 256, ini.c_str());

    wchar_t tmp[32]{};
    _snwprintf_s(tmp, _TRUNCATE, L"%d", inPort);
    SetDlgItemTextW(hDlg, _IDC_EDIT_PORTIN, tmp);

    _snwprintf_s(tmp, _TRUNCATE, L"%d", outPort);
    SetDlgItemTextW(hDlg, _IDC_EDIT_PORTOUT, tmp);

    SetDlgItemTextW(hDlg, _IDC_EDIT_PORTNAME, channel);
}

void LoadSettings(
    HWND hDlg,
    APP_SETTINGS& _GAPP
){
    const std::wstring ini = GetIniPath();

    for (int i = 0; i < _GAPP.size; ++i)
    {
        int inPort = GetPrivateProfileIntW(_GAPP.SVNAME[i].c_str(), L"InPort",   _GAPP._DEF_PORTIN[i], ini.c_str());
        int outPort = GetPrivateProfileIntW(_GAPP.SVNAME[i].c_str(), L"OutPort", _GAPP._DEF_PORTOUT[i], ini.c_str());
        wchar_t channel[256]{};
        GetPrivateProfileStringW(_GAPP.SVNAME[i].c_str(), L"Channel", DEFAULT_STREAMING_SUBNAME, channel, 256, ini.c_str());
        wchar_t tmp[32]{};
        _snwprintf_s(tmp, _TRUNCATE, L"%d", inPort);
        SetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTIN[i], tmp);
        _snwprintf_s(tmp, _TRUNCATE, L"%d", outPort);
        SetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTOUT[i], tmp);
        SetDlgItemTextW(hDlg, _GAPP.IDC_EDIT_PORTNAME[i], channel);
    }

	//HTTP_PORTとHLS_STRを読み込み
    int httpPort = GetPrivateProfileIntW(L"HTTP", L"Port", _GAPP.HTTP_PORT, ini.c_str());
    wchar_t hlsStr[256]{};
    GetPrivateProfileStringW(L"HTTP", L"HLS", _GAPP.HLS_STR.c_str(), hlsStr, 256, ini.c_str());
    wchar_t tmp[32]{};
    _snwprintf_s(tmp, _TRUNCATE, L"%d", httpPort);
    SetDlgItemTextW(hDlg, IDC_EDIT_HTTPPORT, tmp);
	SetDlgItemTextW(hDlg, IDC_EDIT_HLS, hlsStr);
}

//コントロールの状態を一括変更
void SetRunningUi(HWND hDlg, bool running,
    UINT _IDC_BTN_START,
	UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
	UINT _IDC_EDIT_PORTNAME
)
{
    // ステータスチェック（表示用：ユーザー操作させないなら Disable）
    //CheckDlgButton(hDlg, IDC_CHK01, running ? BST_CHECKED : BST_UNCHECKED);

    EnableWindow(GetDlgItem(hDlg, _IDC_BTN_START), running ? FALSE : TRUE);
    EnableWindow(GetDlgItem(hDlg, _IDC_BTN_STOP), running ? TRUE : FALSE);

    // 稼働中は編集不可にする（事故防止）
    EnableWindow(GetDlgItem(hDlg, _IDC_EDIT_PORTIN), running ? FALSE : TRUE);
    EnableWindow(GetDlgItem(hDlg, _IDC_EDIT_PORTOUT), running ? FALSE : TRUE);
    EnableWindow(GetDlgItem(hDlg, _IDC_EDIT_PORTNAME), running ? FALSE : TRUE);
}

//コントロールの状態を一括変更
void SetRunningUi(HWND hDlg, bool running,
    APP_SETTINGS& _GAPP
)
{
    // ステータスチェック（表示用：ユーザー操作させないなら Disable）
    //CheckDlgButton(hDlg, IDC_CHK01, running ? BST_CHECKED : BST_UNCHECKED);

    // 稼働中は編集不可にする（事故防止）
    for (int i = 0; i < _GAPP.size; ++i)
    {
        EnableWindow(GetDlgItem(hDlg, _GAPP.IDC_BTN_START[i]), running ? FALSE : TRUE);
        EnableWindow(GetDlgItem(hDlg, _GAPP.IDC_BTN_STOP[i]), running ? TRUE : FALSE);
        EnableWindow(GetDlgItem(hDlg, _GAPP.IDC_EDIT_PORTIN[i]), running ? FALSE : TRUE);
        EnableWindow(GetDlgItem(hDlg, _GAPP.IDC_EDIT_PORTOUT[i]), running ? FALSE : TRUE);
        EnableWindow(GetDlgItem(hDlg, _GAPP.IDC_EDIT_PORTNAME[i]), running ? FALSE : TRUE);
    }
}

std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), s.data(), len, nullptr, nullptr);
    return s;
}

bool GetIntFromEdit(HWND hDlg, int id, int& outValue)
{
    wchar_t buf[64]{};
    GetDlgItemTextW(hDlg, id, buf, 64);
    int v = _wtoi(buf);
    if (v <= 0) return false;
    outValue = v;
    return true;
}

/*
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
*/
bool SetClipboardTextW(HWND owner, const std::wstring& text)
{
    if (!OpenClipboard(owner)) return false;

    EmptyClipboard();

    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    void* p = GlobalLock(hMem);
    if (!p) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    memcpy(p, text.c_str(), bytes);
    GlobalUnlock(hMem);

    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    // 成功したら hMem の所有権は OS 側へ移る（GlobalFree しない）
    CloseClipboard();
    return true;
}

std::wstring Utf8ToW(const std::string& s)
{
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w;
    w.resize(n - 1);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

std::string MakeChannelSuffix(const std::string& channel_name)
{
    // 例: "default" -> "/default"
    //     "/default" -> "/default"
    if (channel_name.empty()) return std::string("/");
    if (channel_name[0] == '/') return channel_name;
    return std::string("/") + channel_name;
}

void FillUrlCombo(HWND hDlg, int combo_id, int out_port, const std::string& channel_name)
{
    HWND hCmb = GetDlgItem(hDlg, combo_id);
    if (!hCmb) return;

    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);

    const std::string suffix = MakeChannelSuffix(channel_name);

    // 127.0.0.1 は必ず入れる
    {
        std::string url = "rtsp://127.0.0.1:" + std::to_string(out_port) + suffix;
        std::wstring wurl = Utf8ToW(url);
        SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)wurl.c_str());
    }

    // ローカルIP一覧
    const std::vector<std::string> ipList = getLocalIPAddresses();

#if 1
    for (const std::string& ip : ipList) {
        // 127.0.0.1 と同じなら重複回避
        if (ip == "127.0.0.1") 
            continue;
        std::string url = "rtsp://" + ip + ":" + std::to_string(out_port) + suffix;
        std::wstring wurl = Utf8ToW(url);
        SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)wurl.c_str());
    }
#else
    for (auto it = ipList.rbegin(); it != ipList.rend(); ++it) {
        const std::string& ip = *it;
        if (ip == "127.0.0.1")
            continue;
        std::string url = "rtsp://" + ip + ":" + std::to_string(out_port) + suffix;
        std::wstring wurl = Utf8ToW(url);
        SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)wurl.c_str());
    }
#endif

    // 先頭を選択しておく（必要なら）
    SendMessageW(hCmb, CB_SETCURSEL, 0, 0);
}
////////////////////////////////////////////////////////////////////////////////
//cmmbno box に HTTPのHLSのURL をセット
// http://<IP_ADDRESS>:<HTTPPORT>/<channel_name>-<out_port>/index.m3u8
void FillUrlCombo_http(
    HWND hDlg, 
    int combo_id, 
    int http_port, 
    int out_port, 
    const std::wstring& channel_name, 
    const std::wstring& _hls
)
{
    HWND hCmb = GetDlgItem(hDlg, combo_id);
    if (!hCmb) return;
    SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);

    // 127.0.0.1 は必ず入れる
    {
        std::wstring wurl = L"http://127.0.0.1:" 
            + std::to_wstring(http_port)
			//+ L"/" + _hls
            + L"/" + channel_name
			+ L"-" + std::to_wstring(out_port) 
            + L"/index.m3u8";

        SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)wurl.c_str());
    }
	// ローカルIP一覧
    const std::vector<std::string> ipList = getLocalIPAddresses();
    for (const std::string& ip : ipList) {
        // 127.0.0.1 と同じなら重複回避
        if (ip == "127.0.0.1")
            continue;
		std::wstring wip = Utf8ToW(ip);
        std::wstring wurl = L"http://" 
            + wip 
            + L":" + std::to_wstring(http_port)
            //+ L"/" + _hls
            + L"/" + channel_name
            + L"-" + std::to_wstring(out_port) 
            + L"/index.m3u8";
        SendMessageW(hCmb, CB_ADDSTRING, 0, (LPARAM)wurl.c_str());
    }
    // 先頭を選択しておく（必要なら）
	SendMessageW(hCmb, CB_SETCURSEL, 0, 0);
}


//選択されたURLをクリップボードへ（CBN_SELCHANGE）
void CopySelectedComboTextToClipboard(HWND hDlg, int combo_id)
{
    HWND hCmb = GetDlgItem(hDlg, combo_id);
    if (!hCmb) return;

    const LRESULT sel = SendMessageW(hCmb, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) return;

    const LRESULT len = SendMessageW(hCmb, CB_GETLBTEXTLEN, (WPARAM)sel, 0);
    if (len == CB_ERR || len <= 0) return;

    std::wstring text;
    text.resize((size_t)len);

    SendMessageW(hCmb, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)&text[0]);

    SetClipboardTextW(hDlg, text);
}

void ForceWindowOnVisibleMonitor(HWND hWnd)
{
    // いまのウィンドウ位置（ダイアログの矩形）
    RECT rc;
    GetWindowRect(hWnd, &rc);

    // このウィンドウに一番近いモニタ（画面外なら最寄り/なければプライマリ）
    HMONITOR hMon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMon, &mi);

    // 作業領域（タスクバー等を除いた見える領域）
    RECT wa = mi.rcWork;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x = rc.left;
    int y = rc.top;

    // 左上が作業領域からはみ出していたら戻す
    if (x < wa.left) x = wa.left;
    if (y < wa.top)  y = wa.top;

    // 右下がはみ出していたら戻す
    if (x + w > wa.right)  x = wa.right - w;
    if (y + h > wa.bottom) y = wa.bottom - h;

    // 念のため（ダイアログが画面より大きい場合）
    if (x < wa.left) x = wa.left;
    if (y < wa.top)  y = wa.top;

    SetWindowPos(hWnd, nullptr, x, y, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}
