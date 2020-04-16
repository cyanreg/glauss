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

/*	Dependencies	*/
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "physics/physics_aux.h"

/* Camera info */
struct graph_cam_view {
    float view_rotx, view_roty, view_rotz;
    float tr_x, tr_y, tr_z;
    float scalefactor, aspect_ratio;
};

typedef struct {
    /* TODO: Align this mess */

    /* SDL */
    SDL_Window *window;
    SDL_GLContext context;
    SDL_Event *event;

    /* Graph */
    int draw_mode;

    /* Status */
    bool flicked, translate, fullscreen, start_selection;
    int mousex, mousey, initmousex, initmousey;
    unsigned int chosen, currentnum;
    struct graph_cam_view camera;
    struct numbers_selection numbers;
    char currentsel[100];
    float fps;

    /* Physics */
    phys_obj *object;
} graph_window;

/* Default camera position and scale */
extern const struct graph_cam_view def_cam;

int graph_sdl_init(void);
int graph_sdl_deinit(void);
graph_window *graph_win_create(phys_obj *object);
float graph_sdl_resize_wind(graph_window *win);
int graph_sdl_toggle_fullscreen(graph_window *win);
void graph_sdl_move_cam(graph_window *win);
void graph_sdl_swapwin(graph_window *win);
void graph_win_destroy(graph_window *win);
