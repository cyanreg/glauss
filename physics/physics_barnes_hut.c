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
#include <malloc.h>
#include <signal.h>
#include <limits.h>
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "input/parser.h"
#include "physics.h"
#include "physics_barnes_hut.h"

/* Repeated indent string when printing */
#define INDENT "  "

/* Thread local storage allocation stats */
static _Thread_local unsigned int allocated_cells = 0;

/* Maximum octrees, calculated from option->bh_heapsize_max */
static unsigned int bh_octrees_max = 0;

/* Algorithm specific options */
static double          bh_ratio = 0.5;
static double          bh_balance_threshold = 0.0;
static unsigned short  bh_tree_limit = 8;
static unsigned short  bh_lifetime = 24;
static size_t          bh_heapsize_max = 336870912;
static unsigned int    bh_balance_timeout = 175;
static bool            bh_single_assign = true;
static bool            bh_random_assign = true;
static bool            bh_periodic_boundary = false;
static bool            bh_viscosity = false;
static unsigned short  bh_viscosity_cutoff = 1;
static double          bh_viscosity_ratio = 0.5;
static double          bh_boundary_size = 10.0;

/* Used in qsort to assign threads in octrees */
static int thread_cmp_assigned(const void *a, const void *b)
{ return ((*(bh_thread **)a)->assigned - (*(bh_thread **)b)->assigned); }

/* Will distribute the threads in each assignment tree */
static void bh_add_thread(bh_thread *root,
                        bh_octree *octree, struct thread_config_bhut *thread)
{
    /* Not the easiest code to understand without comments. */
    for (uint8_t i = 0; i < 8; i++) {
        /* Init thread subdivs and octree cells if not done already */
        if (!root->subdiv[i])
            root->subdiv[i] = calloc(1, sizeof(bh_thread));
        if (!octree->cells[i]) {
            octree->cells[i] = bh_init_tree();
            octree->cells[i]->halfdim = octree->halfdim/2;
            octree->cells[i]->origin = octree->origin + ((double)octree->halfdim)*0.5*\
                (vec3){ (i&4 ? 1 : -1), (i&2 ? 1 : -1), (i&1 ? 1 : -1) };
        }
        octree->cells[i]->depth = octree->depth + 1;
        root->subdiv[i]->mapped = octree->cells[i];
        root->subdiv[i]->parent = root;
        root->subdiv[i]->assigned = 0;
    }
    /* Assign the thread to the root octree */
    root->assign[root->assigned] = thread;
    /* Wipe other threads' previous octree assignments */
    for (uint8_t v = 0; v < root->assigned + 1; v++) {
        if (root->assign[v]) {
            for (uint8_t d = 0; d < 8; d++) {
                root->assign[v]->octrees[d] = NULL;
            }
        }
    }
    /* Split the octree and assign the threads */
    short int distrb = 8/root->assigned, remain = 8%root->assigned, oct = 0;
    bool oct_assignments[8] = {false};

    for (int k = 1; k < root->assigned + 1; k++) {
        /* Calculate the threads' share. Add the calculated remain to the first ones. */
        int thread_conts = distrb + ((remain-- > 0) ? 1 : 0);

        root->assign[k]->tot_octs = thread_conts;

        for (int l = 0; l < thread_conts; l++) {
            if (bh_random_assign) {
                /* Map threads to random subtrees */
                while (1) {
                    oct = (uint8_t)(8*((float)rand()/RAND_MAX));
                    if (oct_assignments[oct])
                        continue;
                    /* Map the octrees to the real BH octree */
                    root->assign[k]->octrees[oct] = root->subdiv[oct]->mapped;
                    /* Assign the threads within the octree from the root. */
                    root->subdiv[oct]->assign[++root->subdiv[oct]->assigned] = root->assign[k];
                    oct_assignments[oct] = true;
                    break;
                }
            } else {
                /* Map the octrees sequentially to the subtrees */
                root->assign[k]->octrees[oct] = root->subdiv[oct]->mapped;
                /* Assign the threads within the octree from the root. */
                root->subdiv[oct]->assign[++root->subdiv[oct]->assigned] = root->assign[k];
                oct++;
            }
        }
    }
    return;
}

