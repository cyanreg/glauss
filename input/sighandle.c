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
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "physics/physics.h"
#include "parser.h"
#include "sighandle.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "graph/graph_thread.h"
#include "input_thread.h"

#define MAX_DESTRUCTORS 40

static int tot_destructors = 0;
static int ign_destructors = 0;

struct signal_destructors {
    void (*destr_fn)(void);
    char description[55];
    bool ignore;
};

static struct signal_destructors destructors[MAX_DESTRUCTORS];

static void sig_clear_ignored_destructors()
{
    if (ign_destructors > (float)MAX_DESTRUCTORS*(3.0/4.0)) {
        pprint_warn("Cleaning out unused destructors\n");
        /* Will fix IF needed. This should never happen. */
    }
}

int sig_unload_destr_fn(void (*destr_fn)(void))
{
    for (int i = 0; i < tot_destructors; i++) {
        if (destructors[i].destr_fn == destr_fn) {
            destructors[i].ignore = true;
            ign_destructors++;
            return 0;
        }
    }
    return 1;
}

int sig_load_destr_fn(void (*destr_fn)(void), const char description[])
{
    if (tot_destructors > MAX_DESTRUCTORS) {
        pprint_err("Max destructors reached! What are you doing?!?\n");
        return 1;
    }

    destructors[tot_destructors].destr_fn = destr_fn;

    if (tot_destructors > 0)
        strcpy(destructors[tot_destructors].description, "&& ");

    strncat(destructors[tot_destructors].description, description, 51);
    strcat(destructors[tot_destructors].description, " ");

    tot_destructors++;

    sig_clear_ignored_destructors();

    return 0;
}

/* Report stats on command line */
void on_usr1_signal(int signo)
{
    pprintf(PRI_ESSENTIAL, "\n");
    if(!signo)
        pprintf(PRI_ESSENTIAL, "USR1 signal received, current stats:\n");

    if (!phys_stats) {
        pprint_err("Not running!\n");
        return;
    }

    for (struct parser_map *i = phys_stats->global_stats_map; i->name; i++) {
        char res[50];
        parser_get_value_str(*i, res, 50);
        printf("%s = %s\n", i->name, res);
    }

    if (phys_stats->t_stats) {
        for (unsigned int t = 0; t < phys_stats->threads; t++) {
            for (struct parser_map *i = phys_stats->t_stats[t].thread_stats_map; i->name; i++) {
                char res[50];
                parser_get_value_str(*i, res, 50);
                printf("%u.	%s = %s\n", t, i->name, res);
            }
        }
    }
}

/* Function given to signal handler */
void on_quit_signal(int signo)
{
    pprint_enable();
    pprintf(PRI_ESSENTIAL, "\nSignal to quit %i received!\n", signo);

    /* Physics */
    phys_ctrl(PHYS_SHUTDOWN, NULL);

    pprintf(PRI_ESSENTIAL, "Closing: ");

    for (int i = 0; i < tot_destructors; i++) {
        if (destructors[i].ignore)
            continue;
        pprintf(PRI_ESSENTIAL, "%s", destructors[i].description);
        destructors[i].destr_fn();
        pprintf(PRI_OK, "");
    }

    pprintf(PRI_ESSENTIAL, "\n");

    /* Main */
    option->quit_main_now = true;
}

void on_alrm_signal(int signo)
{
    pprint_warn("Watchdog timer kicked in! Program probably hanged up.\n");
}
