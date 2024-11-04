#pragma once
#include <cstdint>
#include <windows.h>

class Win32App
{
public:
    struct Entry;
    Win32App(const Entry& entry, LPCTSTR caption, uint32_t width, uint32_t height);
    virtual ~Win32App();
    void show() const;
    void run();
    virtual void close();
    virtual void onIdle();
    virtual void onPaint();
    virtual void onKeyUp(int key, int repeat, uint32_t flags) {}
    virtual void onKeyDown(int  key, int repeat, uint32_t flags);
    virtual void onRawMouseMove(long dx, long dy) {}
    virtual void onRawMouseWheel(float z) {}

protected:
    HINSTANCE hInstance;
    HWND hWnd;
    uint32_t width, height;
    bool fullscreen;

private:
    static LRESULT WINAPI wndProc(HWND, UINT, WPARAM, LPARAM);

    static Win32App *self;
    bool quit = false;
};

struct Win32App::Entry
{
    HINSTANCE hInstance;
    HINSTANCE hPrevInstance;
    LPSTR lpCmdLine;
    int nCmdShow;
};
