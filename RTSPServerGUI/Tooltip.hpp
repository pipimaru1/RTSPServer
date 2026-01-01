#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <commdlg.h>   // GetOpenFileName

// ------------------------------
// ToolTip
// ------------------------------

inline HINSTANCE g_hInst = nullptr;

class Tooltip
{
    std::wstring title;
    HWND      hnd_ToolTip = nullptr;

public:
    void EnsureTooltip(HWND hDlg) {
        static bool s_inited = [] {
            INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES };
            InitCommonControlsEx(&icc);
            return true;
            }();

        if (hnd_ToolTip) return;

        hnd_ToolTip = CreateWindowExW(
            WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hDlg, nullptr, g_hInst, nullptr);
        if (!hnd_ToolTip) return;

        // 折返し・遅延
        SendMessageW(hnd_ToolTip, TTM_SETMAXTIPWIDTH, 0, 480);
        SendMessageW(hnd_ToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 200);  // ホバー開始まで（ms）
        SendMessageW(hnd_ToolTip, TTM_SETDELAYTIME, TTDT_RESHOW, 50);    // 再表示まで
        SendMessageW(hnd_ToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 8000); // 自動で消えるまで

        // バルーンのタイトル（任意）
        SendMessageW(hnd_ToolTip, TTM_SETTITLE, TTI_INFO, (LPARAM)title.c_str());
    }

    // マウスオーバーでのツールチップ表示
    // 1) 追加：ホバー用ツールを登録するヘルパ
    void AddHoverTooltipForCtrl(HWND hDlg, int ctrlId, const wchar_t* _title, const wchar_t* _text)
    {
        title = _title ? _title : L""; // タイトルを保存（必要なら後で使う）

        EnsureTooltip(hDlg); // ツールチップ作成＆各種設定
        HWND hCtrl = GetDlgItem(hDlg, ctrlId);
        TOOLINFOW ti{};
        // v6 マニフェストが無い環境に配慮して V1 サイズを使う（v6 なら sizeof(TTTOOLINFOW) でもOK）
        ti.cbSize = TTTOOLINFOW_V1_SIZE;
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND /*| TTF_TRANSPARENT*/;
        ti.hwnd = hDlg;                 // 親（ダイアログ）
        ti.uId = (UINT_PTR)hCtrl;      // ツール＝このコントロール HWND
        ti.lpszText = const_cast<LPWSTR>(_text);

        SendMessageW(hnd_ToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
};

#pragma comment(lib, "Comctl32.lib")
//#pragma comment(lib, "Ole32.lib")
//#pragma comment(lib, "Shell32.lib")
//#pragma comment(lib, "Shlwapi.lib")
//#pragma comment(lib, "Uuid.lib")
//#pragma comment(lib, "Comctl32.lib")


//使い方
// ① ツールチップインスタンスを作成
// 変数として 例 Tooltip _TP 等を宣言
// グローバル変数として宣言しておくのを推奨
// ダイアログボックス、ウィンドウ毎にインスタンスを作成する
// ウィンドウ毎にイベントが発生するため
// 
// ➁ ダイアログの WM_INITDIALOG で EnsureTooltip を呼ばなくてもいい
// 
// ③ 各コントロールにツールチップを追加
// 例 _TP.AddHoverTooltipForCtrl(hDlg, IDC_BUTTON1, L"Button1", L"このボタンは…");
//
// ④ ダイアログの WM_DESTROY で hnd_ToolTip を破棄
