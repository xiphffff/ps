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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <fmt/printf.h>
#include "glad.h"
#include <GLFW/glfw3.h>
#include "disasm.h"
#include "../psemu/include/psemu.h"
#include "../psemu/utility/memory.h"

// The file to write disassembler output/debug messages to.
static std::ofstream debug_file;

// The file to write TTY output to.
static std::ofstream tty_file;

// The CD-ROM image, if any.
static std::ifstream cdrom_image;

// `true` if we're to start tracing, or `false` otherwise.
static bool tracing = false;

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
} static opengl_state;

static void GLAPIENTRY opengl_debug_output(GLenum source,
                                           GLenum type,
                                           GLuint id,
                                           GLenum severity,
                                           GLsizei length,
                                           const GLchar* message,
                                           const void* userParam)
{
    const auto str
    {
        fmt::sprintf
        ("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
         type, severity, message)
    };

    std::cout << str << std::endl;

    if (tracing)
    {
        debug_file << str << std::endl;
    }
}

// Called whenever the window resizes for any reason.
static void window_resized(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Called whenever the window has detected a change in key state.
static void key_state_changed
(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_T && action == GLFW_PRESS)
    {
        tracing = !tracing;
    }
}

// Reads a sector from absolute address `address` into `sector_data`.
static void cdrom_read(const unsigned int address,
                       uint8_t* const sector_data) noexcept
{
    cdrom_image.seekg(address, cdrom_image.beg);

    // I don't think this is the right way to handle this.
    cdrom_image.read(reinterpret_cast<char*>(sector_data),
                     PSEMU_CDROM_SECTOR_SIZE);
}

