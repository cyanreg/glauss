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
#include <stdio.h>
#include <stdlib.h>
#include <tgmath.h>
#include <pthread.h>
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "physics.h"
#include "physics_n_body.h"

void **nbody_init(struct glob_thread_config *cfg)
{
    struct thread_config_nbody **thread_config = calloc(cfg->algo_threads,
        sizeof(struct thread_config_nbody*));
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k] = calloc(1, sizeof(struct thread_config_nbody));
    }

    /* Init barrier */
    pthread_barrier_t *barrier = calloc(1, sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier, NULL, cfg->algo_threads);

    int totcore = (int)((float)cfg->obj_num/cfg->algo_threads);

    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k]->glob_stats = cfg->stats;
        thread_config[k]->stats = &cfg->stats->t_stats[k];
        thread_config[k]->obj = cfg->obj;
        thread_config[k]->obj_num = cfg->obj_num;
        thread_config[k]->ctrl = cfg->ctrl;
        thread_config[k]->barrier = barrier;
        thread_config[k]->id = k;
        thread_config[k]->quit = cfg->quit;
        thread_config[k]->objs_low = !k ? 0 : thread_config[k-1]->objs_high;
        thread_config[k]->objs_high = thread_config[k]->objs_low + totcore - 1;
        if (k == cfg->algo_threads - 1) {
            /*	Takes care of rounding problems with odd numbers.	*/
            thread_config[k]->objs_high += cfg->obj_num - thread_config[k]->objs_high;
        }
    }

    return (void**)thread_config;
}

void nbody_quit(struct glob_thread_config *cfg)
{
    struct thread_config_nbody **t = (struct thread_config_nbody **)cfg->threads_conf;

    /* Barrier */
    pthread_barrier_destroy(t[0]->barrier);
    free(t[0]->barrier);

    /* Config */
    for (int k = 0; k < cfg->algo_threads; k++) {
        free(t[k]);
    }
    free(t);
    return;
}

void *thread_nbody(void *thread_setts)
{
    struct thread_config_nbody *t = thread_setts;
    vec3 accprev = (vec3){0,0,0};
    const double dt = option->dt;
    const double gconst = option->gconst;

    while (!*t->quit) {
        for (unsigned int i = t->objs_low; i < t->objs_high + 1; i++) {
            if (t->obj[i].ignore)
                continue;
            t->obj[i].pos += (t->obj[i].vel*dt) +\
            (t->obj[i].acc)*((dt*dt)/2);
        }

        pthread_barrier_wait(t->barrier);

        printf("debi = %i, %i = %lf\n", option->obj, t->objs_high, t->obj[t->objs_high].mass);

        for (unsigned int i = t->objs_low; i < t->objs_high + 1; i++) {
            accprev = t->obj[i].acc;
            for (unsigned int j = 0; j < t->obj_num; j++) {
                if (i==j)
                    continue;
                t->obj[i].acc += VEC3_NORM_DIV_DIST2(t->obj[j].pos - t->obj[i].pos)*\
                                (gconst*t->obj[j].mass);
            }
            t->obj[i].vel += (t->obj[i].acc + accprev)*((dt)/2);
        }

        phys_ctrl_wait(t->ctrl);
    }
    return 0;
}
