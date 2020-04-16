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

#include "input/parser.h"

#define PHYS_LUA {                                                             \
        .name = "lua_threads",                                                 \
        .version = PACKAGE_VERSION,                                            \
        .desc = "Multiple Lua threads, not recommended",                       \
        .author = "Lynne",                                                     \
        .thread_preconfiguration = t_lua_preinit,                              \
        .thread_configuration = t_lua_init,                                    \
        .thread_location = t_lua,                                              \
        .thread_destruction = t_lua_quit,                                      \
        .thread_sched_fn = t_lua_runtime,                                      \
    }

struct t_lua_statistics {
    unsigned int time;
    size_t lua_gc_mem;
};

struct thread_config_lua {
    phys_obj *obj;
    struct t_lua_statistics *glob_stats;
    struct t_lua_statistics *stats;
    unsigned int id, objs_low, objs_high, obj_num;
    char *lua_exec_fn;
    lua_State *L;
    pthread_barrier_t *ctrl;
    pthread_mutex_t *mute;
    volatile bool *quit;
};

void *t_lua_preinit(struct glob_thread_config *cfg);
void **t_lua_init(struct glob_thread_config *cfg);
void t_lua_runtime(struct glob_thread_config *cfg);
void *t_lua(void *thread_setts);
void t_lua_quit(struct glob_thread_config *cfg);
