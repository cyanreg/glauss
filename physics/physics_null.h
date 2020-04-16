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

#define PHYS_NULL {                                                            \
        .name = "null",                                                        \
        .version = PACKAGE_VERSION,                                            \
        .desc = "Only a control thread running, use with Lua",                 \
        .author = "Lynne",                                                     \
        .thread_preconfiguration = null_preinit,                               \
        .thread_configuration = null_init,                                     \
        .thread_location = thread_null,                                        \
        .thread_destruction = null_quit,                                       \
    }

#define PHYS_NULL_STATS {                                                      \
        .name = "null_stats",                                                  \
        .version = PACKAGE_VERSION,                                            \
        .desc = "Outputs statistics(n-body), slow",                            \
        .author = "Lynne",                                                     \
        .thread_preconfiguration = stats_preinit,                              \
        .thread_configuration = null_init,                                     \
        .thread_location = thread_stats,                                       \
        .thread_destruction = null_quit,                                       \
        .thread_sched_fn = stats_runtime_fn,                                   \
    }

struct null_statistics {
    double null_avg_dist, null_max_dist;
};

struct thread_config_null {
    phys_obj *obj;
    struct null_statistics *glob_stats;
    struct null_statistics *stats;
    unsigned int id, objs_low, objs_high, obj_num;
    pthread_barrier_t *ctrl;
    pthread_mutex_t *mute;
    volatile bool *quit;
};

void **null_init(struct glob_thread_config *cfg);

void *null_preinit(struct glob_thread_config *cfg);
void *thread_null(void *thread_setts);

void *stats_preinit(struct glob_thread_config *cfg);
void stats_runtime_fn(struct glob_thread_config *cfg);
void *thread_stats(void *thread_setts);

void null_quit(struct glob_thread_config *cfg);
