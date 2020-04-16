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
#include "physics.h"
#include "physics_null.h"
#include "shared/options.h"
#include "input/parser.h"

void *null_preinit(struct glob_thread_config *cfg)
{
    cfg->total_syncd_threads = 1;
    cfg->algo_thread_stats_raw = calloc(cfg->algo_threads, sizeof(struct null_statistics *));
    return NULL;
}

void *stats_preinit(struct glob_thread_config *cfg)
{
    /* Attempt to balance out the load every cycle */
    cfg->thread_sched_fn_freq = 1;

    struct null_statistics *global_stats = calloc(1, sizeof(struct null_statistics));
    cfg->algo_global_stats_map = allocate_parser_map((struct parser_map []){
        {"null_avg_dist",   P_TYPE(global_stats->null_avg_dist)   },
        {"null_max_dist",   P_TYPE(global_stats->null_max_dist)   },
        {0},
    });
    cfg->algo_global_stats_raw = global_stats;

    struct null_statistics **thread_stats = calloc(cfg->algo_threads, sizeof(struct null_statistics *));
    for (int i = 0; i < cfg->algo_threads; i++) {
        thread_stats[i] = calloc(1, sizeof(struct null_statistics));
        cfg->algo_thread_stats_map[i] = allocate_parser_map((struct parser_map []){
            {"null_avg_dist",   P_TYPE(thread_stats[i]->null_avg_dist)   },
            {"null_max_dist",   P_TYPE(thread_stats[i]->null_max_dist)   },
            {0},
        });
    }
    cfg->algo_thread_stats_raw = (void **)thread_stats;

    return NULL;
}

void **null_init(struct glob_thread_config *cfg)
{
    struct thread_config_null **thread_config = calloc(cfg->algo_threads,
        sizeof(struct thread_config_null*));
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k] = calloc(1, sizeof(struct thread_config_null));
    }

    /* Init mutex */
    pthread_mutex_t *mute = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(mute, NULL);

    int totcore = (int)((float)cfg->obj_num/cfg->algo_threads);

    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k]->glob_stats = cfg->algo_global_stats_raw;
        thread_config[k]->stats = cfg->algo_thread_stats_raw[k];
        thread_config[k]->ctrl = cfg->ctrl;
        thread_config[k]->mute = mute;
        thread_config[k]->id = k;
        thread_config[k]->quit = cfg->quit;
        thread_config[k]->obj = cfg->obj;
        thread_config[k]->obj_num = cfg->obj_num;
        thread_config[k]->objs_low = !k ? 0 : thread_config[k-1]->objs_high;
        thread_config[k]->objs_high = thread_config[k]->objs_low + totcore - 1;
        if (k == cfg->algo_threads - 1) {
            /*	Takes care of rounding problems with odd numbers.	*/
            thread_config[k]->objs_high += cfg->obj_num - thread_config[k]->objs_high;
        }
    }

    return (void**)thread_config;
}

void null_quit(struct glob_thread_config *cfg)
{
    struct thread_config_null **t = (struct thread_config_null **)cfg->threads_conf;

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
        free(t[k]);
    }
    free(t);
    return;
}

void stats_runtime_fn(struct glob_thread_config *cfg)
{
    struct thread_config_null **t = (struct thread_config_null **)cfg->threads_conf;

    double null_avg_dist = 0, null_max_dist = 0;

    for (int k = 0; k < cfg->algo_threads; k++) {
        null_avg_dist += t[k]->stats->null_avg_dist+null_avg_dist;
        null_max_dist = fmax(t[k]->stats->null_max_dist, null_max_dist);
    }

    t[0]->glob_stats->null_avg_dist  = null_avg_dist/cfg->algo_threads;
    t[0]->glob_stats->null_max_dist  = null_max_dist;
}

void *thread_stats(void *thread_setts)
{
    struct thread_config_null *t = thread_setts;
    /* We need to play along with the control thread, so continue running. */
    while (!*t->quit) {
        double dist = 0.0, avg_dist = 0.0, max_dist = 0.0;
        for (unsigned int i = t->objs_low; i < t->objs_high + 1; i++) {
            for (unsigned int j = 0; j < t->obj_num; j++) {
                if (i==j)
                    continue;
                dist = VEC3_LEN(t->obj[j].pos - t->obj[i].pos);
                max_dist = fmax(max_dist, dist);
                avg_dist = (dist + avg_dist)/2;
            }
        }

        t->stats->null_avg_dist = avg_dist;
        t->stats->null_max_dist = max_dist;

        phys_ctrl_wait(t->ctrl);
    }
    return 0;
}

void *thread_null(void *thread_setts)
{
    return 0;
}
