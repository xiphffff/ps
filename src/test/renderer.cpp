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

#include <QDebug>
#include "renderer.h"
#include "../psemu/include/gpu.h"

Renderer::Renderer(QWidget* parent, uint16_t* const m_vram_ptr) noexcept :
QOpenGLWidget(parent),
vram(m_vram_ptr)
{ }

Renderer::~Renderer() noexcept
{ }

void Renderer::initializeGL()
{
    initializeOpenGLFunctions();

    setup_shaders();
    setup_texture();

    glDebugMessageCallback([](GLenum source,
                              GLenum type,
                              GLuint id,
                              GLenum severity,
                              GLsizei length,
                              const GLchar* message,
                              const void* userParam)
    {
        char buf[1024];

        sprintf(buf, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s",
                     (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                     type, severity, message);

        qDebug() << buf;
    }, this);
}

void Renderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, opengl_state.texture);

    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    PSEMU_GPU_VRAM_WIDTH,
                    PSEMU_GPU_VRAM_HEIGHT,
                    GL_RGBA,
                    GL_UNSIGNED_SHORT_1_5_5_5_REV,
                    vram);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Renderer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void Renderer::setup_shaders() noexcept
{
    const auto vertex_shader  { glCreateShader(GL_VERTEX_SHADER)   };
    const auto fragment_shader{ glCreateShader(GL_FRAGMENT_SHADER) };

    const auto vs_src
    {
        R"vs(
        #version 450 core
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 texcoord;
        out vec2 final_texcoord;
        void main()
        {
            final_texcoord = texcoord;
            gl_Position = vec4(position, 0.0, 1.0);
        }
        )vs"
    };

    const auto fs_src
    {
        R"fs(
        #version 450 core
        out vec4 frag_color;
        in vec2 final_texcoord;
        uniform sampler2D gpu_texture;
        void main()
        {
            frag_color = texture(gpu_texture, final_texcoord);
        }
        )fs"
    };

    glShaderSource(vertex_shader,   1, &vs_src, nullptr);
    glShaderSource(fragment_shader, 1, &fs_src, nullptr);

    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    opengl_state.shader_program = glCreateProgram();
    glAttachShader(opengl_state.shader_program, vertex_shader);
    glAttachShader(opengl_state.shader_program, fragment_shader);
    glLinkProgram(opengl_state.shader_program);
    glUseProgram(opengl_state.shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

void Renderer::setup_texture() noexcept
{
    glGenTextures(1, &opengl_state.texture);
    glBindTexture(GL_TEXTURE_2D, opengl_state.texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexStorage2D
    (GL_TEXTURE_2D, 1, GL_RGB5_A1, PSEMU_GPU_VRAM_WIDTH, PSEMU_GPU_VRAM_HEIGHT);

    const GLfloat vertices[] =
    {
        // Position      Texcoords
           -1.0F,  1.0F, 0.0F, 0.0F, // Top-left
            1.0F,  1.0F, 1.0F, 0.0F, // Top-right
            1.0F, -1.0F, 1.0F, 1.0F, // Bottom-right
           -1.0F, -1.0F, 0.0F, 1.0F  // Bottom-left
    };

    const GLuint indices[] =
    {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &opengl_state.vao);
    glGenBuffers(1, &opengl_state.vbo);
    glGenBuffers(1, &opengl_state.ebo);

    glBindVertexArray(opengl_state.vao);

    glBindBuffer(GL_ARRAY_BUFFER, opengl_state.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_state.ebo);

    glBufferData
    (GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer
    (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer
    (1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
}