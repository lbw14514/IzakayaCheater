#include "webview_host.h"

#include <windows.h>
#include <shellapi.h>
#include <unknwn.h>
#include "WebView2.h"

#include <atomic>
#include <string>

namespace
{
ICoreWebView2Controller* g_controller = nullptr;
ICoreWebView2* g_webview = nullptr;
HWND g_hwnd = nullptr;
std::wstring g_url;
HMODULE g_loader = nullptr;

class EnvCallback : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
{
    public:
    EnvCallback() : m_ref(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler))
        {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG left = --m_ref;
        if (left == 0) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override;

    private:
    std::atomic<ULONG> m_ref;
};

class NewWindowHandler : public ICoreWebView2NewWindowRequestedEventHandler
{
    public:
    NewWindowHandler() : m_ref(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ICoreWebView2NewWindowRequestedEventHandler))
        {
            *ppv = static_cast<ICoreWebView2NewWindowRequestedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG left = --m_ref;
        if (left == 0) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) override
    {
        LPWSTR uri = nullptr;
        if (SUCCEEDED(args->get_Uri(&uri)) && uri)
        {
            ShellExecuteW(NULL, L"open", uri, NULL, NULL, SW_SHOWNORMAL);
            CoTaskMemFree(uri);
        }
        args->put_Handled(TRUE);
        return S_OK;
    }

    private:
    std::atomic<ULONG> m_ref;
};

class CtrlCallback : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
{
    public:
    CtrlCallback() : m_ref(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler))
        {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG left = --m_ref;
        if (left == 0) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override
    {
        if (SUCCEEDED(errorCode) && controller)
        {
            g_controller = controller;
            g_controller->AddRef();
            if (SUCCEEDED(g_controller->get_CoreWebView2(&g_webview)) && g_webview)
            {
                RECT rc;
                GetClientRect(g_hwnd, &rc);
                g_controller->put_Bounds(rc);
                g_controller->put_IsVisible(TRUE);

                NewWindowHandler* newWindow = new NewWindowHandler();
                EventRegistrationToken token;
                g_webview->add_NewWindowRequested(newWindow, &token);
                newWindow->Release();

                g_webview->Navigate(g_url.c_str());
            }
        }
        Release();
        return S_OK;
    }

    private:
    std::atomic<ULONG> m_ref;
};

HRESULT STDMETHODCALLTYPE EnvCallback::Invoke(HRESULT errorCode, ICoreWebView2Environment* env)
{
    if (SUCCEEDED(errorCode) && env)
        env->CreateCoreWebView2Controller(g_hwnd, new CtrlCallback());
    Release();
    return S_OK;
}
}

bool WebViewHost::Create(HWND parent, const wchar_t* url)
{
    g_hwnd = parent;
    g_url = url ? url : L"";

    g_loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!g_loader) return false;

    typedef HRESULT(STDAPICALLTYPE * CreateEnvFn)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                                 ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    CreateEnvFn create = (CreateEnvFn)GetProcAddress(g_loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!create) return false;

    HRESULT hr = create(nullptr, nullptr, nullptr, new EnvCallback());
    return SUCCEEDED(hr);
}

void WebViewHost::Resize()
{
    if (!g_controller) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    g_controller->put_Bounds(rc);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    GetDpiForWindowFn getDpi = (GetDpiForWindowFn)GetProcAddress(user32, "GetDpiForWindow");
    if (!getDpi) return;
    UINT dpi = getDpi(g_hwnd);
    if (dpi == 0) return;

    ICoreWebView2Controller3* controller3 = nullptr;
    if (SUCCEEDED(g_controller->QueryInterface(IID_ICoreWebView2Controller3, (void**)&controller3)) && controller3)
    {
        controller3->put_ShouldDetectMonitorScaleChanges(FALSE);
        controller3->put_RasterizationScale(dpi / 96.0);
        controller3->Release();
    }
}

void WebViewHost::Reload()
{
    if (g_webview) g_webview->Reload();
}

void WebViewHost::Destroy()
{
    if (g_controller)
    {
        g_controller->Close();
        g_controller->Release();
        g_controller = nullptr;
    }
    if (g_webview)
    {
        g_webview->Release();
        g_webview = nullptr;
    }
    if (g_loader)
    {
        FreeLibrary(g_loader);
        g_loader = nullptr;
    }
}
