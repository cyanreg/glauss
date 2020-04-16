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

#include "physics.h"

extern struct atomic_cont {
    char *name;
    double mass;
    int number;
    float color[4];
} *atom_prop;

enum num_cmd {
    NUM_ANOTHER,
    NUM_GIVEME,
    NUM_REMOVE,
};

#define DIGITS_MAX 20

struct numbers_selection {
    int digits[DIGITS_MAX];
    unsigned short int final_digit;
};

struct phys_obj_collisions {
    vec3 pos;
    unsigned int *obj_ids, tot_coll_local;
    /* Example: tot_coll_local = 1; => obj_ids[0] and obj_ids[1] exist */
    /* The last collision will have tot_coll_local = 0 */
};

/* Elements system. Submit a NULL filepath to use compiled internal DB */
unsigned short int return_atom_num(const char *name);
char *return_atom_str(unsigned int num);
void free_elements();

/* Shuffle through all algorithms available and change option->algorithm */
void phys_shuffle_algorithms(void);

/* Function to turn individual numbers into a single number. See graph_input
* for example of how to use */
int getnumber(struct numbers_selection *numbers, int currentdigit,
            enum num_cmd status);

/* Lennard-Jones potential force, see documentation */
void phys_lennard_jones_force(phys_obj *obj, phys_obj *end);

/* Rotate vector by angles specified in rot1 */
void rotate_vec(vec3 *vec, vec3 *rot1);

/* Checks for shared coordinates in entire object array */
unsigned int phys_check_collisions(phys_obj *object,
                                unsigned int low, unsigned int high);

/* Counter, will reset the counter variable once it ticks over */
bool phys_timer_exec(unsigned int freq, unsigned int *counter);

/* Check coords for any collisions */
bool phys_check_coords(vec3 *vec, phys_obj *object,
                    unsigned int low, unsigned int high);

/* Structured collision checking */
struct phys_obj_collisions *phys_return_collisions(phys_obj *object, unsigned int count);

/* Return the CLOCK_MONOTONIC(RAW) time in microseconds */
unsigned long long int phys_gettime_us(void);

/* Miliseconds sleep, uses nanosleep(will also this), will carry over a second */
int phys_sleep_msec(long unsigned int time);