/* Recursive function to assign threads to octrees */
static void bh_assign_thread(bh_thread *root,
                            bh_octree *octree,
                            struct thread_config_bhut *thread)
{
    if (!thread || !root) {
        return;
    } else if (++root->assigned < bh_tree_limit + 1) {
        bh_add_thread(root, octree, thread);
    } else {
        /* Create backup to prevent qsort from scrambling them.
        * Otherwise we'd have to recursively update their origins. Not fun. */
        struct thread_alloc_tree *keep[8];
        for (uint8_t i = 0; i < 8; i++)
            keep[i] = root->subdiv[i];

        /* Sort: lowest to highest, [0]'s always the least assigned one. */
        qsort(keep, 8, sizeof(bh_thread *), thread_cmp_assigned);

        /* Find the lowest occupied subcell's original octree subcell */
        for (uint8_t i = 0; i < 8; i++) {
            if (root->subdiv[i] == keep[0]) {
                /* Insert the thread into it */
                bh_assign_thread(root->subdiv[i], root->subdiv[i]->mapped, thread);
                break;
            }
        }
    }
    return;
}

static void bh_decimate_assignment_tree(bh_thread *root)
{
    for (uint8_t i=0; i < 8; i++) {
        if (root->subdiv[i])
            bh_decimate_assignment_tree(root->subdiv[i]);
    }
    free(root);
}

static void bh_print_thread_tree(struct thread_config_bhut **thread,
                                unsigned int threads)
{
    for (unsigned int m = 0; m < threads; m++) {
        pprintf(PRI_HIGH, "   %02i   | ", m);
        for (uint8_t h=0; h < 8; h++) {
            if (thread[m]->octrees[h]) {
                pprintf(PRI_HIGH, "%i(%u) ", h, thread[m]->octrees[h]->depth);
            }
        }
        pprintf(PRI_HIGH, "\n");
    }
}

void *bhut_preinit(struct glob_thread_config *cfg)
{
    /* Attempt to balance out the load every cycle */
    cfg->thread_sched_fn_freq = 1;

#ifdef __linux__
    /* Use this size for glibc's fastbins. In theory any memory below this size
    * will not be consolidated together, allowing us to allocate and free
    * memory real fast. Not portable. */
    mallopt(M_MXFAST, sizeof(bh_octree));
#endif

    /* Global stats. You are responsible for updating them. Use the exec f-n */
    struct bh_statistics *global_stats = calloc(1, sizeof(struct bh_statistics));
    cfg->algo_global_stats_map = \
        allocate_parser_map((struct parser_map []){
            {"bh_total_alloc",   P_TYPE(global_stats->bh_total_alloc)   },
            {"bh_new_alloc",     P_TYPE(global_stats->bh_new_alloc)     },
            {"bh_new_cleaned",   P_TYPE(global_stats->bh_new_cleaned)   },
            {"bh_heapsize",      P_TYPE(global_stats->bh_heapsize)      },
            {0},
        });
    cfg->algo_global_stats_raw = global_stats;

    /* Thread stats. Just stash their pointer in cfg->algo_thread_stats_raw
    * and allocate cfg->algo_thread_stats_map */
    struct bh_statistics **thread_stats = calloc(cfg->algo_threads, sizeof(struct bh_statistics *));
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_stats[k] = calloc(1, sizeof(struct bh_statistics));
        cfg->algo_thread_stats_map[k] = \
            allocate_parser_map((struct parser_map []){
                {"bh_total_alloc",   P_TYPE(thread_stats[k]->bh_total_alloc)   },
                {"bh_new_alloc",     P_TYPE(thread_stats[k]->bh_new_alloc)     },
                {"bh_new_cleaned",   P_TYPE(thread_stats[k]->bh_new_cleaned)   },
                {"bh_heapsize",      P_TYPE(thread_stats[k]->bh_heapsize)      },
                {0},
            });
    }
    cfg->algo_thread_stats_raw = (void **)thread_stats;

    /* physics_ctrl will call parser and read them, just submit them now */
    cfg->algo_opt_map = \
        allocate_parser_map((struct parser_map []){
            /* C can be a beautiful language sometimes too            */
            {"bh_single_assign",       P_TYPE(bh_single_assign)       },
            {"bh_random_assign",       P_TYPE(bh_random_assign)       },
            {"bh_ratio",               P_TYPE(bh_ratio)               },
            {"bh_lifetime",            P_TYPE(bh_lifetime)            },
            {"bh_balance_timeout",     P_TYPE(bh_balance_timeout)     },
            {"bh_heapsize_max",        P_TYPE(bh_heapsize_max)        },
            {"bh_tree_limit",          P_TYPE(bh_tree_limit)          },
            {"bh_balance_threshold",   P_TYPE(bh_balance_threshold)   },
            {"bh_periodic_boundary",   P_TYPE(bh_periodic_boundary)   },
            {"bh_boundary_size",       P_TYPE(bh_boundary_size)       },
            {"bh_viscosity",           P_TYPE(bh_viscosity)           },
            {"bh_viscosity_cutoff",    P_TYPE(bh_viscosity_cutoff)    },
            {"bh_viscosity_ratio",     P_TYPE(bh_viscosity_ratio)     },
            {0},
        });

    /* We manage the free()ing of every map and global/thread_stats_init */
    /* Yes absolutely every memory allocation here relating to maps is freed by ctrl_quit. */
    /* I'll say this again, NO NEED TO FREE ANY MEMORY RELATING TO MAPS! */

    /* You still need to free the structures hosting the stats however! */

    return NULL;
}

