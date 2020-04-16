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
#include <readline/readline.h>
#include <readline/history.h>
#include "input/parser.h"
#include "sighandle.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "shared/output.h"
#include "physics/physics.h"
#include "graph/graph.h"
#include "graph/graph_thread.h"
#include "input_thread.h"
#include "sighandle.h"

static const char pointgreen[] = "\033[032m"CMD_PROMPT_DOT"\033[0m";
static const char pointyellow[] = "\033[033m"CMD_PROMPT_DOT"\033[0m";
static const char pointred[] = "\033[031m"CMD_PROMPT_DOT"\033[0m";

/* Needed for the generator and completions */
static int list_index_cmd = 0, len_cmd = 0;
static int list_index_opt = 0, len_opt = 0;
static struct input_cfg *global_cfg = NULL;
static bool cleanup = false;

int input_thread_init(graph_window **win, phys_obj **object)
{
    if (option->debug) {
        pprint_warn("Debugging mode active, disabling CMD interface!\n");
        return 1;
    }

    struct input_cfg *cfg = calloc(1, sizeof(struct input_cfg));

    cfg->obj = object;
    cfg->win = win;
    cfg->status = true;
    cfg->selfquit = false;
    cfg->line = NULL;
    cfg->cmd_map = allocate_parser_map((struct parser_map[]){
        {"phys_check_collisions", NULL, VAR_CHECK_COLLISIONS, VAR_CMD},
        {"save", NULL, VAR_SAVE, VAR_CMD},
        {"load", NULL, VAR_LOAD, VAR_CMD},
        {"list", NULL, VAR_LIST, VAR_CMD},
        {"quit", NULL, VAR_QUIT, VAR_CMD},
        {"exit", NULL, VAR_QUIT, VAR_CMD},
        {"start", NULL, VAR_START, VAR_CMD},
        {"stop", NULL, VAR_STOP, VAR_CMD},
        {"restart", NULL, VAR_RESTART, VAR_CMD},
        {"help", NULL, VAR_HELP, VAR_CMD},
        {"status", NULL, VAR_STATUS, VAR_CMD},
        {"stats", NULL, VAR_STATS, VAR_CMD},
        {"clear", NULL, VAR_CLEAR, VAR_CMD},
        {"pause", NULL, VAR_PAUSE, VAR_CMD},
        {"unpause", NULL, VAR_PAUSE, VAR_CMD},
        {"step_fwd", NULL, VAR_STEP_FWD},
        {"step_bwd", NULL, VAR_STEP_BWD},
        {"element", NULL, VAR_ELE_COLOR, VAR_CMD},
        {"win_create", NULL, VAR_ENABLE_WINDOW, VAR_CMD},
        {"win_destroy", NULL, VAR_DISABLE_WINDOW, VAR_CMD},
        {"win_draw_mode", NULL, VAR_WIN_DRAW_MODE, VAR_CMD},
        {"lua_readopts", NULL, VAR_LUA_READOPTS, VAR_CMD},
        {"lua_run_gc", NULL, VAR_LUA_RUN_GC, VAR_CMD},
        {"set_view", NULL, VAR_SET_VIEW, VAR_CMD},
        {"list_cmd", NULL, VAR_LIST_CMD, VAR_CMD},
        {"#command (runs a system command)", NULL, VAR_CMD_SYS, VAR_CMD},
        {0}
    });

    global_cfg = cfg;

    cleanup = false;

    pthread_create(&cfg->input, NULL, input_thread, cfg);

    sig_load_destr_fn(input_thread_quit, "Input");
    sig_load_destr_fn(input_thread_reset_term, "Terminal");

    return 0;
}

void input_thread_quit(void)
{
    if (!global_cfg)
        return;

    global_cfg->status = false;

    void *res = NULL;
    void *val = global_cfg->selfquit ? NULL : PTHREAD_CANCELED;

    while (res != val) {
        if (!global_cfg->selfquit)
            pthread_cancel(global_cfg->input);
        pthread_join(global_cfg->input, &res);
    }

    /* Reset terminal */
    rl_deprep_terminal();
    rl_free_line_state();
    rl_cleanup_after_signal();
    cleanup = true;

    /* Free resources */
    free(global_cfg->line);               /* Should be NULL, but just in case */
    free(global_cfg->cmd_map);
    free(global_cfg);

    global_cfg = NULL;

    sig_unload_destr_fn(input_thread_quit);
    sig_unload_destr_fn(input_thread_reset_term);

    return;
}

