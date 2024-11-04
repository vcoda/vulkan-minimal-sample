#include "win32App.h"
#include <memory>
#include <vector>
#include <cassert>

static LPCTSTR ClassName = TEXT("Vulkan");

Win32App *Win32App::self;

Win32App::Win32App(const Entry& entry, LPCTSTR caption, uint32_t width_, uint32_t height_):
    width(width_),
    height(height_),
    fullscreen(false),
    hInstance(entry.hInstance),
    hWnd(NULL)
{
    Win32App::self = this;
    HICON icon = (HICON)LoadImage(NULL, TEXT("resources\\vulkan.ico"),
        IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    const WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        Win32App::wndProc,
        0, 0,
        entry.hInstance,
        icon,
        LoadCursor(NULL, IDC_ARROW),
        NULL, NULL, ClassName,
        icon
    };
    RegisterClassEx(&wc);
    HWND hDesktopWnd = GetDesktopWindow();
    RECT rcDesktop;
    GetWindowRect(hDesktopWnd, &rcDesktop);
    DWORD style;
    if (width >= (uint32_t)rcDesktop.right ||
        height >= (uint32_t)rcDesktop.bottom)
    {
        width = (uint32_t)rcDesktop.right;
        height = (uint32_t)rcDesktop.bottom;
        style = WS_POPUP;
        fullscreen = true;
    }
    else
    {
        style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }
    hWnd = CreateWindow(wc.lpszClassName, caption, style,
        0, 0, width, height,
        NULL, NULL, wc.hInstance, NULL);
    SetWindowText(hWnd, caption);

    RECT rc = {0L, 0L, (LONG)width, (LONG)height};
    AdjustWindowRect(&rc, style, FALSE);
    SetWindowPos(hWnd, HWND_TOP, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_HIDEWINDOW);
}

Win32App::~Win32App()
{
    DestroyWindow(hWnd);
    UnregisterClass(ClassName, hInstance);
}

void Win32App::show() const
{
    if (fullscreen)
    {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, width, height, SWP_SHOWWINDOW);
    }
    else
    {
        const HWND hDesktopWnd = GetDesktopWindow();
        RECT rcDesktop;
        GetWindowRect(hDesktopWnd, &rcDesktop);
        RECT rc;
        GetWindowRect(hWnd, &rc);
        const int cx = static_cast<int>(rc.right - rc.left);
        const int cy = static_cast<int>(rc.bottom - rc.top);
        int x = 0, y = 0;
        if (cx < rcDesktop.right && cy < rcDesktop.bottom)
        {
            x = (rcDesktop.right - cx) / 2;
            y = (rcDesktop.bottom - cy) / 2;
        }
        SetWindowPos(hWnd, HWND_TOP, x, y, cx, cy, SWP_SHOWWINDOW);
    }
    ShowCursor(FALSE);
}

void Win32App::run()
{
    while (!quit)
    {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (!IsIconic(hWnd))
                onIdle();
        }
    }
}

void Win32App::close()
{
    quit = true;
}

void Win32App::onIdle()
{
    onPaint();
}

void Win32App::onPaint()
{
}

void Win32App::onKeyDown(int key, int repeat, uint32_t flags)
{
    if (VK_ESCAPE == key)
        close();
}

LRESULT WINAPI Win32App::wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        {
            self->onKeyDown((int)wParam, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam));
        }
        break;
    case WM_KEYUP:
        {
            self->onKeyUp((int)wParam, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam));
        }
        break;
#ifndef _DEBUG
    case WM_PAINT:
        Win32App::self->onPaint();
        break;
#endif
    case WM_CLOSE:
        self->close();
        break;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

std::unique_ptr<Win32App> appFactory(const Win32App::Entry&);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    Win32App::Entry entry;
    entry.hInstance = hInstance;
    entry.hPrevInstance = hPrevInstance;
    entry.lpCmdLine = pCmdLine;
    entry.nCmdShow = nCmdShow;
    std::unique_ptr<Win32App> win32App;
    try
    {
        win32App = appFactory(entry);
        win32App->show();
        win32App->run();
    }
    catch (const std::exception& exc)
    {
        MessageBoxA(NULL, exc.what(), "Error", MB_ICONERROR);
    }
    return 0;
}
