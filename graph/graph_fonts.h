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

#include "physics/physics.h"

const char *graph_init_fontconfig(void);
void graph_stop_fontconfig(void);
GLuint graph_init_freetype(const char *fontname);
void graph_stop_freetype(void);
void graph_display_text(const char *text, GLfloat x, GLfloat y, GLfloat s,
                        const GLfloat *col);
void graph_display_object_info(phys_obj *object, unsigned int num);
