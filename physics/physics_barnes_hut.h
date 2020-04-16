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

#define PHYS_BARNES_HUT {                                                      \
        .name = "barnes-hut",                                                  \
        .version = PACKAGE_VERSION,                                            \
        .desc = "Barnes-Hut simulation, velocity verlet integration",          \
        .author = "Lynne",                                                     \
        .thread_preconfiguration = bhut_preinit,                               \
        .thread_configuration = bhut_init,                                     \
        .thread_location = thread_bhut,                                        \
        .thread_destruction = bhut_quit,                                       \
        .thread_sched_fn = bhut_runtime_fn                                     \
    }

/* Octree structure */
typedef struct phys_barnes_hut_octree {
    vec3 origin, avg_obj_pos;
    double mass;
    long double halfdim;
    struct phys_barnes_hut_octree *parent, *cells[8];
    phys_obj *data;
    unsigned long int score, depth;
    bool leaf;
} bh_octree;

/* Thread assignment tree structure */
typedef struct thread_alloc_tree {
    int assigned;
    bool leaf;
    bh_octree *mapped;
    struct thread_config_bhut *thread, *assign[9];
    struct thread_alloc_tree *parent, *subdiv[8];
} bh_thread;

struct bh_statistics {
    unsigned int bh_total_alloc;
    unsigned int bh_new_alloc;
    unsigned int bh_new_cleaned;
    size_t bh_heapsize;
};

/* Thread configuration struct */
struct thread_config_bhut {
    phys_obj *obj;
    bh_octree *root, *octrees[8], **endpoints;
    unsigned int id, objs_low, objs_high, obj_num, tot_octs, balance_timeout;
    struct bh_statistics *glob_stats;
    struct bh_statistics *stats;
    pthread_barrier_t *ctrl, *barrier;
    pthread_mutex_t *mute, *root_lock;
    volatile bool *quit;
};

/* Returns flux of octrees */
unsigned int bh_cleanup_octree(bh_octree *octree);

/* Completely removes octree */
void bh_decimate_octree(bh_octree *octree);

/* Prints any cells and objects inside octree */
void bh_print_octree(bh_octree *octree);
/* Prints octrees and their dimensions */
void bh_depth_print(bh_octree *octree);

/* Used during init only to get a valid octree data */
double bh_init_max_displacement(phys_obj *object, bh_octree *octree);
void bh_init_center_of_mass(phys_obj *object, bh_octree *octree);

/* Synchs positions */
void bh_cascade_position(bh_octree *target, bh_octree *root,
                        pthread_mutex_t *root_lock);

/* Init a tree(start of one - NOT A SUBCELL) */
bh_octree *bh_init_tree(void);

/* Functions */
void bhut_runtime_fn(struct glob_thread_config *cfg);
void *bhut_preinit(struct glob_thread_config *cfg);
void **bhut_init(struct glob_thread_config *cfg);
void *thread_bhut(void *thread_setts);
void bhut_quit(struct glob_thread_config *cfg);
