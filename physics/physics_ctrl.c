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
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "input/parser.h"
#include "input/input_thread.h"
#include "shared/options.h"
#include "shared/output.h"
#include "shared/msg_phys.h"
#include "physics.h"
#include "physics_aux.h"
#include "physics_ctrl.h"

struct glob_thread_config *ctrl_preinit(struct global_statistics *stats, phys_obj *obj)
{
    struct glob_thread_config *cfg = calloc(1, sizeof(struct glob_thread_config));

    cfg->algo_threads = option->threads;
    cfg->total_syncd_threads = cfg->algo_threads + 1;
    cfg->obj = obj;
    cfg->obj_num = option->obj;
    cfg->stats = stats;

    cfg->algo_thread_stats_map = calloc(cfg->algo_threads+1, sizeof(struct parser_map *));

    if (!stats)
        return cfg;

    /* Reinit stats */
    cfg->stats->threads = cfg->algo_threads;
    cfg->stats->t_stats = calloc(cfg->algo_threads+1, sizeof(struct thread_statistics));

    for (int k = 0; k < cfg->algo_threads; k++) {
        cfg->stats->t_stats[k].thread_stats_map = \
            allocate_parser_map((struct parser_map []){
                {"clockid",   P_TYPE(cfg->stats->t_stats[k].clockid)   },
                {0},
            });
    }
    return cfg;
}

struct glob_thread_config *ctrl_init(struct glob_thread_config *cfg)
{
    /* Transfer global stat map */
    if (cfg->algo_global_stats_map) {
        register_parser_map(cfg->algo_global_stats_map,
                            &cfg->stats->global_stats_map);
    }

    /* Transfer thread stat map */
    if (cfg->algo_thread_stats_map) {
        for (int k = 0; k < cfg->algo_threads; k++) {
            register_parser_map(cfg->algo_thread_stats_map[k],
                                &cfg->stats->t_stats[k].thread_stats_map);
        }
    }

    cfg->threads = calloc(cfg->algo_threads, sizeof(pthread_t));

    cfg->ctrl = calloc(1, sizeof(pthread_barrier_t));
    pthread_barrier_init(cfg->ctrl, NULL, cfg->total_syncd_threads);

    cfg->io_halt = calloc(1, sizeof(pthread_spinlock_t));
    pthread_spin_init(cfg->io_halt, PTHREAD_PROCESS_PRIVATE);

    cfg->pause = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(cfg->pause, NULL);

    cfg->pause_cond = calloc(1, sizeof(pthread_cond_t));
    pthread_cond_init(cfg->pause_cond, NULL);

    cfg->step_pause = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(cfg->step_pause, NULL);

    cfg->step_cond = calloc(1, sizeof(pthread_cond_t));
    pthread_cond_init(cfg->step_cond, NULL);

    cfg->quit = calloc(1, sizeof(bool));

    /* Back buffer allocation */
    cfg->step_back_buffer_size = option->step_back_buffer;

    if (cfg->step_back_buffer_size) {
        cfg->step_back_buffer = calloc(option->step_back_buffer, sizeof(phys_obj *));
        for (unsigned int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
            cfg->step_back_buffer[i] = calloc(cfg->obj_num + 1, sizeof(phys_obj));
        }
    }

    return cfg;
}

void ctrl_quit(struct glob_thread_config *cfg)
{
    if (cfg->algo_global_stats_map) {
        unregister_parser_map(cfg->algo_global_stats_map, &cfg->stats->global_stats_map);
        /* Free all maps set by any threads */
        free(cfg->algo_global_stats_map);
    }

    if (cfg->algo_thread_stats_map) {
        for (int k = 0; k < cfg->algo_threads; k++) {
            free(cfg->algo_thread_stats_map[k]);
            free(cfg->stats->t_stats[k].thread_stats_map);
        }
        free(cfg->algo_thread_stats_map);
    }
    free(cfg->stats->t_stats);
    cfg->stats->t_stats = NULL;
    cfg->stats->threads = 0;

    /* Free global sync barrier */
    pthread_barrier_destroy(cfg->ctrl);
    free(cfg->ctrl);

    /* Free IO spinlock */
    pthread_spin_destroy(cfg->io_halt);
    free((void *)cfg->io_halt);

    /* Free pause mutex */
    pthread_mutex_destroy(cfg->pause);
    free(cfg->pause);

    /* Free pause cond */
    pthread_cond_destroy(cfg->pause_cond);
    free(cfg->pause_cond);

    /* Free step pause mutex */
    pthread_mutex_destroy(cfg->step_pause);
    free(cfg->step_pause);

    /* Free step pause cond */
    pthread_cond_destroy(cfg->step_cond);
    free(cfg->step_cond);

    /* Free step_back_buffer */
    if (cfg->step_back_buffer_size) {
        for (unsigned int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
            free(cfg->step_back_buffer[i]);
        }
        free(cfg->step_back_buffer);
    }

    /* Free anything else */
    free((void *)cfg->quit);
    free(cfg->algorithm);
    free(cfg->simconf_id);
    free(cfg);
    return;
}

