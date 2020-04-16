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
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "config.h"
#include "physics.h"
#include "physics_aux.h"
#include "shared/options.h"
#include "shared/output.h"
#include "shared/msg_phys.h"
#include "input/parser.h"
#include "input/sighandle.h"
#include "physics_ctrl.h"

/* Algorithms */
#include "physics_null.h"
#include "physics_n_body.h"
#include "physics_barnes_hut.h"
#include "physics_lua_threading.h"

/* "none" algorithm description */
#define PHYS_NONE {                               \
        .name = "none",                           \
        .version = PACKAGE_VERSION,               \
        .desc = "Starts absolutely no threads",   \
        .author = NULL,                           \
        .thread_configuration = NULL,             \
        .thread_location = (thread_function)1,    \
        .thread_destruction = NULL,               \
    }

/* Default threads to use when system != linux. */
#define AUTO_UNAVAIL_THREADS 1

pthread_spinlock_t *halt_objects = NULL;
pthread_attr_t thread_attribs;
static struct sched_param parameters;

/* Thread statistics and primary struct */
struct global_statistics *phys_stats;
struct glob_thread_config *cfg;
phys_obj **urgetnt_dump_object = NULL;

/* Previous algorithm */
static struct list_algorithms *prev_algorithm = NULL;
static struct parser_map *prev_algo_opts = NULL;

/* Populate structure with names and function pointers */
phys_algorithm phys_algorithms[] = {
    PHYS_NONE,
    PHYS_NULL,
    PHYS_NULL_STATS,
    PHYS_LUA,
    PHYS_NBODY,
    PHYS_BARNES_HUT,
    {0}
};

phys_algorithm *phys_find_algorithm(const char *name)
{
    for (phys_algorithm *i = phys_algorithms; i->name; i++) {
        if (!strcmp(i->name, name)) {
            return i;
        }
    }
    return NULL;
}

struct glob_thread_config *phys_get_config()
{
    if (!cfg)
        return NULL;
    else
        return cfg;
}

void phys_list_algo(void)
{
    pprintf(PRI_ESSENTIAL, "Implemented algorithms:\n");
    pprintf(PRI_ESSENTIAL, "    name		version			description\n");
    for (int n = 0; phys_algorithms[n].name; n++) {
        pprintf(PRI_ESSENTIAL, "%i.  %s		%s		%s\n", n,
            phys_algorithms[n].name,
            phys_algorithms[n].version,
            phys_algorithms[n].desc);
    }
}

