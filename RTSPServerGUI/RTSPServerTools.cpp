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
bool RtspServerController::Start(int inPort, int outPort, const std::string& channelUtf8, HWND hwndNotify)
{
    if (running_.load()) 
        return false;

    hwndNotify_ = hwndNotify;
    loop_ = nullptr;

    running_.store(true);

    th_ = std::thread(&RtspServerController::ThreadMain, this, inPort, outPort, channelUtf8);
    return true;
}

// RTSPサーバーを停止
void RtspServerController::Stop()
{
    if (!running_.load())
        return;

    // loop_ が生成済みなら quit
    if (loop_ != nullptr)
    {
        g_main_loop_quit(loop_);
        // wakeup を入れておくと止まりが良いことが多い
        if (GMainContext* ctx = g_main_loop_get_context(loop_))
        {
            g_main_context_wakeup(ctx);
        }
    }

    if (th_.joinable())
        th_.join();

    // OpenRTSPServer 側では unref していないのでここで unref
    if (loop_ != nullptr)
    {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }

    running_.store(false);
}

// RTSPサーバースレッドのメイン
void RtspServerController::ThreadMain(int inPort, int outPort, std::string channelUtf8)
{
    // OpenRTSPServer が g_option_context_parse を呼ぶので、ダミー argv を用意
    char arg0[] = "RTSPServerGUI";
    char* argv[] = { arg0, nullptr };
    int argc = 1;

    // loop_ は OpenRTSPServer 内で g_main_loop_new されます
    int r = OpenRTSPServer(loop_, inPort, outPort, channelUtf8, argc, argv);
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
}

void LoadSettings(HWND hDlg, UINT _IDC_EDIT_PORTIN, UINT _IDC_EDIT_PORTOUT, UINT _IDC_EDIT_PORTNAME)
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
}


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
