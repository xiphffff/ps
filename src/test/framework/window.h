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

#pragma once

#include <string>
#include <Windows.h>

enum class WindowPosition : unsigned int
{
    Center
};

class Window
{
public:
    virtual ~Window() noexcept;

    inline HWND get_hwnd() const noexcept { return hwnd; }

    // Sets the title of the window to `title`.
    void set_title(const std::wstring& title) const noexcept;

    // Sets the size of the window to `width` pixels wide and `height` pixels
    // high.
    void set_size(const int width, const int height) const noexcept;

    // Sets the position of the window to (x, y) on the current monitor.
    void set_position(const int x, const int y) const noexcept;

    // Sets the window's visibility state to `state`.
    void show(const int state) const noexcept;

    // Sets the position of the window to a predefined position on the current
    // monitor.
    void set_position(const WindowPosition pos) const noexcept;

protected:
    Window() noexcept;
    Window(const Window& parent) noexcept;

private:
    // Initializes the window.
    void setup(const HWND hwnd);

    HWND hwnd;
};