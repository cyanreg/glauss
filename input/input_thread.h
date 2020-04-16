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

#include <pthread.h>
#include "graph/graph_sdl.h"
#include "input/parser.h"

#define CMD_CLEAR_REPS 20
#define CMD_MAX_TOKENS 20
#define CMD_PROMPT_BASE "phys"
#define CMD_PROMPT_DOT "•"
#define CMD_PROMPT_SPACE " "

enum setall_ret {
    CMD_SYS_RET_ERR,
    CMD_ALL_FINE,
    CMD_EXIT,
    CMD_NOT_FOUND,
    CMD_INVALID_ASSIGN,
    CMD_TOO_MANY_TOKENS,
};

enum commands {
    /* VAR prefix because historic reasons... should just run sed */
    VAR_CMD,
    VAR_CMD_SYS,
    VAR_QUIT,
    VAR_LIST,
    VAR_START,
    VAR_STOP,
    VAR_RESTART,
    VAR_HELP,
    VAR_STATUS,
    VAR_STATS,
    VAR_CLEAR,
    VAR_PAUSE,
    VAR_SAVE,
    VAR_LOAD,
    VAR_ELE_COLOR,
    VAR_ENABLE_WINDOW,
    VAR_DISABLE_WINDOW,
    VAR_WIN_DRAW_MODE,
    VAR_LUA_READOPTS,
    VAR_LUA_RUN_GC,
    VAR_CHECK_COLLISIONS,
    VAR_STEP_FWD,
    VAR_STEP_BWD,
    VAR_LIST_CMD,
    VAR_SET_VIEW,
};

/* Sent to input thread */
struct input_cfg {
    pthread_t input;
    phys_obj **obj;
    graph_window **win;
    char *line;
    unsigned int lines;
    struct parser_map *cmd_map;
    volatile bool status, selfquit;
};

int input_thread_init(graph_window **win, phys_obj **object);
void input_thread_quit(void);
void input_repos_prompt(void);
void input_thread_reset_term(void);
//int input_change_cam_angle(graph_window *win, char *mat, float val);
int input_change_element_col(const char *name, const char *col, const char *value);
int input_call_system(const char *cmd);
enum setall_ret input_token_setall(char *line, struct input_cfg *t);
void *input_thread(void *thread_setts);
