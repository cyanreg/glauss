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

int    out_write_xyz(phys_obj *object, const char *template_str,
                    pthread_spinlock_t *io_halt);
size_t out_write_array(phys_obj *object, const char *template_str,
                    pthread_spinlock_t *io_halt);
size_t in_write_array(phys_obj **object, const char *filename,
                    pthread_spinlock_t *io_halt);
