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
#include "physics/physics_aux.h"

void draw_obj_axis(GLfloat scale);
void draw_obj_sphere(phys_obj *object);
void draw_obj_points(phys_obj *object);
void draw_obj_packed_elements_draw(phys_obj *object, struct atomic_cont *element);
void draw_obj_col_points(phys_obj *object);
void draw_objs_mode(phys_obj *object, enum draw_mode mode);
GLuint graph_init_objects(void);
