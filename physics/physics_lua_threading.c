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
#include <unistd.h>
#include <stdio.h>
#include <complex.h>
#include <tgmath.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "physics.h"
#include "physics_aux.h"
#include "physics_lua_threading.h"
#include "shared/options.h"
#include "shared/msg_phys.h"

static unsigned int t_lua_gc_freq = 10;

void *t_lua_preinit(struct glob_thread_config *cfg)
{
    /* Attempt to balance out the load every cycle */
    cfg->thread_sched_fn_freq = 1;

    struct t_lua_statistics *global_stats = calloc(1, sizeof(struct t_lua_statistics));
    cfg->algo_global_stats_map = allocate_parser_map((struct parser_map []){
        {"lua_gc_mem",   P_TYPE(global_stats->lua_gc_mem)   },
        {"time",         P_TYPE(global_stats->time)         },
        {0},
    });
    cfg->algo_global_stats_raw = global_stats;

    struct t_lua_statistics **thread_stats = calloc(cfg->algo_threads, sizeof(struct t_lua_statistics *));
    for (int i = 0; i < cfg->algo_threads; i++) {
        thread_stats[i] = calloc(1, sizeof(struct t_lua_statistics));
        cfg->algo_thread_stats_map[i] = allocate_parser_map((struct parser_map []){
            {"lua_gc_mem",   P_TYPE(thread_stats[i]->lua_gc_mem)   },
            {"time",         P_TYPE(thread_stats[i]->time)         },
            {0},
        });
    }
    cfg->algo_thread_stats_raw = (void **)thread_stats;

    cfg->algo_opt_map = \
        allocate_parser_map((struct parser_map []){
            {"t_lua_gc_freq",       P_TYPE(t_lua_gc_freq)       },
            {0},
        });

    return NULL;
}

static lua_State *t_lua_open_file(const char *filename)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    /* Load file */
    if(luaL_loadfile(L, filename)) {
        pprintf(PRI_ERR, "Opening Lua file %s failed!\n", filename);
        return NULL;
    }
    /* Execute script */
    lua_pcall(L, 0, 0, 0);

    return L;
}

void **t_lua_init(struct glob_thread_config *cfg)
{
    struct thread_config_lua **thread_config = calloc(cfg->algo_threads,
                                                    sizeof(struct thread_config_lua*));
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k] = calloc(1, sizeof(struct thread_config_lua));
    }

    /* Init mutex */
    pthread_mutex_t *mute = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(mute, NULL);

    int totcore = (int)((float)cfg->obj_num/cfg->algo_threads);

    for (int k = 0; k < cfg->algo_threads; k++) {
        /* Yeah, it's slow, USE THE DAMN C API if you want proper multithreading! */
        thread_config[k]->L = t_lua_open_file(option->filename);
        thread_config[k]->glob_stats = cfg->algo_global_stats_raw;
        thread_config[k]->stats = cfg->algo_thread_stats_raw[k];
        thread_config[k]->ctrl = cfg->ctrl;
        thread_config[k]->mute = mute;
        thread_config[k]->id = k;
        thread_config[k]->quit = cfg->quit;
        thread_config[k]->obj = cfg->obj;
        thread_config[k]->obj_num = cfg->obj_num;
        thread_config[k]->lua_exec_fn = strdup(option->timestep_funct);
        thread_config[k]->objs_low = !k ? 0 : thread_config[k-1]->objs_high;
        thread_config[k]->objs_high = thread_config[k]->objs_low + totcore - 1;
        if (k == cfg->algo_threads - 1) {
            /*	Takes care of rounding problems with odd numbers.	*/
            thread_config[k]->objs_high += cfg->obj_num - thread_config[k]->objs_high;
        }
    }

    return (void**)thread_config;
}

void t_lua_quit(struct glob_thread_config *cfg)
{
    struct thread_config_lua **t = (struct thread_config_lua **)cfg->threads_conf;

    /* Stats */
    if (cfg->total_syncd_threads > 1) {
        free(cfg->algo_global_stats_raw);
    }
    free(cfg->algo_thread_stats_raw);

    /* Mutex */
    pthread_mutex_destroy(t[0]->mute);
    free(t[0]->mute);

    /* Thread cfg */
    for (int k = 0; k < cfg->algo_threads; k++) {
        lua_close(t[k]->L);
        free(t[k]->lua_exec_fn);
        free(t[k]);
    }
    free(t);
    return;
}

void t_lua_runtime(struct glob_thread_config *cfg)
{
    struct thread_config_lua **t = (struct thread_config_lua **)cfg->threads_conf;

    unsigned int time = 0;
    size_t lua_gc_mem = 0;

    for (int k = 0; k < cfg->algo_threads; k++) {
        time += t[k]->stats->time;
        t[k]->stats->lua_gc_mem = parser_lua_current_gc_mem(t[k]->L);
        lua_gc_mem += t[k]->stats->lua_gc_mem;
    }

    t[0]->glob_stats->time  = time/cfg->algo_threads;
    t[0]->glob_stats->lua_gc_mem  = lua_gc_mem;
}

void *t_lua(void *thread_setts)
{
    struct thread_config_lua *t = thread_setts;
    /* We need to play along with the control thread, so continue running. */

    unsigned int gc_count = 0;

    unsigned int l_obj_low = t->objs_low + 1;
    unsigned int l_obj_high = t->objs_high + 1;
    struct parser_map range[] = {
        {"thread_id",      P_TYPE(t->id)        },
        {"range_low",      P_TYPE(l_obj_low)    },
        {"range_high",     P_TYPE(l_obj_high)   },
        {0},
    };

    while (!*t->quit) {
        /* Set to the exec f-n */
        lua_getglobal(t->L, t->lua_exec_fn);

        /* Push all stats */
        parser_push_stat_array(t->L, phys_stats);
        /* Push objects and metadata table */
        parser_push_object_array(t->L, t->obj, range);

        /* Run function with arguments */
        lua_call(t->L, 2, 1);

        /* Get objects */
        unsigned int len = 0;
        if (!lua_isnil(t->L, -1)) {
            len = conf_lua_getlen(t->L, -1);
        }

        struct lua_parser_state *parser_state = &(struct lua_parser_state){
            .i = 0,
            .nullswitch = 0,
            .fileset = 0,
            .read_id = 1,
            .file = {0},
            .buffer = {{0}},
            .object = t->obj,
        };

        if (len) {
            if (lua_istable(t->L, -1)) {
                conf_traverse_table(t->L, &conf_lua_parse_objs, parser_state);
            } else {
                pprint_warn("Lua f-n \"%s\" did not return a table of objects. Ignoring.\n",
                            t->lua_exec_fn);
            }
        }

        if (phys_timer_exec(t_lua_gc_freq, &gc_count)) {
            lua_gc(t->L, LUA_GCCOLLECT, 0);
        }

        phys_ctrl_wait(t->ctrl);
    }
    return 0;
}