int phys_init(phys_obj **object)
{
    if (!object) {
        pprint_err("Cannot init - received NULL object pointer.\n");
        return 1;
    }
    /* Check if physics algorithm is valid */
    if (!phys_find_algorithm(option->algorithm)) {
        pprintf(PRI_ERR, "Algorithm \"%s\" not found!\n", option->algorithm);
        phys_list_algo();
        exit(1);
    }

    /* Allocate memory for all the objects */
    *object = calloc(option->obj+1, sizeof(phys_obj));

    if (*object) {
        pprintf(PRI_OK, "Allocated %lu bytes(%u objects) to object array at %p.\n",
                option->obj*sizeof(phys_obj), option->obj, *object);
    } else {
        return 3; /* Impossible, should never ever happen(glibc). */
    }

    /* Set urgent dump object pointer */
    urgetnt_dump_object = object;

    /* Seed RNG even though it's only used when option->bh_random_assign */
    srand(time(NULL));

    /* Set the amount of threads */
    unsigned short online_cores = 0;

#ifdef __linux__
    online_cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    /* Threads */
    if (!option->threads) {
        if (online_cores) {
            option->threads = online_cores;
            pprintf(PRI_OK, "Detected %u threads, will use all.\n", online_cores);
        } else {
            option->threads = AUTO_UNAVAIL_THREADS;
            pprintf(PRI_WARN,
                    "Core detection unavailable, running with %u thread(s).\n",
                    AUTO_UNAVAIL_THREADS);
        }
    } else {
        if (online_cores) {
            pprintf(PRI_VERYHIGH, "Running with %u thread(s), out of %u cores.\n",
                    option->threads, online_cores);
        } else {
            pprintf(PRI_VERYHIGH, "Runninh with %u threads.\n", option->threads);
        }
    }

    if (option->threads > option->obj) {
        pprintf(PRI_WARN, "More threads than objects. Capping threads to %u\n",
                option->obj);
        option->threads = option->obj;
    }

    phys_stats = calloc(1, sizeof(struct global_statistics));
    phys_stats->global_stats_map = allocate_parser_map((struct parser_map []){
            {"total_steps",        P_TYPE(phys_stats->total_steps)        },
            {"rng_seed",           P_TYPE(phys_stats->rng_seed)           },
            {"progress",           P_TYPE(phys_stats->progress)           },
            {"time_running",       P_TYPE(phys_stats->time_running)       },
            {"time_per_step",      P_TYPE(phys_stats->time_per_step)      },
            {"lua_gc_mem",         P_TYPE(phys_stats->lua_gc_mem)         },
            {0},
        });

    /* pthreads configuration */
    parameters.sched_priority = 50;
    pthread_attr_init(&thread_attribs);
    pthread_attr_setinheritsched(&thread_attribs, PTHREAD_INHERIT_SCHED);
    pthread_attr_setschedparam(&thread_attribs, &parameters);
    return 0;
}

int phys_set_sched_mode(char **mode)
{
    bool found = false;
    struct parser_map sched_modes[] = {
        {"SCHED_RR",      NULL,   SCHED_RR,      LUA_TNUMBER   },
        {"SCHED_FIFO",    NULL,   SCHED_FIFO,    LUA_TNUMBER   },
        {"SCHED_OTHER",   NULL,   SCHED_OTHER,   LUA_TNUMBER   },
        {0},
    };
    for (struct parser_map *i = sched_modes; i->name; i++) {
        if (!strcmp(i->name, *mode)) {
            pthread_attr_setschedpolicy(&thread_attribs, i->type);
            found = true;
        }
    }
    if (!found) {
        pprint_err("Scheduling mode %s not found. Possible modes: ", *mode);
        for (struct parser_map *i = sched_modes; i->name; i++) {
            pprintf(PRI_ESSENTIAL, "%s ", i->name);
        }
        pprintf(PRI_ESSENTIAL, "\nSetting mode to default SCHED_RR\n");
        free(*mode);
        *mode = strdup("SCHED_RR");
        pthread_attr_setschedpolicy(&thread_attribs, SCHED_RR);
    }
    return found;
}

void phys_list_opts(const char *algo)
{
    phys_algorithm *temp_algo = phys_find_algorithm(algo);
    if (!temp_algo) {
        pprintf(PRI_ERR, "Algorithm %s not found!\n", algo);
        return;
    }

    struct glob_thread_config *temp_cfg = cfg = ctrl_preinit(NULL, NULL);

    if (temp_algo->thread_preconfiguration) {
        temp_algo->thread_preconfiguration(temp_cfg);
    }

    if (!temp_cfg->algo_opt_map) {
        printf("\nAlgorithm %s has no options\n", temp_algo->name);
        goto close;
    }

    char str[25];
    printf("\nAlgorithm %s has the following options:\n", temp_algo->name);
    for (struct parser_map *i = temp_cfg->algo_opt_map; i->name; i++) {
        parser_get_value_str(*i, str, sizeof(str));
        printf("	%s = %s\n", i->name, str);
    }

    /* YES, IT STILL LEAKS. I KNOW, RIGHT? THEN DONT RUN THIS 10^6 TIMES! */

    close:
        free(temp_cfg->algo_opt_map);
        free(temp_cfg->algo_global_stats_map);
        free(temp_cfg->algo_thread_stats_map);
        free(temp_cfg);
}