void *thread_ctrl(void *thread_setts)
{
    struct glob_thread_config *t = thread_setts;
    const double dt = option->dt;
    unsigned int xyz_counter = 0, sshot_counter = 0, funct_counter = 0;
    unsigned int thread_fn_counter = 0, gc_count = 0, steps_count = 0;
    long long unsigned int t1 = phys_gettime_us(), t2 = 0;

    pthread_mutex_lock(t->pause);
    pthread_mutex_lock(t->step_pause);

    while (!*t->quit) {
        /* Block all threads */
        pthread_barrier_wait(t->ctrl);

        /* Lua function execution */
        if (phys_timer_exec(option->exec_funct_freq, &funct_counter)) {
            lua_exec_funct(option->timestep_funct, t->obj, t->stats);
            /* Update parser lua gc memory allocation stats */
            phys_stats->lua_gc_mem = parser_lua_current_gc_mem(NULL);
        }

        if (t->thread_sched_fn && phys_timer_exec(t->thread_sched_fn_freq,
                                                &thread_fn_counter)) {
            t->thread_sched_fn(t);
        }

        if (phys_timer_exec(option->lua_gc_sweep_freq, &gc_count)) {
            size_t mem_sweep = parser_lua_gc_sweep(NULL);
            pprint_ok("Ran full garbage collector sweep on the parser Lua context, freed %lu bytes\n", mem_sweep);
        }

        /* Fill back buffer if needed */
        if (t->step_back_buffer_size) {
            /* Reset buffer position */
            t->step_back_buffer_pos = 0;
            /* Copy the old pointer order */
            phys_obj *old_buffer[t->step_back_buffer_size];
            for (unsigned int i = 0; i < t->step_back_buffer_size + 1; i++) {
                old_buffer[i] = t->step_back_buffer[i];
            }
            /* Rotate the pointers in the new array */
            for (int i = 0; i < t->step_back_buffer_size + 1; i++) {
                /* Why does Clang crash if we do it directly? A big mystery. */
                volatile int old_ind = i < 1 ? t->step_back_buffer_size : i - 1;
                t->step_back_buffer[i] = old_buffer[old_ind];
            }
            /* Write to the oldest buffer */
            memcpy(t->step_back_buffer[0], t->obj, t->obj_num*sizeof(phys_obj));
        }

        /* Unlock IO */
        pthread_spin_unlock(t->io_halt);

        /* Update progress */
        t->total_steps++;
        t->stats->total_steps++;
        t2 = phys_gettime_us();
        t->stats->time_per_step = (t2 - t1)*1.0e-6;
        t1 = t2;
        t->stats->progress += dt;

        /* Dump XYZ file, will not increment timer if !option->dump_xyz */
        if (phys_timer_exec(option->dump_xyz, &xyz_counter)) {
            out_write_xyz(t->obj, option->xyz_temp, NULL);
        }

        /* Signal to create a screenshot next frame */
        if (phys_timer_exec(option->dump_sshot, &sshot_counter)) {
            option->write_sshot_now = true;
        }

        /* Step forward block */
        if (t->steps_fwd && phys_timer_exec(t->steps_fwd, &steps_count)) {
            t->step_end = true;
            t->steps_fwd = 0;
            pthread_cond_wait(t->step_cond, t->step_pause);
        }

        if (option->final_frame && t->total_steps >= option->final_frame) {
            t->paused = 1;
        }

        /* Pause all threads by stalling unlocking t->ctrl */
        if (t->paused) {
            pthread_cond_wait(t->pause_cond, t->step_pause);
        }

        /* Lock back the IO */
        pthread_spin_lock(t->io_halt);

        /* Unblock and hope the other threads follow */
        pthread_barrier_wait(t->ctrl);
    }

    pthread_mutex_unlock(t->step_pause);
    pthread_mutex_unlock(t->pause);

    return 0;
}
