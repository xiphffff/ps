// Copyright 2019 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <Windows.h>
#include "window.h"

Window::Window() noexcept
{
    setup(nullptr);
}

Window::Window(const Window& parent) noexcept
{
    setup(parent.get_hwnd());
}

Window::~Window() noexcept
{ }

// Initializes the window.
void Window::setup(const HWND parent)
{
    HINSTANCE hInstance;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, nullptr, &hInstance);

    WNDCLASSEX wnd_class;

    if (!GetClassInfoEx(hInstance, L"winapisucks", &wnd_class))
    {
        wnd_class.cbSize = sizeof(WNDCLASSEX);
        wnd_class.style = CS_VREDRAW | CS_HREDRAW;

        wnd_class.lpfnWndProc = [](HWND m_hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
        };

        wnd_class.cbClsExtra = 0;
        wnd_class.cbWndExtra = 0;

        wnd_class.hInstance = hInstance;

        wnd_class.hIcon = nullptr;

        wnd_class.hCursor =
        reinterpret_cast<HCURSOR>(LoadImage(hInstance,
                                            MAKEINTRESOURCE(OCR_NORMAL),
                                            IMAGE_CURSOR,
                                            0,
                                            0,
                                            LR_DEFAULTSIZE));

        wnd_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        wnd_class.lpszMenuName = nullptr;
        wnd_class.lpszClassName = L"winapisucks";
        wnd_class.hIconSm = nullptr;

        if (!RegisterClassEx(&wnd_class))
        {
            return;
        }
    }

    hwnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
                          L"winapisucks",
                          nullptr,
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          parent,
                          nullptr,
                          hInstance,
                          this);

    if (hwnd == nullptr)
    {
        return;
    }
}

// Sets the title of the window to `title`.
void Window::set_title(const std::wstring& title) const noexcept
{
    SetWindowText(hwnd, title.c_str());
}

// Sets the size of the window to `width` pixels wide and `height` pixels high.
void Window::set_size(const int width, const int height) const noexcept
{
    SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE);
}

// Sets the position of the window to (x, y) on the current monitor.
void Window::set_position(const int x, const int y) const noexcept
{
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE);
}

// Sets the window's visibility state to `state`.
void Window::show(const int state) const noexcept
{
    ShowWindow(hwnd, state);
}

// Sets the position of the window to a specific position on the current
// monitor.
void Window::set_position(const WindowPosition pos) const noexcept
{
    RECT rect;
    GetWindowRect(hwnd, &rect);

    switch (pos)
    {
        case WindowPosition::Center:
        {
            int x_pos = (GetSystemMetrics(SM_CXSCREEN) - (rect.right  - rect.left)) / 2;
            int y_pos = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;

            set_position(x_pos, y_pos);
            break;
        }
    }
}