void phys_urgent_dump(void)
{
    pprint_warn("Backing up entire system in backup.bin!\n");
    out_write_array(*urgetnt_dump_object, "backup.bin", NULL);
}

int phys_quit(phys_obj **object)
{
    if (cfg) {
        pprint_err("Physics threads were probably still running.\n");
        free(cfg);
        cfg = NULL;
        return 1;
    }

    /* Elements */
    free_elements();

    /* Stats */
    free(phys_stats);

    /* Prev opts */
    free(prev_algo_opts);

    /* Obj */
    if (object) {
        free(*object);
    }
    return 0;
}

void phys_ctrl_wait(pthread_barrier_t *barr)
{
    /* Signal ctrl thread and wait for it to finish */
    pthread_barrier_wait(barr);
    pthread_barrier_wait(barr);
}

phys_obj *phys_history(unsigned int steps)
{
    if (!cfg)
        return NULL;
    steps = steps > cfg->step_back_buffer_size ? cfg->step_back_buffer_size : steps;
    steps = steps > cfg->stats->total_steps ? cfg->stats->total_steps : steps;
    return cfg->step_back_buffer[steps];
}

unsigned int phys_fwd_steps(unsigned int steps)
{
    if (!cfg)
        return 0;
    if (cfg->step_end) {
        pthread_cond_signal(cfg->step_cond);
        cfg->step_end = false;
    }
    cfg->steps_fwd += steps;
    if (cfg->paused) {
        pthread_cond_signal(cfg->pause_cond);
    }
    return cfg->steps_fwd;
}

unsigned int phys_bwd_steps(unsigned int steps)
{
    if (!cfg || !steps)
        return 0;

    /* Checks */
    steps = steps > cfg->step_back_buffer_size ? cfg->step_back_buffer_size : steps;
    steps = steps > cfg->stats->total_steps ? cfg->stats->total_steps : steps;

    pthread_spin_lock(cfg->io_halt);

    memcpy(cfg->obj, cfg->step_back_buffer[steps], cfg->obj_num*sizeof(phys_obj));

    pthread_spin_unlock(cfg->io_halt);

    return steps;
}

int phys_buffer_forward_single_step(void)
{
    if (!cfg || !cfg->paused || !cfg->step_back_buffer[cfg->step_back_buffer_size])
        return 1;

    cfg->step_back_buffer_pos--;

    /* Code so hard to write I can't even comment it */
    if (cfg->step_back_buffer_size > cfg->total_steps) {
        pprint_err("Incomplete buffer, cannot advance!\n");
        /* Technically it's possible to shuffle the pointers enough so that
        * the very first is at the top, but nah, this is enough for now */
        cfg->step_back_buffer_pos++;
        return 1;
    } else if (cfg->step_back_buffer_pos < 0) {
        cfg->stats->total_steps -= cfg->step_back_buffer_size + 1;
        cfg->step_back_buffer_pos = cfg->step_back_buffer_size;
    }

    cfg->stats->total_steps++;

    pthread_spin_lock(cfg->io_halt);

    phys_obj *old_buffer[cfg->step_back_buffer_size];
    for (unsigned int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
        old_buffer[i] = cfg->step_back_buffer[i];
    }
    for (int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
        cfg->step_back_buffer[i] = old_buffer[i < 1 ? cfg->step_back_buffer_size : i - 1];
    }

    memcpy(cfg->obj, cfg->step_back_buffer[0], cfg->obj_num*sizeof(phys_obj));

    pprint_verb("Buffer position - %i out of %i\n",
                cfg->step_back_buffer_pos, cfg->step_back_buffer_size);

    pthread_spin_unlock(cfg->io_halt);

    return 0;
}