void **bhut_init(struct glob_thread_config *cfg)
{
    struct thread_config_bhut **thread_config = calloc(cfg->algo_threads,
                                            sizeof(struct thread_config_bhut*));
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k] = calloc(1, sizeof(struct thread_config_bhut));
    }

    /* Check if options are within their limits */
    if (bh_tree_limit  < 2 || bh_tree_limit > 8) {
        pprintf(PRI_ERR, "[BH] Option bh_tree_limit must be within [2,8]!");
        exit(1);
    }
    if (bh_ratio > 1.0 || bh_ratio < 0.0) {
        pprint_warn("Option bh_ratio does not belong to [0, 1]. Fixing to ");
        if (bh_ratio > 1.0) {
            pprint("1.0.\n");
            bh_ratio = 1.0;
        } else {
            pprint("0.0.\n");
            bh_ratio = 0.0;
        }
    }

    /* Calculate maximum octrees */
    bh_octrees_max = (bh_heapsize_max/sizeof(bh_octree))+1;

    /* Init root mutex */
    pthread_mutex_t *root_lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(root_lock, NULL);

    /* Init main thread mutex */
    pthread_mutex_t *mute = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(mute, NULL);

    /* Init barrier */
    pthread_barrier_t *barrier = calloc(1, sizeof(pthread_barrier_t));
    pthread_barrier_init(barrier, NULL, cfg->algo_threads);

    /* Create the root octree */
    bh_octree *root_octree = bh_init_tree();
    bh_init_center_of_mass(cfg->obj, root_octree);
    root_octree->halfdim = bh_init_max_displacement(cfg->obj, root_octree);

    /* Create endpoints array pointing at the octree an object is in */
    bh_octree **endpoints = calloc(cfg->obj_num + 1, sizeof(bh_octree *));

    /* Create temporary assignment tree for threads */
    bh_thread *thread_tree = calloc(1, sizeof(bh_thread));

    int totcore = (int)((float)cfg->obj_num/cfg->algo_threads);
    for (int k = 0; k < cfg->algo_threads; k++) {
        thread_config[k]->root = root_octree;
        /* Recursively insert the threads into assignment octree and map it to
        * the root octree */
        bh_assign_thread(thread_tree, root_octree, thread_config[k]);
        thread_config[k]->glob_stats = cfg->algo_global_stats_raw;
        thread_config[k]->stats = cfg->algo_thread_stats_raw[k];
        thread_config[k]->obj = cfg->obj;
        thread_config[k]->obj_num = cfg->obj_num;
        thread_config[k]->ctrl = cfg->ctrl;
        thread_config[k]->endpoints = endpoints;
        thread_config[k]->barrier = barrier;
        thread_config[k]->mute = mute;
        thread_config[k]->root_lock = root_lock;
        thread_config[k]->quit = cfg->quit;
        thread_config[k]->id = k;
        thread_config[k]->objs_low = !k ? 0 : thread_config[k-1]->objs_high;
        thread_config[k]->objs_high = thread_config[k]->objs_low + totcore - 1;
        if (k == cfg->algo_threads - 1) {
            /*	Takes care of rounding problems with odd numbers.	*/
            thread_config[k]->objs_high+=cfg->obj_num-thread_config[k]->objs_high;
        }
    }

    /* When operating on 1 thread */
    if (cfg->algo_threads == 1 && bh_single_assign) {
        for (uint8_t h = 0; h < 8; h++) {
            thread_config[0]->octrees[h] = NULL;
        }
        thread_config[0]->octrees[0] = root_octree;
        thread_config[0]->tot_octs = 1;
    } else {
        pprintf(PRI_HIGH, " Thread | Assignments: oct(lvl)\n");
        bh_print_thread_tree(thread_config, cfg->algo_threads);
    }

    /* Free assignment octree since all's been done with */
    bh_decimate_assignment_tree(thread_tree);

    return (void**)thread_config;
}

