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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad.h"
#include <GLFW/glfw3.h>
#include "disasm.h"
#include "../psemu/include/psemu.h"
#include "../psemu/utility/memory.h"

// The file to write disassembler output/debug messages to.
static FILE* debug_file = NULL;

// The file to write TTY output to.
static FILE* tty_file = NULL;

// The CD-ROM image, if any.
static FILE* cdrom_image = NULL;

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

void GLAPIENTRY opengl_debug_output(GLenum source,
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

    printf("%s\n", buf);

    if (tracing)
    {
        fprintf(debug_file, "%s\n", buf);
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
static void cdrom_read(const unsigned int address, uint8_t* const sector_data)
{
    fseek(cdrom_image, address, SEEK_SET);
    fread(sector_data, PSEMU_CDROM_SECTOR_SIZE, 1, cdrom_image);
}

static void setup_shaders(void)
{
    const GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    const char* const vs_src =
    "#version 460 core\n"
    "layout (location = 0) in vec2 position;\n"
    "layout (location = 1) in vec2 texcoord;\n"
    "out vec2 final_texcoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    final_texcoord = texcoord;\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n\0";

    const char* const fs_src =
    "#version 460 core\n"
    "out vec4 frag_color;\n"
    "\n"
    "in vec2 final_texcoord;\n"
    "\n"
    "uniform sampler2D gpu_texture;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    frag_color = texture(gpu_texture, final_texcoord);\n"
    "}\n\0";

    glShaderSource(vertex_shader, 1, &vs_src, NULL);
    glShaderSource(fragment_shader, 1, &fs_src, NULL);

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

static void setup_texture(void)
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

    static char tty_str[256];
    strcat(tty_str, (char*)&ps_emu->cpu.gpr[4]);

    if (ps_emu->cpu.gpr[4] == '\n')
    {
        fprintf(tty_file, "%s", tty_str);
        fflush(tty_file);

        printf("%s", tty_str);
        memset(tty_str, 0, 256);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "%s: Missing required argument.\n", argv[0]);
        fprintf(stderr, "Syntax: %s bios_file [exe_file] [cdrom_image]\n", argv[0]);

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

    FILE* bios_file = fopen(argv[1], "rb");
    uint8_t bios_data[0x80000];
    fread(bios_data, 1, 0x80000, bios_file);
    fclose(bios_file);

    struct psemu_system* ps_emu = psemu_create(bios_data);

    if (run_cdrom)
    {
        cdrom_image = fopen(argv[3], "rb");
        psemu_set_cdrom(ps_emu, &cdrom_read);
    }
    bool running = true;

    // Set to `true` if we're to stop emulation on any exception raised, or
    // `false` otherwise. This is important to run test suites that verify CPU
    // behavior. A Reserved Instruction exception (RI) will always stop
    // emulation regardless of this value.
    const bool break_on_exception = false;

    debug_file = fopen("debug.txt", "w");
    tty_file = fopen("tty.txt", "w");

    if (!glfwInit())
    {
        fprintf(stderr, "%s: glfwInit() failed.\n", argv[0]);
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(PSEMU_GPU_VRAM_WIDTH,
                                          PSEMU_GPU_VRAM_HEIGHT,
                                          "psemu",
                                          NULL,
                                          NULL);

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
    glDebugMessageCallback(opengl_debug_output, NULL);

    setup_shaders();
    setup_texture();

    double previous_time = glfwGetTime();
    int fps = 0;

    while (running)
    {
        double current_time = glfwGetTime();
        fps++;

        if ((current_time - previous_time) >= 1.0)
        {
            char buf[32];
            sprintf(buf, "psemu [FPS: %d (%f ms)]", fps, 1000.0 / (double)fps);

            glfwSetWindowTitle(window, buf);

            fps = 0;
            previous_time += 1.0;
        }

        if (glfwWindowShouldClose(window))
        {
            running = false;
            break;
        }

        for (unsigned int cycle = 0; cycle < 33868800 / 60; ++cycle)
        {
            if (ps_emu->cpu.pc == 0x80000080)
            {
                const unsigned int exc_code =
                (ps_emu->cpu.cop0[PSEMU_CPU_COP0_Cause] >> 2) & 0x0000001F;

                if (exc_code != PSEMU_CPU_EXCCODE_Sys &&
                    exc_code != PSEMU_CPU_EXCCODE_Int)
                {
                    // A reserved instruction exception will always halt
                    // emulation, regardless of the `break_on_exception`
                    // value.
                    if (exc_code == PSEMU_CPU_EXCCODE_RI)
                    {
                        fprintf(debug_file,
                                "Reserved instruction exception (RI) raised. "
                                "Emulation halted.\n");

                        running = false;
                        break;
                    }

                    char exception[256];

                    switch (exc_code)
                    {
                        case PSEMU_CPU_EXCCODE_AdEL:
                            sprintf(exception,
                                    "Address error load exception (AdEL)");
                            break;

                        case PSEMU_CPU_EXCCODE_AdES:
                            sprintf(exception,
                                    "Address error store exception (AdES)");
                            break;

                        case PSEMU_CPU_EXCCODE_Bp:
                            sprintf(exception, "Breakpoint exception (Bp)");
                            break;

                        case PSEMU_CPU_EXCCODE_Ov:
                            sprintf(exception,
                                    "Arithmetic overflow exception (Ov)");
                            break;
                    }

                    if (break_on_exception)
                    {
                        fprintf(debug_file,
                                "%s raised. Emulation halted.\n", exception);

                        running = false;
                        break;
                    }

                    fprintf(debug_file,
                            "%s raised. Emulation continuing.\n", exception);
                }
            }

            if ((ps_emu->cpu.pc == 0x80030000) && inject_exe)
            {
                fprintf(debug_file, "Injecting EXE: %s\n", argv[2]);

                FILE* exe_file = fopen(argv[2], "rb");

                // Ew.
                fseek(exe_file, 0, SEEK_END);
                const long exe_size = ftell(exe_file);
                rewind(exe_file);

                uint8_t* exe_data = psemu_safe_malloc(exe_size);
                fread(exe_data, 1, exe_size, exe_file);
                fclose(exe_file);

                uint32_t dest = *(uint32_t*)&exe_data[0x10];

                for (unsigned int ptr = 0x800;
                    ptr != (exe_size - 0x800);
                    ++ptr)
                {
                    *(uint32_t*)&ps_emu->bus.ram[dest++ & 0x1FFFFFFF] =
                    exe_data[ptr];
                }

                ps_emu->cpu.pc      = *(uint32_t*)&exe_data[0x18];
                ps_emu->cpu.next_pc = ps_emu->cpu.pc;

                ps_emu->cpu.instruction.word =
                psemu_bus_load_word(&ps_emu->bus, ps_emu->cpu.pc);

                psemu_safe_free(exe_data);
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
                        const char* msg =
                        "SystemErrrorUnresolvedException() reached. Emulation "
                        "halted.\n";

                        fprintf(debug_file, "%s", msg);
                        printf("%s", msg);
                            
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
                fprintf(debug_file, "%s\n", disassemble_after());
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

    fflush(tty_file);
    fflush(debug_file);

    fclose(tty_file);
    fclose(debug_file);

    return EXIT_SUCCESS;
}