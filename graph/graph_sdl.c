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
#include <stdbool.h>
#include <signal.h>
#include "graph.h"
#include "graph_sdl.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "input/sighandle.h"

#include "config.h"

#define MOUSE_ROT_SENS 0.4
#define MOUSE_TR_SENS 0.001

/* Default values(for use in graph_input) */
const struct graph_cam_view def_cam = {
    .view_rotx = 32.0,
    .view_roty = 315.0,
    .view_rotz = 0,
    .tr_x = 0,
    .tr_y = 0,
    .tr_z = 0,
    .scalefactor = 0.005,
};

graph_window *main_win = NULL;
static bool sdl_initd = 0;

int graph_sdl_init(void)
{
    if (!sdl_initd) {
        SDL_Init(SDL_INIT_NOPARACHUTE);
        SDL_Init(SDL_INIT_VIDEO);

        /* OGL settings */
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        /* OGL version */
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        sdl_initd = true;
        return 0;
    }
    return 1;
}

graph_window *graph_win_create(phys_obj *object)
{
    graph_window *win = calloc(1, sizeof(graph_window));
    win->event = calloc(1, sizeof(SDL_Event));

    /* Default options */
    win->camera = def_cam;
    win->object = object;
    graph_set_draw_mode(win, option->default_draw_mode);
    strcpy(win->currentsel, "Select object:");

    /* Spawn window */
    win->window = SDL_CreateWindow(PACKAGE_STRING, \
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, option->width,
        option->height, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|\
        SDL_WINDOW_ALLOW_HIGHDPI);
    /* Create GL context */
    win->context = SDL_GL_CreateContext(win->window);

    /* Init opengl finally */
    graph_init();

    return win;
}

float graph_sdl_resize_wind(graph_window *win)
{
    float aspect_ratio = 0.0;
    if (!win)
        return aspect_ratio;
    SDL_GL_GetDrawableSize(win->window, &option->width, &option->height);
    /* Usually it's the other way around */
    aspect_ratio = (float)option->height/option->width;
    graph_reset_viewport();
    win->camera.aspect_ratio = aspect_ratio;
    return aspect_ratio;
}

int graph_sdl_toggle_fullscreen(graph_window *win)
{
    if (!win)
        return 1;
    if (win->fullscreen) {
        SDL_SetWindowFullscreen(win->window, 0);
        win->fullscreen = 0;
    } else {
        /* Because real fullscreen sucks */
        SDL_SetWindowFullscreen(win->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        win->fullscreen = 1;
    }
    SDL_GL_GetDrawableSize(win->window, &option->width, &option->height);
    graph_sdl_resize_wind(win);
    return 0;
}

void graph_sdl_move_cam(graph_window *win)
{
    if (!win)
        return;
    if (win->flicked || win->translate) {
        SDL_GetRelativeMouseState(&win->mousex, &win->mousey);
        if (win->flicked) {
            win->camera.view_roty += win->mousex*MOUSE_ROT_SENS;
            win->camera.view_rotx += win->mousey*MOUSE_ROT_SENS;
        }
        if (win->translate) {
            win->camera.tr_x += -win->mousex*MOUSE_TR_SENS;
            win->camera.tr_y += win->mousey*MOUSE_TR_SENS;
        }
    }
    if (win->chosen) {
        win->camera.tr_x = win->object[win->chosen].pos[0];
        win->camera.tr_y = win->object[win->chosen].pos[1];
        win->camera.tr_z = win->object[win->chosen].pos[2];
    }
    graph_set_view(win);
}

void graph_sdl_swapwin(graph_window *win)
{
    SDL_GL_SwapWindow(win->window);
}

int graph_sdl_deinit(void)
{
    if (sdl_initd) {
        SDL_Quit();
        sdl_initd = 0;
        return 0;
    }
    return 1;
}

void graph_win_destroy(graph_window *win) {
    if (!win)
        return;
    SDL_GL_DeleteContext(win->context);
    SDL_DestroyWindow(win->window);
    free(win->event);
    free(win);
    graph_quit();
}