/* Deinit function */
void bhut_quit(struct glob_thread_config *cfg) {
    struct thread_config_bhut **t = (struct thread_config_bhut **)cfg->threads_conf;

    free(cfg->algo_thread_stats_raw);
    free(cfg->algo_global_stats_raw);

    /* Root mutex */
    pthread_mutex_destroy(t[0]->root_lock);
    free(t[0]->root_lock);

    /* Barrier */
    pthread_barrier_destroy(t[0]->barrier);
    free(t[0]->barrier);

    /* Mutex */
    pthread_mutex_destroy(t[0]->mute);
    free(t[0]->mute);

    /* Main octree */
    bh_decimate_octree(t[0]->root);

    for (int k = 0; k < cfg->algo_threads; k++) {
        free(t[k]);
    }
    free(t);

    return;
}

void bhut_runtime_fn(struct glob_thread_config *cfg)
{
    struct thread_config_bhut **t = (struct thread_config_bhut **)cfg->threads_conf;

    unsigned int bh_total_alloc = 0;
    unsigned int bh_new_alloc = 0;
    unsigned int bh_new_cleaned = 0;
    size_t bh_heapsize = 0;

    for (int k = 0; k < cfg->algo_threads; k++) {
        bh_total_alloc += t[k]->stats->bh_total_alloc;
        bh_new_alloc += t[k]->stats->bh_new_alloc;
        bh_new_cleaned += t[k]->stats->bh_new_cleaned;
        bh_heapsize += t[k]->stats->bh_heapsize;
    }

    /* Update global stats. Could use cfg->algo_global_stats_raw but we'd need
    * to cast that pointer. */
    t[0]->glob_stats->bh_total_alloc  = bh_total_alloc;
    t[0]->glob_stats->bh_new_alloc    = bh_new_alloc;
    t[0]->glob_stats->bh_new_cleaned  = bh_new_cleaned;
    t[0]->glob_stats->bh_heapsize     = bh_heapsize;

    if (!bh_balance_threshold)
        return;

    if (cfg->stats->total_steps < bh_balance_timeout || cfg->algo_threads == 1)
        return;

    unsigned int count_oct[cfg->algo_threads], alloc_oct[cfg->algo_threads];
    unsigned int max_oct = 0, t_index_max = 0;

    for (int k = 0; k < cfg->algo_threads; k++) {
        count_oct[k] = alloc_oct[k] = 0;
        for (uint8_t i = 0; i < 8; i++) {
            if (t[k]->octrees[i]) {
                count_oct[k]++;
            }
        }
        alloc_oct[k] = t[k]->stats->bh_total_alloc;
        if (alloc_oct[k] > max_oct) {
            max_oct = alloc_oct[k];
            t_index_max = k;
        }
    }

    unsigned int min_oct = max_oct, t_index_min = 0;
    for (int k = 0; k < cfg->algo_threads; k++) {
        if (min_oct > alloc_oct[k]) {
            min_oct = alloc_oct[k];
            t_index_min = k;
        }
    }

    double bal_ratio = fabs(((double)alloc_oct[t_index_min]/\
                                    alloc_oct[t_index_max])-1);

    if (!isnormal(bal_ratio) || bal_ratio < bh_balance_threshold)
        return;

    if (t[t_index_min]->balance_timeout) {
        t[t_index_min]->balance_timeout--;
        return;
    }

    if (t[t_index_max]->balance_timeout) {
        t[t_index_max]->balance_timeout--;
        return;
    }

    bh_octree **src = NULL;
    bh_octree **dest = NULL;
    bh_octree **temp = NULL;

    uint8_t i;
    for (i = 0; i < 8; i++) {
        src = &t[t_index_max]->octrees[i];
        if (src)
            break;
    }
    if (!src)
        return;
    unsigned int rand_index = (unsigned int)((rand()/(double)RAND_MAX)*8+0.5);
    dest = temp = &t[t_index_min]->octrees[rand_index];
    *dest = *src;

    printf("[BH] Balance = %lf => ", bal_ratio);
    if (*temp) {
        *src = *temp;
        printf("Swap octrees: threads %i(%i)<->%u(%i)\n",
            t_index_min, rand_index, t_index_max, i);
    } else {
        *src = NULL;
        t[t_index_min]->tot_octs++;
        t[t_index_max]->tot_octs--;
        printf("Move octree: threads %i(%i)->%u(%i)\n",
            t_index_min, rand_index, t_index_max, i);
    }

    t[t_index_min]->balance_timeout = bh_balance_timeout;
    t[t_index_max]->balance_timeout = bh_balance_timeout;
}

