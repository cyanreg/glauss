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
#include <stdarg.h>
#include "msg_phys.h"
#include "options.h"
#include "input/input_thread.h"

static bool disable_print = 0;

void pprint_enable(void)
{
    fflush(stdout);
    disable_print = false;
}

void pprint_disable(void)
{
    fflush(stdout);
    disable_print = true;
}

void pprint_log_open(const char *filename)
{
    option->logfile = fopen(filename, "w");
    if (!option->logfile) {
        pprint_err("Error opening log file to write to\n");
        return;
    }
    pprintf(PRI_ESSENTIAL, "Writing log to file %s.\n", filename);
    option->logenable = true;
}

void pprint_log_close(void)
{
    if (!option->logenable)
        return;
    fclose(option->logfile);
    option->logenable = false;
}

/* pprintf - a priority printing function. */
void pprintf(enum MSG_PRIORITIY priority, const char *format, ...)
{
    va_list args;
    const char okmsg[]   = "[\033[032m Ok! \033[0m] ";
    const char warnmsg[] = "[\033[033m Warning! \033[0m] ";
    const char errmsg[]  = "[\033[031m Error! \033[0m] ";
    if (option->logenable) {
        va_start(args, format);
        vfprintf(option->logfile, format, args);
        va_end(args);
    }
    if (priority == PRI_ERR) {
        fprintf(stderr, "%s", errmsg);
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    } else if (priority <= option->verbosity || priority == PRI_OK || priority == PRI_WARN) {
        /* Ugly but works */
        if (disable_print)
            return;
        else if(priority == PRI_OK)
            printf("%s", okmsg);
        else if(priority == PRI_WARN)
            printf("%s", warnmsg);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}
