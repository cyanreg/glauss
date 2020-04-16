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
#include <signal.h>
#include <stdlib.h>
#include "graph_input.h"
#include "physics/physics.h"
#include "sighandle.h"
#include "graph/graph.h"
#include "graph/graph_sdl.h"
#include "shared/options.h"
#include "shared/msg_phys.h"

/* Time constant scaling */
#define DT_SCALE 0.5

/* MWheel scroll scale factor */
#define ZOOM_SENS 1.11

/* Maximum zoom out */
#define MIN_SCALE 0.0005

static void graph_sdl_scan_selection(graph_window *win)
{
    if (win->event->key.keysym.sym!=SDLK_RETURN && win->numbers.final_digit < DIGITS_MAX) {
        if (win->event->key.keysym.sym==SDLK_BACKSPACE && win->numbers.final_digit > 0) {
            getnumber(&win->numbers, 0, NUM_REMOVE);
            win->currentsel[strlen(win->currentsel)-1] = '\0';
            return;
        }
        /*	sscanf will return 0 if nothing was done	*/
        if (!sscanf(SDL_GetKeyName(win->event->key.keysym.sym), "%u", &win->currentnum)) {
            return;
        } else {
            strcat(win->currentsel, SDL_GetKeyName(win->event->key.keysym.sym));
            getnumber(&win->numbers, win->currentnum, NUM_ANOTHER);
        }
        return;
    } else {
        strcpy(win->currentsel, "Select object:");
        win->start_selection = 0;
        win->chosen = getnumber(&win->numbers, 0, NUM_GIVEME);
        if (win->chosen >= option->obj) {
            win->chosen = 0;
        } else {
            pprintf(PRI_HIGH, "Object %u selected.\n", win->chosen);
            return;
        }
    }
}

void graph_press_mouse(graph_window *win)
{
    if (win->event->button.button == SDL_BUTTON_LEFT)
        win->flicked = 1;
    if (win->event->button.button == SDL_BUTTON_MIDDLE) {
        win->chosen = 0;
        win->translate = 1;
    }
    SDL_ShowCursor(0);
    SDL_GetMouseState(&win->initmousex, &win->initmousey);
    SDL_SetRelativeMouseMode(1);
    SDL_GetRelativeMouseState(NULL, NULL);
}


void graph_release_mouse(graph_window *win)
{
    if (win->event->button.button == SDL_BUTTON_LEFT)
        win->flicked = 0;
    if (win->event->button.button == SDL_BUTTON_MIDDLE)
        win->translate = 0;
    SDL_SetRelativeMouseMode(0);
    SDL_WarpMouseInWindow(win->window, win->initmousex, win->initmousey);
    SDL_ShowCursor(1);
}

void graph_adj_zoom_mwheel(graph_window *win)
{
    if (win->event->wheel.y > 0)
        win->camera.scalefactor *= ZOOM_SENS;
    if (win->event->wheel.y < 0)
        win->camera.scalefactor /= ZOOM_SENS;
    /* Cap to specified max zoom */
    if (win->camera.scalefactor < MIN_SCALE)
        win->camera.scalefactor = MIN_SCALE;
}

int graph_scan_keypress(graph_window *win)
{
    /* Check if we need to get numbers for object selecting */
    if (win->start_selection) {
        graph_sdl_scan_selection(win);
    }
    /* Scan hotkeys */
    switch (win->event->key.keysym.sym) {
        case SDLK_RETURN:
            win->start_selection = 1;
            break;
        case SDLK_RIGHTBRACKET:
            option->dt /= DT_SCALE;
            break;
        case SDLK_LEFTBRACKET:
            option->dt *= DT_SCALE;
            break;
        case SDLK_SPACE:
            phys_ctrl(PHYS_PAUSESTART, NULL);
            break;
        case SDLK_r:
            win->camera = def_cam;
            win->camera.aspect_ratio = graph_sdl_resize_wind(win);
            win->chosen = 0;
            break;
        case SDLK_MINUS:
            phys_shuffle_algorithms();
            break;
        case SDLK_BACKSPACE:
            if(phys_ctrl(PHYS_STATUS, NULL) == PHYS_STATUS_STOPPED)
                phys_ctrl(PHYS_START, &win->object);
            else
                phys_ctrl(PHYS_SHUTDOWN, NULL);
            break;
        case SDLK_PERIOD:
            if (win->chosen < option->obj)
                win->chosen++;
            break;
        case SDLK_c:
            phys_buffer_revert_single_step();
            break;
        case SDLK_v:
            phys_buffer_forward_single_step();
            break;
        case SDLK_d:
            phys_ctrl(PHYS_STEP_FWD, NULL);
            break;
        case SDLK_COMMA:
            if(win->chosen > 0)
                win->chosen--;
            break;
        case SDLK_f:
            graph_sdl_toggle_fullscreen(win);
            break;
        case SDLK_s:
            graph_sshot(phys_stats->total_steps);
            break;
        case SDLK_ESCAPE:
            return 1;
            break;
        case SDLK_q:
            return 1;
            break;
        default:
            break;
    }
    return 0;
}