/* Returns nearest larger even integer plus a little offset to dither stuff */
static inline long double bh_even_round(double num)
{
    long double dither = ((rand() + 1) & ~1)/((RAND_MAX-1)*10000.0);
    return (long double)(((unsigned int)(fabs(num) + 2)) & ~1)+dither;
}

/* INIT ONLY: Returns furthest object's furthest position from an octree. */
double bh_init_max_displacement(phys_obj *object, bh_octree *octree)
{
    double maxdist = 0.0;
    for (unsigned int i = 0; i < option->obj; i++) {
        maxdist = fmax(VEC3_LEN(object[i].pos - octree->origin), maxdist);
    }
    return bh_even_round(maxdist);
}

/* INIT ONLY: Will calculate and update an octree's center of mass */
void bh_init_center_of_mass(phys_obj *object, bh_octree *octree)
{
    vec3 center = (vec3){0,0,0};
    for (unsigned int i = 0; i < option->obj; i++) {
        center = (center + object[i].pos)/2;
    }
    octree->origin = octree->avg_obj_pos = center;
}

/* Completely free an octree */
void bh_decimate_octree(bh_octree *octree)
{
    for (uint8_t i=0; i < 8; i++) {
        if (octree->cells[i]) {
            bh_decimate_octree(octree->cells[i]);
        }
    }
    free(octree);
}

/* Recursive function to scrub off all objects, update scores and reset mass. */
static bool bh_clean_octree(bh_octree *octree)
{
    octree->mass = 0;
    if (octree->data) {
        /* Object was here, chances are cell will be used, do not free yet */
        octree->data = NULL;
        return 0;
    } else {
        /* No object, if end node we reduce its score and check if 0 */
        if (octree->leaf)
            return !octree->score-- ? 1 : 0;
        /* Not end node, recursively check non-empty cells */
        uint8_t filled_cells = 8;
        for (uint8_t i = 0; i < 8; i++) {
            if (octree->cells[i]) {
                if (bh_clean_octree(octree->cells[i])) {
                    /* Since cell is empty, free it */
                    free(octree->cells[i]);
                    octree->cells[i] = NULL;
                    filled_cells--;
                    allocated_cells--;
                }
            } else filled_cells--;
        }
        if (!filled_cells) {
            /* No child cells, set as leaf and reduce score */
            octree->leaf = 1;
            return !octree->score-- ? 1 : 0;
        } else
            return 0;
    }
}

/* Wrapper function to return a threads' new allocations */
unsigned int bh_cleanup_octree(bh_octree *octree)
{
    if (!octree)
        return 0;
    unsigned int prev_allocated_cells = allocated_cells;
    bh_clean_octree(octree);
    return prev_allocated_cells - allocated_cells;
}

/* Returns the octant of an octree a position is in */
static inline uint8_t bh_get_octant(vec3 *pos, bh_octree *octree)
{
    uint8_t oct = 0;
    if ((*pos)[0] >= octree->origin[0])
        oct |= 4;
    if ((*pos)[1] >= octree->origin[1])
        oct |= 2;
    if ((*pos)[2] >= octree->origin[2])
        oct |= 1;
    return oct;
}

