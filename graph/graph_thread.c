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
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "graph.h"
#include "graph_sdl.h"
#include "graph_thread.h"
#include "input/parser.h"
#include "input/sighandle.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "physics/physics.h"
#include "physics/physics_aux.h"
#include "input/graph_input.h"

struct graph_cfg *global_cfg = NULL;

graph_window **graph_thread_init(phys_obj *object)
{
    if (global_cfg) {
        return NULL;
    }

    struct graph_cfg *cfg = calloc(1, sizeof(struct graph_cfg));

    cfg->obj = object;
    cfg->status = true;
    cfg->selfquit = false;

    graph_sdl_init();

    pthread_create(&cfg->graph, NULL, graph_thread, cfg);

    global_cfg = cfg;

    sig_load_destr_fn(graph_thread_quit, "Graphics");

    return &cfg->win;
}

void graph_thread_quit(void)
{
    if (!global_cfg) {
        return;
    }

    global_cfg->status = false;

    pthread_join(global_cfg->graph, NULL);

    graph_sdl_deinit();

    /* Free resources */
    free(global_cfg);

    global_cfg = NULL;

    option->novid = true;

    sig_unload_destr_fn(graph_thread_quit);

    return;
}

void *graph_thread(void *thread_setts)
{
    struct graph_cfg *t = thread_setts;

    /* SDL */
    t->win = graph_win_create(t->obj);

    graph_sdl_resize_wind(t->win);

    /* Cancel policy */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* FPS */
    unsigned int frames = 0;
    unsigned long long int time = 0, t1 = 0, t2 = 0;

    t1 = phys_gettime_us();

    while (t->status) {
        /* Get input from SDL */
        while (SDL_PollEvent(t->win->event)) {
            switch (t->win->event->type) {
                case SDL_WINDOWEVENT:
                    if (t->win->event->window.event == SDL_WINDOWEVENT_RESIZED)
                        graph_sdl_resize_wind(t->win);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    graph_press_mouse(t->win);
                    break;
                case SDL_MOUSEBUTTONUP:
                    graph_release_mouse(t->win);
                    break;
                case SDL_MOUSEWHEEL:
                    graph_adj_zoom_mwheel(t->win);
                    break;
                case SDL_KEYDOWN:
                    if (graph_scan_keypress(t->win)) {
                        t->selfquit = 1;
                        t->status = 0;
                    }
                    break;
                case SDL_QUIT:
                    t->selfquit = 1;
                    t->status = 0;
                    break;
            }
        }

        /* FPS */
        /* Update timer */
        t2 = phys_gettime_us();
        time += t2 - t1;
        if (time > 1.0/1.0e-6) {
            t->win->fps = frames/(time*1.0e-6);
            time = frames = 0;
        }
        t1 = t2;
        frames++;

        /* Move camera */
        graph_sdl_move_cam(t->win);

        /* Draw scene */
        graph_draw_scene(t->win);

        /* Swap Front and Back buffers */
        graph_sdl_swapwin(t->win);
    }

    graph_win_destroy(t->win);

    if (t->selfquit)
        raise(SIGINT);

    return 0;
}
