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
#include <stdbool.h>

/* Options struct */
/* "//--" denotes internal, non-parser settable variables */

extern struct option_struct {
    /* Frontend */
    unsigned short verbosity;
    float def_radius;
    int width, height, fontsize;
    bool logenable, debug;
    FILE *logfile; //--
    bool novid;

    /* Graph */
    char *lua_print;
    float bgcolor[4];

    /* Input */
    char *fontname, *filename, *algorithm, *sshot_temp, *xyz_temp;
    char *elements_file, *simconf_id;
    char *spawn_funct, *timestep_funct;
    char *thread_schedule_mode;
    char *custom_sprite_png;
    char *default_draw_mode;
    unsigned int exec_funct_freq, skip_model_vec, lua_gc_sweep_freq;
    bool lua_expose_obj_array;
    bool input_thread_enable;
    unsigned long long int final_frame;

    /* Physics */
    double dt;
    unsigned int obj; //--
    unsigned short threads;
    unsigned int rng_seed;
    double gconst;
    unsigned int step_back_buffer;

    /* Physics - ctrl */
    unsigned int dump_xyz, dump_sshot;
    volatile bool write_sshot_now, quit_main_now;

    /* For all other physics options we let the algorithms handle it */
} *option;