/* Inits a cell, in case it isn't, updates score and position */
static void bh_init_cell(bh_octree *octree, uint8_t k)
{
    if (allocated_cells > bh_octrees_max) {
        pprint_err("Reached maximum octree heapsize of %lu bytes!\n"
                "Possible errors: two objects sharing the same location,"
                "check by calling phys_check_collisions!\n",
                bh_heapsize_max);
        phys_urgent_dump();
        raise(9);
    }

    /* Allocate cell */
    if (!octree->cells[k]) {
        octree->cells[k] = calloc(1, sizeof(bh_octree));
        octree->cells[k]->depth = octree->depth+1;
        octree->cells[k]->leaf = true;
        octree->cells[k]->score = bh_lifetime;
        octree->cells[k]->parent = octree;
        allocated_cells++;
    }

    /* Scoring system: object is in therefore increase score */
    if (octree->cells[k]->score < bh_lifetime)
        octree->cells[k]->score++;

    /* Update the positions and dimensions. Yes, it is the best place. */
    octree->cells[k]->halfdim = octree->halfdim/2;
    octree->cells[k]->origin = octree->origin + (double)octree->halfdim*0.5*\
        (vec3){ (k&4 ? 1 : -1), (k&2 ? 1 : -1), (k&1 ? 1 : -1) };

    return;
}

/* Recursive function to insert object into an octree */
static bh_octree *bh_insert_object(phys_obj *object, bh_octree *octree)
{
    /* Update octree mass/center of mass. */
    octree->avg_obj_pos = (octree->avg_obj_pos+object->pos)/2;
    octree->mass += object->mass;
    if (!octree->data && octree->leaf) {
        /* This cell has no object or subcells */
        octree->data = object;
        return octree; /* Cascade octree location */
    } else if (octree->data && octree->leaf) {
        /* This cell has object but no subcells */
        uint8_t oct_current_obj = bh_get_octant(&object->pos, octree);
        uint8_t oct_octree_obj = bh_get_octant(&octree->data->pos, octree);

        /* Prep and transfer octee object */
        bh_init_cell(octree, oct_octree_obj);
        bh_insert_object(octree->data, octree->cells[oct_octree_obj]);

        /* Insert current object */
        bh_init_cell(octree, oct_current_obj);

        /* No longer leaf */
        octree->data = NULL;
        octree->leaf = false;

        /* Recurse and cascade */
        return bh_insert_object(object, octree->cells[oct_current_obj]);
    } else {
        /* This cell has subcells with objects */
        uint8_t oct_current_obj = bh_get_octant(&object->pos, octree);
        bh_init_cell(octree, oct_current_obj);
        return bh_insert_object(object, octree->cells[oct_current_obj]);
    }
}

void bh_print_octree(bh_octree *octree)
{
    if (octree->data) {
        for(int i = 0; i < octree->depth; i++) pprintf(PRI_SPAM, INDENT);
        pprintf(PRI_SPAM, "Object %i(pos = {%f, %f, %f}) is at cell level %i\n",
                octree->data->id, octree->data->pos[0], octree->data->pos[1],
                octree->data->pos[2], octree->depth);
    } else {
        for (uint8_t i = 0; i < 8; i++) {
            if (octree->cells[i]) {
                for (int i = 0; i < octree->depth; i++)
                    pprintf(PRI_SPAM, INDENT);
                pprintf(PRI_SPAM,
                        "Cell(pos = {%f, %f, %f},\
                        mass(center) = {%f, %f, %f},\
                        mass = %lf) contents:\n", octree->cells[i]->origin[0],
                        octree->cells[i]->origin[1],
                        octree->cells[i]->origin[2],
                        octree->cells[i]->avg_obj_pos[0],
                        octree->cells[i]->avg_obj_pos[1],
                        octree->cells[i]->avg_obj_pos[2],
                        octree->cells[i]->mass);
                bh_print_octree(octree->cells[i]);
            } else {
                for (int i = 0; i < octree->depth; i++)
                    pprintf(PRI_SPAM, INDENT);
                pprintf(PRI_SPAM, "Cell empty.\n",  octree->origin[0],
                        octree->origin[1], octree->origin[2]);
            }
        }
    }
}

