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

#include "physics.h"

/* Allocate the cfg structure, as well as the maps needed to push the stats to Lua.
* It also allocates the double pointer(**) to the map per thread so you don't have to */
struct glob_thread_config *ctrl_preinit(struct global_statistics *stats, phys_obj *obj);

/* Initialize the cfg. Once this is called, it's all been set in stone and modifying
* whatever you want in your own algorithm's init function is not recommended.
* The maps for statistics are merged by us into the global statistics structure.
* The options map is registered by physics.c, which for convenience keeps them until
* an algorithm has been changed. So you could stop, modify a setting and restart. */
struct glob_thread_config *ctrl_init(struct glob_thread_config *cfg);

/* Free everything. Yes, every map allocated will be freed. You can access the _map
* pointers until this point. Afterwards, they are cleaned up. */
void ctrl_quit(struct glob_thread_config *cfg);

void *thread_ctrl(void *thread_setts);