int phys_buffer_revert_single_step(void)
{
    if (!cfg || !cfg->paused)
        return 1;

    cfg->step_back_buffer_pos++;

    /* Yep, largely the result of trial, rewriting and error */
    if (cfg->step_back_buffer_pos >= cfg->total_steps) {
        pprint_err("Incomplete buffer, unable to rewind!\n");
        cfg->step_back_buffer_pos--;
        return 1;
    } else if (cfg->step_back_buffer_pos > cfg->step_back_buffer_size) {
        cfg->stats->total_steps += cfg->step_back_buffer_pos;
        cfg->step_back_buffer_pos = 0;
    }
    cfg->stats->total_steps--;

    pthread_spin_lock(cfg->io_halt);

    /* Basically undoes the control thread's actions */
    phys_obj *old_buffer[cfg->step_back_buffer_size];
    for (unsigned int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
        old_buffer[i] = cfg->step_back_buffer[i];
    }
    for (int i = 0; i < cfg->step_back_buffer_size + 1; i++) {
        cfg->step_back_buffer[i] = old_buffer[i >= cfg->step_back_buffer_size ? 0 : i + 1];
    }

    memcpy(cfg->obj, cfg->step_back_buffer[0], cfg->obj_num*sizeof(phys_obj));

    pprint_verb("Buffer position - %i out of %i\n",
                cfg->step_back_buffer_pos, cfg->step_back_buffer_size);

    pthread_spin_unlock(cfg->io_halt);

    return 0;
}