/* Just a debug tool */
void bh_depth_print(bh_octree *octree)
{
    if (!octree)
        return;
    pprintf(PRI_ESSENTIAL, "Oct depth = %i, halfdim = %0.100Lf\n",
            octree->depth, octree->halfdim);
    for (uint8_t i=0; i < 8; i++) {
        if (octree->cells[i])
            bh_depth_print(octree->cells[i]);
    }
}

/* Will init a sub-root octree as root. We have to do the sync ourselves. */
bh_octree *bh_init_tree(void)
{
    bh_octree *octree = calloc(1, sizeof(bh_octree));
    octree->score = USHRT_MAX;
    octree->leaf = true;
    octree->parent = NULL;
    return octree;
}

/* Check whether the object is in the target octree. Returns 1 in case it is. */
static inline bool bh_recurse_check_obj(phys_obj *object, bh_octree *target, bh_octree *root)
{
    if (!root)
        return 0;
    if (root->depth >= target->depth)
        return root == target;
    /* Cascade the return value */
    else return bh_recurse_check_obj(object, target,
                                root->cells[bh_get_octant(&object->pos, root)]);
}

/* Recursive function, basis of the BH algorithm */
static void bh_calculate_force(phys_obj *object, bh_octree *octree)
{
    vec3 vecnorm = object->pos - octree->origin;
    double dist = sqrt(vecnorm[0]*vecnorm[0] +\
                    vecnorm[1]*vecnorm[1] + vecnorm[2]*vecnorm[2]);
    vecnorm /= dist;
    if (octree->data) {
        if (octree->data == object)
            return;
        object->acc += -vecnorm*\
        (option->gconst*octree->data->mass)/(dist*dist);
    } else if ((octree->halfdim/dist) < bh_ratio) {
        object->acc += -vecnorm*\
        (option->gconst*octree->mass)/(dist*dist);
    } else {
        for (uint8_t i=0; i < 8; i++) {
            if (octree->cells[i]) {
                bh_calculate_force(object, octree->cells[i]);
            }
        }
    }
}

static inline unsigned int bh_build_octree(phys_obj *object, bh_octree *octree,
                                        bh_octree *root, bh_octree **endpoints)
{
    unsigned int prev_allocated_cells = allocated_cells;
    if (!octree || !root || !object)
        return 0;
    for (unsigned int i = 0; i < option->obj; i++) {
        if (bh_recurse_check_obj(&object[i], octree, root)) {
            endpoints[i] = bh_insert_object(&object[i], octree);
        }
    }
    return allocated_cells - prev_allocated_cells;
}

/* Will sync the centre of mass and origin of all related octrees root-target */
static void bh_cascade_mass(bh_octree *target, bh_octree *root,
                            pthread_mutex_t *root_lock)
{
    /* Will not update target */
    if (!root || !target)
        return;
    if (target->depth >= root->depth)
        return;
    pthread_mutex_lock(root_lock);
        root->avg_obj_pos = (root->avg_obj_pos+target->avg_obj_pos)/2;
        root->mass += target->mass;
    pthread_mutex_unlock(root_lock);
    bh_cascade_mass(target, root->cells[bh_get_octant(&target->origin, root)],
                    root_lock);
}

/* Sets the root's position and dimensions, resets mass */
static void bh_atomic_update_root(long double dimension, bh_octree *root,
                                pthread_mutex_t *root_lock)
{
    if (!root)
        return;
    pthread_mutex_lock(root_lock);
        root->mass = 0;
        root->origin = root->avg_obj_pos;
        root->halfdim = dimension;
    pthread_mutex_unlock(root_lock);
}

static void bh_force_periodic_bound(phys_obj *obj, bh_octree *root)
{
    return;
    if (!root)
        return;
    vec3 vecnorm = VEC3_NORM_DIV_DIST2(obj->pos - root->origin);
    obj->acc += vecnorm*(option->gconst*root->mass);
}

/* Scan nearby trees and apply a LJ force to emulate viscosity/bonds */
void bh_neighbour_viscosity(phys_obj *obj, bh_octree *oct, int surface)
{
    if (!oct || !oct->parent || !obj)
        return;

    for(uint8_t s = 0; s < 8; s++) {
        bh_octree *sub = surface > 0 ? oct->parent->cells[s] : oct->cells[s];
        if (sub && sub != oct) {
            if (sub->data && sub->data != obj) {
                phys_lennard_jones_force(obj, sub->data);
            } else if(sub->cells[s] && surface) {
                bh_neighbour_viscosity(obj, sub->cells[s], -(abs(surface)-1));
            }
        }
    }

    if (surface >= bh_viscosity_cutoff || surface == 0) { /* Stop */
        return;
    } else { /* Up */
        bh_neighbour_viscosity(obj, oct->parent, surface++);
    }
}