void input_thread_reset_term(void)
{
    if (cleanup == true)
        return;
    rl_free_line_state();
    rl_cleanup_after_signal();
    cleanup = true;
}

void input_repos_prompt(void)
{
    if (!global_cfg)
        return;
    rl_on_new_line();
    rl_redisplay();
}

static void input_cmd_printall(struct parser_map *cmd_map, struct parser_map *opt_map)
{
    pprint_input("Implemented variables:\n");
    for (struct parser_map *i = opt_map; i->name; i++) {
        parser_print_generic(i);
    }

    pprint_input("Implemented commands:\n");
    for (struct parser_map *i = cmd_map; i->name; i++) {
        if (i->cmd_or_lua_type == VAR_CMD) {
            pprint_input("%s\n", i->name);
        } else {
            parser_print_generic(i);
        }
    }
}

int input_call_system(const char *cmd)
{
    pprint_disable();
    int ret = system(cmd);
    pprint_enable();
    return ret;
}

int input_change_cam_angle(graph_window *win, char mat, float val)
{
    const char *usage = "Syntax: set_view <{X,Y,Z}> <value>\n";

    if (!win)
        return 1;

    switch(mat) {
        case 'X': case 'x':
            win->camera.view_rotx = val;
            break;
        case 'Y': case 'y':
            win->camera.view_roty = val;
            break;
        case 'Z': case 'z':
            win->camera.view_rotz = val;
            break;
        default:
            pprint_input("%s", usage);
            return 1;
            break;
    }

    return 0;
}

int input_change_element_col(const char *name, const char *col, const char *value)
{
    const char *usage = "Syntax: element <Number/Name> <{R,G,B,A}> <value>\n";
    if (!name) {
        pprint_input("%s", usage);
        return 1;
    }

    int i = -1;
    int res = sscanf(name, "%i", &i);
    if (!res) {
        for (i = 1; i < 121; i++) {
            if (!strncasecmp(name, atom_prop[i].name, strlen(name))) {
                break;
            }
        }
    }
    if (i < 0 || i > 119) {
        pprint_input("%s", usage);
        return 1;
    }

    const char code_col = col ? col[0] : '\0';
    unsigned int val = value ? strtol(value, NULL, 10) : 256;

    if (code_col != '\0' && val < 256) {
        switch(code_col) {
            case 'R': case 'r':
                atom_prop[i].color[0] = val/255.0;
                break;
            case 'G': case 'g':
                atom_prop[i].color[1] = val/255.0;
                break;
            case 'B': case 'b':
                atom_prop[i].color[2] = val/255.0;
                break;
            case 'A': case 'a':
                atom_prop[i].color[3] = val/255.0;
                break;
            default:
                return 1;
                break;
        }
    }

    pprint_input("Element %s = {%i, %i, %i, %i}\n", atom_prop[i].name,
        (int)(atom_prop[i].color[0]*255.0),
        (int)(atom_prop[i].color[1]*255.0),
        (int)(atom_prop[i].color[2]*255.0),
        (int)(atom_prop[i].color[3]*255.0));

    return 0;
}

enum setall_ret input_token_setall(char *line, struct input_cfg *t)
{
    enum setall_ret retval = CMD_ALL_FINE;
    int num_tok = 0;
    bool match = 0;
    char *tokstr = strdup(line);
    char *recstr = NULL, *token[CMD_MAX_TOKENS] = {NULL}, *freestr = tokstr;

    tokstr = strtok_r(tokstr, " ", &recstr);
    while (tokstr) {
        if (num_tok > CMD_MAX_TOKENS) {
            retval = CMD_TOO_MANY_TOKENS;
            goto cleanall;
        }
        if (*tokstr == '=') {
            retval = CMD_INVALID_ASSIGN;
            goto cleanall;
        }
        token[num_tok++] = strdup(tokstr);
        tokstr = strtok_r(NULL, " ", &recstr);
    }

