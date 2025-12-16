#pragma once

#include "resource.h"

struct _GMainLoop;
typedef struct _GMainLoop GMainLoop;

class RtspServerController
{
public:
    RtspServerController();
    ~RtspServerController();

    bool Start(int inPort, int outPort, const std::string& channelUtf8, HWND hwndNotify);
    void Stop();

    bool IsRunning() const { return running_.load(); }

private:
    void ThreadMain(int inPort, int outPort, std::string channelUtf8);

private:
    std::atomic<bool> running_{ false };
    std::thread       th_;
    GMainLoop* loop_{ nullptr };
    HWND              hwndNotify_{ nullptr };
};


/////////////////////////////////////////
//
// ÉwÉãÉpÅ[ä÷êî
//
////////////////////////////////////////
std::wstring GetIniPath();
//void SaveSettings(HWND hDlg);
void SaveSettings(HWND hDlg, UINT _IDC_EDIT_PORTIN, UINT _IDC_EDIT_PORTOUT, UINT _IDC_EDIT_PORTNAME);
//void LoadSettings(HWND hDlg);
void LoadSettings(HWND hDlg, UINT _IDC_EDIT_PORTIN, UINT _IDC_EDIT_PORTOUT, UINT _IDC_EDIT_PORTNAME);
//void SetRunningUi(HWND hDlg, bool running);
void SetRunningUi(HWND hDlg, bool running,
    UINT _IDC_BTN_START,
    UINT _IDC_BTN_STOP,
    UINT _IDC_EDIT_PORTIN,
    UINT _IDC_EDIT_PORTOUT,
    UINT _IDC_EDIT_PORTNAME
);
std::string WideToUtf8(const std::wstring& ws);
bool GetIntFromEdit(HWND hDlg, int id, int& outValue);