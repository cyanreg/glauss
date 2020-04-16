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

#define PHYS_NBODY {                                                           \
        .name = "n-body",                                                      \
        .version = PACKAGE_VERSION,                                            \
        .desc = "Standard n-body simulation, velocity verlet integration",     \
        .author = "Lynne",                                                     \
        .thread_preconfiguration = NULL,                                       \
        .thread_configuration = nbody_init,                                    \
        .thread_location = thread_nbody,                                       \
        .thread_destruction = nbody_quit,                                      \
    }

struct thread_config_nbody {
    phys_obj *obj;
    unsigned int id, objs_low, objs_high, obj_num;
    struct global_statistics *glob_stats;
    struct thread_statistics *stats;
    pthread_barrier_t *ctrl, *barrier;
    volatile bool *quit;
};

void **nbody_init(struct glob_thread_config *cfg);
void nbody_quit(struct glob_thread_config *cfg);
void *thread_nbody(void *thread_setts);