    if (token[0][0] == '#') {
        size_t len = strlen(line)-1;
        char *command = calloc(1, len);
        strncpy(command, line+1, len);
        int cmd_ret = input_call_system(command);
        free(command);
        retval = cmd_ret ? CMD_SYS_RET_ERR : CMD_ALL_FINE;
        goto cleanall;
    }

    for (struct parser_map *i = total_opt_map; i->name; i++) {
        if (!strcmp(i->name, token[0])) {
            match = 1;
            if (num_tok < 2)
                parser_print_generic(i);
            else
                parser_set_generic(i, token[1]);
        }
    }

    for (struct parser_map *i = t->cmd_map; i->name; i++) {
        if (!strcmp(i->name, token[0])) {
            match = 1;
            if (i->cmd_or_lua_type == VAR_CMD) {
                switch(i->type) {
                    case VAR_CHECK_COLLISIONS:
                        pprint_warn("Checking for collisions...\n");
                        unsigned int coll = phys_check_collisions(*t->obj, 0, option->obj);
                        if (coll) {
                            pprint_err("%i objects share coordinates,\
                                    reconsider starting anything at all and\
                                    check your input script\n");
                        } else {
                            pprint_ok("No object share coordinates\n");
                        }
                        break;
                    case VAR_HELP:
                        input_cmd_printall(t->cmd_map, total_opt_map);
                        break;
                    case VAR_STOP:
                        phys_ctrl(PHYS_SHUTDOWN, NULL);
                        break;
                    case VAR_RESTART:
                        phys_ctrl(PHYS_SHUTDOWN, NULL);
                        phys_ctrl(PHYS_START, t->obj);
                        break;
                    case VAR_LIST:
                        phys_list_algo();
                        break;
                    case VAR_START:
                        phys_ctrl(PHYS_START, t->obj);
                        break;
                    case VAR_SAVE:
                        out_write_array(*t->obj, "array_%0.2Lf.bin", halt_objects);
                        break;
                    case VAR_LOAD:
                        if (num_tok < 2) {
                            printf("Usage: \"load <filename>\"\n");
                        } else {
                            in_write_array(t->obj, token[1], halt_objects);
                        }
                        break;
                    case VAR_PAUSE:
                        phys_ctrl(PHYS_PAUSESTART, NULL);
                        break;
                    case VAR_STEP_FWD:
                        if (num_tok < 2) {
                            printf("Usage: \"step_fwd <number>\"\n");
                        } else {
                            unsigned int steps = strtoul(token[1], NULL, 10);
                            phys_fwd_steps(steps);
                        }
                        break;
                    case VAR_STEP_BWD:
                        if (num_tok < 2) {
                            printf("Usage: \"step_bwd <number>\"\n");
                        } else {
                            unsigned int steps = strtoul(token[1], NULL, 10);
                            phys_bwd_steps(steps);
                        }
                        break;
                    case VAR_QUIT:
                        retval = CMD_EXIT;
                        break;
                    case VAR_STATS:
                        raise(SIGUSR1);
                        break;
                    case VAR_STATUS:
                        raise(SIGUSR1);
                        break;
                    case VAR_CLEAR:
                        for (int c = 0; c < t->lines; c++)
                            printf("\n");
                        break;
                    case VAR_LUA_READOPTS:
                        if (num_tok < 2) {
                            parse_lua_simconf_options(total_opt_map);
                        } else {
                            if (strcmp(option->filename, token[1])) {
                                if (parse_lua_open_file(token[1])) {
                                    break;
                                }
                            }
                            parse_lua_simconf_options(total_opt_map);
                        }
                        break;
                    case VAR_LUA_RUN_GC:
                        parser_lua_gc_sweep(NULL);
                        break;
                    case VAR_LIST_CMD:
                        for (struct parser_map *i = t->cmd_map; i->name; i++) {
                            printf("%s\n", i->name);
                        }
                        break;
                    case VAR_WIN_DRAW_MODE:
                        if (!t->win || !*t->win)
                            break;
                        graph_set_draw_mode(*t->win, token[1]);
                        break;
                    case VAR_SET_VIEW:
                        if (!t->win || !*t->win || num_tok < 2)
                            break;
                        float val = 0;
                        if (token[2])
                            val = strtof(token[2], NULL);
                        else
                            break;
                        input_change_cam_angle(*(t->win), token[1][0], val);
                        break;
                    case VAR_ELE_COLOR:
                        input_change_element_col(token[1], token[2], token[3]);
                        break;
                    case VAR_ENABLE_WINDOW:
                        if (*t->win)
                            break;
                        option->novid = false;
                        t->win = graph_thread_init(*t->obj);
                        break;
                    case VAR_DISABLE_WINDOW:
                        if (!*t->win)
                            break;
                        graph_thread_quit();
                        *t->win = NULL;
                        option->novid = true;
                        break;
                    default:
                        break;
                }
            }
            break;
        }
    }
    if(!match) retval = CMD_NOT_FOUND;