enum phys_status phys_ctrl(enum phys_set_status status, phys_obj **object)
{
    phys_algorithm *algo = NULL;
    enum phys_status retval = PHYS_CMD_NOT_FOUND;
    switch (status) {
        case PHYS_STATUS:
            if (!cfg) {
                retval = PHYS_STATUS_STOPPED;
            } else if (!cfg->pause) {
                retval = PHYS_STATUS_INIT;
            } else {
                retval = cfg->paused ? PHYS_STATUS_PAUSED : PHYS_STATUS_RUNNING;
            }
            break;
        case PHYS_PAUSESTART:
            if (!cfg) {
                retval = PHYS_STATUS_STOPPED;
            }
            if (cfg->step_end) {
                pthread_cond_signal(cfg->step_cond);
                cfg->step_end = false;
                cfg->steps_fwd = 0;
                break;
            }
            if (cfg->paused) {
                pthread_cond_signal(cfg->pause_cond);
                cfg->paused = false;
                retval = PHYS_STATUS_RUNNING;
            } else {
                cfg->paused = true;
                retval = PHYS_STATUS_PAUSED;
            }
            break;
        case PHYS_STEP_FWD:
            phys_fwd_steps(1);
            retval = PHYS_STATUS_PAUSED;
            break;
        case PHYS_START:
            algo = phys_find_algorithm(option->algorithm);
            if (!algo) {
                pprintf(PRI_ERR, "Algorithm %s not found!\n", option->algorithm);
                retval = PHYS_INVALID_ALGORITHM;
                break;
            } else if (algo->thread_location && !algo->thread_configuration) {
                /* Algorithm = none */
                retval = PHYS_STATUS_STOPPED;
                break;
            } else if (cfg) {
                retval = PHYS_STATUS_RUNNING;
                break;
            }

            if (!option->threads) {
                pprint_err("Threads = 0, nothing to start\n");
                retval = PHYS_INVALID_ALGORITHM;
                break;
            }

            if (!object) {
                pprint_err("Cannot start - received NULL object pointer.\n");
                retval = PHYS_INVALID_ALGORITHM;
                break;
            }

            /* Init RNG */
            phys_stats->rng_seed = option->rng_seed ? option->rng_seed : phys_gettime_us();
            srand(phys_stats->rng_seed);

            /* Set scheduling mode */
            phys_set_sched_mode(&option->thread_schedule_mode);

            /* Create configuration */
            cfg = ctrl_preinit(phys_stats, *object);
            cfg->thread_sched_fn = algo->thread_sched_fn;
            cfg->algorithm = strdup(algo->name);
            cfg->simconf_id = strdup(option->simconf_id);

            /* Algorithms should register options here */
            if (algo->thread_preconfiguration) {
                cfg->returned_from_preinit = algo->thread_preconfiguration(cfg);
            }

            /* Options state machine - only replace upon algorithm change */
            if (prev_algorithm != algo) {
                if (prev_algorithm) {
                    unregister_parser_map(prev_algo_opts, &total_opt_map);
                    free(prev_algo_opts);
                }
                register_parser_map(cfg->algo_opt_map, &total_opt_map);
                /* Read any options set by algorithm */
                parse_lua_simconf_options(cfg->algo_opt_map);
            } else {
                /* Will just update the pointers */
                update_parser_map(cfg->algo_opt_map, &total_opt_map);
            }

            cfg = ctrl_init(cfg);
            halt_objects = cfg->io_halt;
            cfg->threads_conf = algo->thread_configuration(cfg);

            /* Check for errors */
            if (!cfg->threads_conf) {
                pprint_err("Algorithm's config f-n returned NULL, failure.\n");
                ctrl_quit(cfg);
                return 1;
            }

            /* Run Lua exec function */
            lua_exec_funct(option->timestep_funct, *object, cfg->stats);

            /* Start threads */
            pprintf(PRI_ESSENTIAL, "Starting threads...");
            for (unsigned int k = 0; k < cfg->algo_threads; k++) {
                if (pthread_create(&cfg->threads[k], &thread_attribs,
                                algo->thread_location, cfg->threads_conf[k])) {
                    pprintf(PRI_ERR, "Creating thread %u failed!\n", k);
                    return 1;
                } else {
                    pthread_getcpuclockid(cfg->threads[k],
                                        &phys_stats->t_stats[k].clockid);
                    pprintf(PRI_ESSENTIAL, "%u...", k);
                }
            }
            if (pthread_create(&cfg->control_thread, &thread_attribs, thread_ctrl, cfg)) {
                pprintf(PRI_ERR, "Creating control thread failed!\n");
                return 1;
            } else {
                pprintf(PRI_ESSENTIAL, "C...");
            }
            pprintf(PRI_OK, "\n");
            /* Start threads */

            prev_algo_opts = cfg->algo_opt_map;
            prev_algorithm = (struct list_algorithms *)algo;

            retval = PHYS_STATUS_RUNNING;

            break;
        case PHYS_SHUTDOWN:
            algo = phys_find_algorithm(option->algorithm);
            if (algo->thread_location && !algo->thread_destruction) {
                /* Algorithm == none */
                retval = PHYS_STATUS_STOPPED;
                break;
            } else if (!cfg) {
                retval = PHYS_STATUS_STOPPED;
                break;
            }

            /* Stop threads */
            pprintf(PRI_ESSENTIAL, "Stopping threads...");
            *cfg->quit = true;
            if (cfg->paused || cfg->step_end)
                phys_ctrl(PHYS_PAUSESTART, NULL);
            void *res = PTHREAD_CANCELED; /* Can be any !null pointer */
            for (unsigned int k = 0; k < cfg->algo_threads; k++) {
                while (res) {
                    pthread_join(cfg->threads[k], &res);
                }
                res = PTHREAD_CANCELED;
                pprintf(PRI_ESSENTIAL, "%u...", k);
            }
            while (res) {
                pthread_join(cfg->control_thread, &res);
            }
            pprintf(PRI_ESSENTIAL, "C...");
            pprintf(PRI_OK, "\n");
            /* Stop threads */

            algo->thread_destruction(cfg);

            ctrl_quit(cfg);
            halt_objects = NULL;
            cfg = NULL;

            retval = PHYS_STATUS_STOPPED;

            break;
        default:
            break;
    }
    return retval;
}
