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

enum ext_file {
    MOL_XYZ,
    MOL_PDB,
    MOL_OBJ,
    MOL_UNKNOWN,
};

typedef struct {
    phys_obj *inf;
    vec3 rot;
    char filename[100];
    double scale;
} in_file;

enum ext_file in_file_ext(const char *filename);
int in_probe_file(const char *filename);
int in_read_file(phys_obj *object, int *i, in_file *file);