/* Sets the origins and dimensions of any thread octrees and in between root. */
void bh_cascade_position(bh_octree *target, bh_octree *root,
                        pthread_mutex_t *root_lock)
{
    /* Will update the target as well */
    if (!root || !target)
        return;
    if (!target->depth)
        return;

    uint8_t oct = bh_get_octant(&target->origin, root);
    if (!root->cells[oct])
        return;

    pthread_mutex_lock(root_lock);
        root->mass = 0;
        root->cells[oct]->halfdim = root->halfdim/2;
        root->cells[oct]->origin = root->origin + ((double)root->halfdim)*0.5*\
            (vec3){ (oct&4 ? 1 : -1), (oct&2 ? 1 : -1), (oct&1 ? 1 : -1) };
    pthread_mutex_unlock(root_lock);

    if (root->cells[oct]->depth >= target->depth)
        return;
    else
        bh_cascade_position(target, root->cells[oct], root_lock);
}

void *thread_bhut(void *thread_setts)
{
    struct thread_config_bhut *t = thread_setts;
    const double dt = option->dt;
    vec3 accprev = (vec3){0,0,0};

    while (!*t->quit) {
        unsigned int new_alloc = 0, new_cleaned = 0;
        double maxdist = 0.0;
        /* Move objects */
        for (unsigned int i = t->objs_low; i < t->objs_high + 1; i++) {
            if (t->obj[i].ignore)
                continue;
            t->obj[i].pos += (t->obj[i].vel*dt) + (t->obj[i].acc)*((dt*dt)/2);
            if (bh_periodic_boundary) {
                for (uint8_t j = 0; j < 3; j++) {
                    if (fabs(t->obj[i].pos[j]) > bh_boundary_size) {
                        t->obj[i].pos[j] = (t->obj[i].pos[j] > 0 ? -1 : 1)*\
                                (2*bh_boundary_size - fabs(t->obj[i].pos[j]));
                    }
                }
            }
        }

        pthread_barrier_wait(t->barrier);

        /* Build octree */
        for (uint8_t s=0; s < 8; s++) {
            new_alloc += bh_build_octree(t->obj, t->octrees[s], t->root, t->endpoints);
            /* Sync mass and center of mass with any higher trees */
            bh_cascade_mass(t->octrees[s], t->root, t->root_lock);
        }

        pthread_barrier_wait(t->barrier);

        /* Calculate force */
        for (unsigned int i = t->objs_low; i < t->objs_high + 1; i++) {
            accprev = t->obj[i].acc;
            bh_calculate_force(&t->obj[i], t->root);
            if (bh_periodic_boundary) { /* Periodic boundary */
                bh_force_periodic_bound(&t->obj[i], t->root);
            }
            if (bh_viscosity) { /* Viscosity */
                bh_neighbour_viscosity(&t->obj[i], t->endpoints[i], 1);
            }
            t->obj[i].vel += (t->obj[i].acc + accprev)*((dt)/2);
            /* Get updated maximum for root while we're at it. */
            maxdist = fmax(VEC3_LEN(t->obj[i].pos - t->root->origin), maxdist);
        }

        /* Insert updated halfdim into root */
        bh_atomic_update_root(bh_even_round(maxdist), t->root, t->root_lock);

        /* Update the positions of octrees and their halfdims + cleanup */
        for (uint8_t s = 0; s < 8; s++) {
            new_cleaned += bh_cleanup_octree(t->octrees[s]);
            bh_cascade_position(t->octrees[s], t->root, t->root_lock);
        }

        /* Update stats */
        t->stats->bh_total_alloc  =  allocated_cells;
        t->stats->bh_new_alloc    =  new_alloc;
        t->stats->bh_new_cleaned  =  new_cleaned;
        t->stats->bh_heapsize     =  sizeof(bh_octree)*allocated_cells;

        /* Sync & wakeup control thread */
        phys_ctrl_wait(t->ctrl);
    }
    return 0;
}
