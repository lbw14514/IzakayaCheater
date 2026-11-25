#include "main_thread.h"
#include "log.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
const UINT WM_CORE_TASK = WM_APP + 0x51;

struct Task
{
    std::function<void()> fn;
    std::mutex lock;
    std::condition_variable cv;
    bool done;
    bool cancelled;
    bool started;

    Task() : done(false), cancelled(false), started(false) {}
};

HWND g_hwnd = NULL;
WNDPROC g_oldProc = NULL;
std::mutex g_queueLock;
std::vector<std::shared_ptr<Task>> g_tasks;
std::atomic<bool> g_busy(false);

struct FindContext
{
    DWORD pid;
    HWND unity;
    HWND any;
};

BOOL CALLBACK FindWindowProc(HWND hwnd, LPARAM lParam)
{
    FindContext* ctx = (FindContext*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t cls[128] = {0};
    GetClassNameW(hwnd, cls, 127);
    bool isUnity = _wcsicmp(cls, L"UnityWndClass") == 0;
    if (isUnity) {
        ctx->unity = hwnd;
        return FALSE;
    }
    if (!ctx->any && GetWindowTextLengthW(hwnd) > 0) ctx->any = hwnd;
    return TRUE;
}

LRESULT CALLBACK CoreWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CORE_TASK) {
        std::vector<std::shared_ptr<Task>> tasks;
        {
            std::lock_guard<std::mutex> guard(g_queueLock);
            tasks.swap(g_tasks);
        }
        for (size_t i = 0; i < tasks.size(); i++) {
            std::shared_ptr<Task> task = tasks[i];
            {
                std::lock_guard<std::mutex> guard(task->lock);
                if (task->cancelled) {
                    task->done = true;
                    task->cv.notify_all();
                    continue;
                }
                task->started = true;
            }
            g_busy = true;
            try {
                task->fn();
            } catch (...) {
                CoreLog("main thread task threw");
            }
            g_busy = false;
            {
                std::lock_guard<std::mutex> guard(task->lock);
                task->done = true;
                task->cv.notify_all();
            }
        }
        return 0;
    }
    return CallWindowProcW(g_oldProc, hwnd, msg, wParam, lParam);
}

bool RemoveTask(const std::shared_ptr<Task>& task)
{
    std::lock_guard<std::mutex> guard(g_queueLock);
    for (size_t i = 0; i < g_tasks.size(); i++) {
        if (g_tasks[i] == task) {
            g_tasks.erase(g_tasks.begin() + i);
            return true;
        }
    }
    return false;
}
}

HWND MainThread_Window()
{
    return g_hwnd;
}

bool MainThread_EnsureInstalled(std::string* error)
{
    if (g_hwnd && IsWindow(g_hwnd)) return true;

    if (g_hwnd && !IsWindow(g_hwnd)) {
        g_hwnd = NULL;
        g_oldProc = NULL;
    }

    FindContext ctx;
    ctx.pid = GetCurrentProcessId();
    ctx.unity = NULL;
    ctx.any = NULL;
    EnumWindows(FindWindowProc, (LPARAM)&ctx);

    HWND target = ctx.unity ? ctx.unity : ctx.any;
    if (!target) {
        if (error) *error = "未找到游戏窗口 等游戏进入主界面后重试";
        return false;
    }

    SetLastError(0);
    LONG_PTR old = SetWindowLongPtrW(target, GWLP_WNDPROC, (LONG_PTR)CoreWndProc);
    if (old == 0) {
        DWORD code = GetLastError();
        if (error) *error = "子类化游戏窗口失败 错误码 " + std::to_string(code);
        return false;
    }

    g_oldProc = (WNDPROC)old;
    g_hwnd = target;
    CoreLog("main thread hooked hwnd=%p", target);
    return true;
}

bool MainThread_Run(const std::function<void()>& fn, int timeoutMs, std::string* error)
{
    std::string installError;
    if (!MainThread_EnsureInstalled(&installError)) {
        if (error) *error = installError;
        return false;
    }
    if (g_busy.load()) {
        if (error) *error = "主线程正忙 请稍后重试";
        return false;
    }

    std::shared_ptr<Task> task = std::make_shared<Task>();
    task->fn = fn;
    {
        std::lock_guard<std::mutex> guard(g_queueLock);
        g_tasks.push_back(task);
    }

    if (!PostMessageW(g_hwnd, WM_CORE_TASK, 0, 0)) {
        RemoveTask(task);
        if (error) *error = "投递主线程消息失败 错误码 " + std::to_string(GetLastError());
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(task->lock);
        if (task->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() { return task->done; })) {
            return true;
        }
        if (!task->started) task->cancelled = true;
    }
    RemoveTask(task);

    if (error) *error = "主线程执行超时";
    return false;
}

void MainThread_Uninstall()
{
    if (g_hwnd && g_oldProc) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldProc);
        CoreLog("main thread unhooked hwnd=%p", g_hwnd);
    }
    g_hwnd = NULL;
    g_oldProc = NULL;
}