static void setup_shaders() noexcept
{
    const auto vertex_shader{ glCreateShader(GL_VERTEX_SHADER) };
    const auto fragment_shader{ glCreateShader(GL_FRAGMENT_SHADER) };

    const auto vs_src{ R"vs(
        #version 460 core
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 texcoord;
        out vec2 final_texcoord;

        void main()
        {
            final_texcoord = texcoord;
            gl_Position = vec4(position, 0.0, 1.0);
        }
        )vs" };

    const auto fs_src{ R"fs(
        #version 460 core
        out vec4 frag_color;
        in vec2 final_texcoord;

        uniform sampler2D gpu_texture;

        void main()
        {
            frag_color = texture(gpu_texture, final_texcoord);
        }
        )fs" };

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

static void setup_texture() noexcept
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

// Called when `std_out_putchar` has been called by the BIOS.
static void handle_tty_string(const struct psemu_system* const ps_emu)
{
    assert(ps_emu != NULL);

    const auto c{ static_cast<char>(ps_emu->cpu.gpr[4]) };

    std::cout << c;
    tty_file  << c;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << fmt::sprintf("%s: Missing required argument.", argv[0]) << std::endl;
        std::cerr << fmt::sprintf("Syntax: %s bios_file [exe_file] [cdrom_image]", argv[0]) << std::endl;

        return EXIT_FAILURE;
    }

    bool run_cdrom = true;
    bool inject_exe = false;

    if (argc == 2)
    {
        inject_exe = true;
    }

    if (argc == 3)
    {
        inject_exe = false;
        run_cdrom = true;
    }

    std::ifstream bios_file{ argv[1], std::ios::binary };
    uint8_t bios_data[0x80000];
    std::copy(std::istreambuf_iterator<char>{bios_file}, std::istreambuf_iterator<char>{}, bios_data);
    bios_file.close();

    auto ps_emu{ psemu_create(bios_data) };

    psemu_log_set_cb([](const char* const msg)
    {
        debug_file << msg << std::endl;
    });

    if (run_cdrom)
    {
        cdrom_image = std::ifstream(argv[2], std::ios::binary);
        psemu_set_cdrom(ps_emu, &cdrom_read);
    }
    bool running = true;

    // Set to `true` if we're to stop emulation on any exception raised, or
    // `false` otherwise. This is important to run test suites that verify CPU
    // behavior. A Reserved Instruction exception (RI) will always stop
    // emulation regardless of this value.
    constexpr bool break_on_exception{ true };

    debug_file = std::ofstream("debug.txt", std::ios::trunc);
    tty_file   = std::ofstream("tty.txt",   std::ios::trunc);

    if (!glfwInit())
    {
        fprintf(stderr, "%s: glfwInit() failed.\n", argv[0]);
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    auto window{ glfwCreateWindow(PSEMU_GPU_VRAM_WIDTH,
                                  PSEMU_GPU_VRAM_HEIGHT,
                                  "psemu",
                                  nullptr,
                                  nullptr) };

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, window_resized);
    glfwSetKeyCallback(window, key_state_changed);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "gladLoadGLLoader() failed.\n");
        return EXIT_FAILURE;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(opengl_debug_output, nullptr);

    setup_shaders();
    setup_texture();

    auto previous_time{ glfwGetTime() };
    auto fps{ 0 };

    while (running)
    {
        auto current_time{ glfwGetTime() };
        fps++;

        if ((current_time - previous_time) >= 1.0)
        {
            const auto title
            {
                fmt::sprintf("psemu: [FPS: %d (%f ms)]",
                             fps,
                             1000.0 / static_cast<double>(fps))
            };

            glfwSetWindowTitle(window, title.c_str());

            fps = 0;
            previous_time += 1.0;
        }

        if (glfwWindowShouldClose(window))
        {
            running = false;
            break;
        }

        for (auto cycle{ 0 }; cycle < 33868800 / 60; ++cycle)
        {
            if (ps_emu->cpu.pc == 0x80000080)
            {
                const auto exc_code
                {
                    (ps_emu->cpu.cop0[PSEMU_CPU_COP0_Cause] >> 2) & 0x0000001F
                };

                if (exc_code != PSEMU_CPU_EXCCODE_Sys &&
                    exc_code != PSEMU_CPU_EXCCODE_Int)
                {
                    // A reserved instruction exception will always halt
                    // emulation, regardless of the `break_on_exception`
                    // value.
                    if (exc_code == PSEMU_CPU_EXCCODE_RI)
                    {
                        debug_file << "Reserved instruction (RI) raised. "
                                      "Emulation halted." << std::endl;

                        running = false;
                        break;
                    }

                    std::string exception;

                    switch (exc_code)
                    {
                        case PSEMU_CPU_EXCCODE_AdEL:
                            exception = "Address error load exception (AdEL)";
                            break;

                        case PSEMU_CPU_EXCCODE_AdES:
                            exception = "Address error store exception (AdES)";
                            break;

                        case PSEMU_CPU_EXCCODE_Bp:
                            exception = "Breakpoint exception (Bp)";
                            break;

                        case PSEMU_CPU_EXCCODE_Ov:
                            exception = "Arithmetic overflow exception (Ov)";
                            break;
                    }

                    if constexpr (break_on_exception)
                    {
                        debug_file <<
                        exception << "raised. Emulation halted." << std::endl;
 
                        running = false;
                        break;
                    }

                    debug_file <<
                    exception << "raised. Emulation continuing." << std::endl;
                }
            }

            if ((ps_emu->cpu.pc == 0x80030000) && inject_exe)
            {
                debug_file << "Injecting EXE: " << argv[2] << std::endl;

                std::ifstream exe_file{ argv[2], std::ios::binary };
                const auto exe_size{ std::filesystem::file_size(argv[2]) };

                auto exe_data{ new uint8_t[exe_size] };
                std::copy(std::istreambuf_iterator<char>{exe_file}, std::istreambuf_iterator<char>{}, exe_data);

                exe_file.close();

                auto dest{ *(uint32_t *)&exe_data[0x10] };

                for (auto ptr{ 0x800 }; ptr != (exe_size - 0x800); ++ptr)
                {
                    *(uint32_t *)&ps_emu->bus.ram[dest++ & 0x1FFFFFFF] =
                    exe_data[ptr];
                }

                ps_emu->cpu.pc      = *(uint32_t *)&exe_data[0x18];
                ps_emu->cpu.next_pc = ps_emu->cpu.pc;

                ps_emu->cpu.instruction.word =
                psemu_bus_load_word(&ps_emu->bus, ps_emu->cpu.pc);

                delete[] exe_data;
            }

            if (ps_emu->cpu.pc == 0x000000A0)
            {
                switch (ps_emu->cpu.gpr[9])
                {
                    case 0x3C:
                        handle_tty_string(ps_emu);
                        break;

                    case 0x40:
                    {
                        const std::string str
                        {
                            "SystemErrrorUnresolvedException() reached. "
                            "Emulation halted."
                        };

                        debug_file << str << std::endl;
                        std::cout << str << std::endl;
                            
                        running = false;
                        break;
                    }
                }
            }

            if (ps_emu->cpu.pc == 0x000000B0)
            {
                switch (ps_emu->cpu.gpr[9])
                {
                    case 0x3D:
                        handle_tty_string(ps_emu);
                        break;
                }
            }

            if (tracing)
            {
                disassemble_before(&ps_emu->cpu);
            }

            psemu_step(ps_emu);

            if (tracing)
            {
                debug_file << disassemble_after() << std::endl;
            }
        }
        psemu_vblank(ps_emu);

        glClear(GL_COLOR_BUFFER_BIT);

        glTexSubImage2D
        (GL_TEXTURE_2D,
         0,
         0,
         0,
         PSEMU_GPU_VRAM_WIDTH,
         PSEMU_GPU_VRAM_HEIGHT,
         GL_RGBA,
         GL_UNSIGNED_SHORT_1_5_5_5_REV,
         ps_emu->bus.gpu.vram);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    psemu_destroy(ps_emu);

    tty_file.flush();
    debug_file.flush();

    tty_file.close();
    debug_file.close();

    return EXIT_SUCCESS;
}