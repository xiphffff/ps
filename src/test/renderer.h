// Copyright 2020 Michael Rodriguez
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

#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLWidget>

class Renderer : public QOpenGLWidget, protected QOpenGLFunctions_4_5_Core
{
    Q_OBJECT

public:
    explicit Renderer(QWidget* parent, uint16_t* const vram_ptr) noexcept;
    ~Renderer() noexcept;

private:
    // Defines the structure of the current OpenGL state.
    struct
    {
        // Texture ID
        GLuint texture;

        // Shader program
        GLuint shader_program;

        // Vertex array object
        GLuint vao;

        // Vertex buffer object
        GLuint vbo;

        // Element buffer object
        GLuint ebo;
    } opengl_state;

    void initializeGL();
    void paintGL();
    void resizeGL(int w, int h);

    void setup_shaders() noexcept;
    void setup_texture() noexcept;

    // Pointer to the VRAM data from the emulator
    uint16_t* vram;
};