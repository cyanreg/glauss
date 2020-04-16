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

#include "graph_sdl.h"

/* Sent to graph thread */
struct graph_cfg {
    pthread_t graph;
    phys_obj *obj;
    graph_window *win;
    unsigned int *frames;
    float *fps;
    volatile bool status, selfquit;
};

graph_window **graph_thread_init(phys_obj *object);
void graph_thread_quit(void);
void *graph_thread(void *thread_setts);

