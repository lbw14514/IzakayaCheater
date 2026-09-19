#pragma once

#include <windows.h>

namespace WebViewHost
{
    bool Create(HWND parent, const wchar_t* url);
    void Resize();
    void Reload();
    void Destroy();
}
