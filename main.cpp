#include "web_server.h"
#include "webview_host.h"
#include <wx/wx.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/iconbndl.h>
#include <wx/image.h>
#include <wx/stdpaths.h>

#include <algorithm>
#include <cstdio>

static const int PREFERRED_PORT = 17845;
static const int HOTKEY_BASE = 2001;
static const char* kHotkeyFeatures[] = {
    "freeze_time", "freeze_day", "freeze_night", "max_combo", "lock_patience",
    "max_mood", "instant_cook", "perfect_qte", "infinite_exp", "money_multiplier",
    "pickup_multiplier", "exp_multiplier", "move_speed_multiplier", "game_speed"
};
static const int kHotkeyKeys[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    VK_OEM_MINUS, VK_OEM_PLUS, VK_OEM_PERIOD, VK_OEM_2
};
static const char* kHotkeyLabels[] = {
    "Ctrl+1", "Ctrl+2", "Ctrl+3", "Ctrl+4", "Ctrl+5", "Ctrl+6", "Ctrl+7",
    "Ctrl+8", "Ctrl+9", "Ctrl+0", "Ctrl+-", "Ctrl+=", "Ctrl+.", "Ctrl+/"
};
static const int kHotkeyCount = (int)(sizeof(kHotkeyFeatures) / sizeof(kHotkeyFeatures[0]));

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((void*)-4)
#endif

namespace
{
void EnableHighDpi()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;

    typedef BOOL(WINAPI * SetContextFn)(void*);
    SetContextFn setContext = (SetContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;

    typedef BOOL(WINAPI * SetAwareFn)();
    SetAwareFn setAware = (SetAwareFn)GetProcAddress(user32, "SetProcessDPIAware");
    if (setAware) setAware();
}

struct HighDpiInitializer
{
    HighDpiInitializer() { EnableHighDpi(); }
};

HighDpiInitializer g_highDpiInitializer;

double WindowScale(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return 1.0;
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    GetDpiForWindowFn getDpi = (GetDpiForWindowFn)GetProcAddress(user32, "GetDpiForWindow");
    if (!getDpi) return 1.0;
    UINT dpi = getDpi(hwnd);
    return dpi > 0 ? dpi / 96.0 : 1.0;
}
}

static wxString ExecutableDir()
{
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    return exe.GetPath();
}

class CheatFrame : public wxFrame
{
    public:
    CheatFrame(const wxString& url);
    void AdjustToScreen();

    private:
    void OnSize(wxSizeEvent& event);
    void OnDpiChanged(wxDPIChangedEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnHotKey(wxKeyEvent& event);
    void ApplyWindowIcon();
    wxPanel* m_panel;
};

CheatFrame::CheatFrame(const wxString& url)
    : wxFrame(NULL, wxID_ANY, wxString::FromUTF8("东方夜雀食堂修改器"), wxDefaultPosition, wxSize(1200, 830))
{
    m_panel = new wxPanel(this, wxID_ANY);
    m_panel->SetBackgroundColour(wxColour(245, 239, 230));

    ApplyWindowIcon();

    Bind(wxEVT_SIZE, &CheatFrame::OnSize, this);
    Bind(wxEVT_DPI_CHANGED, &CheatFrame::OnDpiChanged, this);
    Bind(wxEVT_CLOSE_WINDOW, &CheatFrame::OnClose, this);
    Bind(wxEVT_HOTKEY, &CheatFrame::OnHotKey, this);

    Centre();

    if (!WebViewHost::Create((HWND)m_panel->GetHWND(), url.wc_str()))
    {
        wxMessageBox(wxString::FromUTF8("无法加载 WebView2 界面。请确认 WebView2Loader.dll 与程序在同一目录，且系统已安装 WebView2 运行库。"),
                     wxString::FromUTF8("启动失败"), wxOK | wxICON_ERROR);
    }

    wxString failedHotkeys;
    for (int i = 0; i < kHotkeyCount; i++)
    {
        if (!RegisterHotKey(HOTKEY_BASE + i, wxMOD_CONTROL, kHotkeyKeys[i]))
        {
            failedHotkeys += wxString::FromUTF8(kHotkeyLabels[i]);
            failedHotkeys += " ";
        }
    }
    if (!failedHotkeys.IsEmpty())
    {
        CallAfter([failedHotkeys]() {
            wxMessageBox(wxString::FromUTF8("以下全局快捷键被其他程序占用，未生效：\n") + failedHotkeys,
                         wxString::FromUTF8("全局快捷键"), wxOK | wxICON_WARNING);
        });
    }
}

void CheatFrame::AdjustToScreen()
{
    RECT area;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0)) return;

    double scale = WindowScale((HWND)GetHWND());
    int maxWidth = (int)((area.right - area.left) * 0.92);
    int maxHeight = (int)((area.bottom - area.top) * 0.92);
    int width = std::min((int)(1180 * scale), maxWidth);
    int height = std::min((int)(840 * scale), maxHeight);

    SetWindowPos((HWND)GetHWND(), NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    Centre();
}

void CheatFrame::OnSize(wxSizeEvent& event)
{
    if (m_panel) m_panel->SetSize(GetClientSize());
    if (!IsIconized()) WebViewHost::Resize();
    event.Skip();
}

void CheatFrame::OnDpiChanged(wxDPIChangedEvent& event)
{
    if (m_panel) m_panel->SetSize(GetClientSize());
    WebViewHost::Resize();
    event.Skip();
}

void CheatFrame::OnClose(wxCloseEvent& event)
{
    for (int i = 0; i < kHotkeyCount; i++)
    {
        UnregisterHotKey(HOTKEY_BASE + i);
    }
    WebViewHost::Destroy();
    WebServer::Stop();
    event.Skip();
}

void CheatFrame::OnHotKey(wxKeyEvent& event)
{
    int index = event.GetId() - HOTKEY_BASE;
    if (index < 0 || index >= kHotkeyCount)
    {
        event.Skip();
        return;
    }
    char json[128] = {0};
    _snprintf(json, sizeof(json) - 1, "{\"type\":\"toggle-feature\",\"key\":\"%s\"}", kHotkeyFeatures[index]);
    WebViewHost::PostJson(json);
    event.Skip();
}

void CheatFrame::ApplyWindowIcon()
{
    wxImage img;
    wxString path = ExecutableDir() + wxFileName::GetPathSeparator() + "web" + wxFileName::GetPathSeparator() +
                    "assets" + wxFileName::GetPathSeparator() + "game-icon.png";
    if (!img.LoadFile(path, wxBITMAP_TYPE_PNG)) return;

    wxIconBundle bundle;
    static const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        wxImage scaled = img.Scale(sizes[i], sizes[i], wxIMAGE_QUALITY_HIGH);
        wxBitmap bitmap(scaled);
        wxIcon icon;
        icon.CopyFromBitmap(bitmap);
        bundle.AddIcon(icon);
    }
    SetIcons(bundle);
}

class MyApp : public wxApp
{
    public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    if (!WebServer::Start(PREFERRED_PORT))
    {
        wxMessageBox(wxString::FromUTF8("无法启动本地服务，端口可能被占用。"),
                     wxString::FromUTF8("启动失败"), wxOK | wxICON_ERROR);
        return false;
    }

    wxString url = wxString::Format("http://127.0.0.1:%d/", WebServer::Port());
    CheatFrame* frame = new CheatFrame(url);
    frame->Show();
    frame->AdjustToScreen();
    return true;
}