    cleanall:
        for(int i = 0; i < num_tok; i++) {
            free(token[i]);
        }
        free(freestr);

    return retval;
}

char *input_cmd_generator(const char *line, int state)
{
    /* Readline examples were BAD as fuck. They didn't say if variables
    * were supposed to be global. FUCK YOU. I seriously thought that I was haunted */
    char *name = NULL;
    if (!state) {
        list_index_cmd = 0;
        len_cmd = strlen(line);
    }
    while ((name = (char *)global_cfg->cmd_map[list_index_cmd++].name)) {
        if (!strncmp(name, line, len_cmd)) {
            return strdup(name);
        }
    }
    return NULL;
}

char *input_opt_generator(const char *line, int state)
{
    char *name = NULL;
    if (!state) {
        list_index_opt = 0;
        len_opt = strlen(line);
    }
    while ((name = (char *)total_opt_map[list_index_opt++].name)) {
        if (!strncmp(name, line, len_opt)) {
            return strdup(name);
        }
    }
    return NULL;
}

char **input_completion(const char *line, int start, int end)
{
    char **matches = NULL;

    if (start == 0) {
        matches = rl_completion_matches(line, input_cmd_generator);
        if (!matches) {
            matches = rl_completion_matches(line, input_opt_generator);
        }
    }

    return matches;
}

void *input_thread(void *thread_setts)
{
    struct input_cfg *t = thread_setts;
    char prompt[sizeof(CMD_PROMPT_BASE)+sizeof(pointyellow)+sizeof(CMD_PROMPT_SPACE)+1];

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* Rant: why the shit does every library in existance want to install its
    * own signal handler which THEN passes the signal to us? Even damn SDL
    * tries to do that, DESPITE having to make us call SDL_Quit(). It's retarded */
    rl_catch_signals = false;
    rl_attempted_completion_function = input_completion;

    unsigned int lines = 0; /* TODO: find terminal size WITHOUT an ioctl call */
    t->lines = lines ? lines : CMD_CLEAR_REPS;

    rl_prep_terminal(0);

    while (t->status) {
        /* Refresh prompt */
        if (phys_ctrl(PHYS_STATUS, NULL) == PHYS_STATUS_PAUSED) {
            sprintf(prompt, "%s %s ", CMD_PROMPT_BASE, pointyellow);
        } else if (phys_ctrl(PHYS_STATUS, NULL) == PHYS_STATUS_RUNNING) {
            sprintf(prompt, "%s %s ", CMD_PROMPT_BASE, pointgreen);
        } else {
            sprintf(prompt, "%s %s ", CMD_PROMPT_BASE, pointred);
        }

        t->line = readline(prompt);

        if (!t->line) {
            t->selfquit = 1;
            t->status = 0;
        } else if (*t->line) {
            add_history(t->line);
            switch(input_token_setall(t->line, t)) {
                case CMD_SYS_RET_ERR:
                    pprint_input("System command error\n");
                    break;
                case CMD_ALL_FINE:
                    break;
                case CMD_EXIT:
                    t->selfquit = 1;
                    t->status = 0;
                    break;
                case CMD_NOT_FOUND:
                    pprint_input("Command not found\n");
                    break;
                case CMD_INVALID_ASSIGN:
                    pprint_input("Correct assignment syntax is \"variable value\"\n");
                    break;
                default:
                    break;
            }
        }

        free(t->line);
        t->line = NULL;
    }

    if (t->selfquit) {
        raise(SIGINT);
        return 0;
    }

    return 0;
}
