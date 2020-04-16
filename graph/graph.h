/*
 * glauss is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glauss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glauss.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <GLES2/gl2.h>
#include "graph_sdl.h"

enum draw_mode {
    MODE_SPRITE,
    MODE_SPHERE,
    MODE_POINTS,
    MODE_POINTS_COL,
};

void graph_init(void);
void graph_quit(void);
void graph_reset_viewport(void);
void graph_set_view(graph_window *win);
GLuint graph_compile_shader(const char *src_vert_shader,
                                const char *src_frag_shader);
void graph_draw_scene(graph_window *win);
int graph_set_draw_mode(graph_window *win, const char *mode);
GLuint graph_load_c_png_texture(void *bin, size_t len, GLint *width, GLint *height);
GLuint graph_load_png_texture(const char *filename, GLint *width, GLint *height);
int graph_sshot(unsigned long long int total_steps);
