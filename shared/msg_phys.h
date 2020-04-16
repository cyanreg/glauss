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

enum MSG_PRIORITIY {
    PRI_ESSENTIAL = 1,
    PRI_VERYHIGH = 2,
    PRI_HIGH = 3,
    PRI_HIGHMED = 4,
    PRI_MEDIUM = 5,
    PRI_LOWMED = 6,
    PRI_LOW = 7,
    PRI_VERYLOW = 8,
    PRI_SPAM = 9,
    PRI_OK = 10,
    PRI_WARN = 11,
    PRI_ERR = 12,
    PRI_INPUT = 13,
};

void pprint_log_open(const char *filename);
void pprint_log_close(void);
void pprint_enable(void);
void pprint_disable(void);
void pprintf(enum MSG_PRIORITIY priority, const char *format, ...);

#define pprint_ok(...)     pprintf(PRI_OK, __VA_ARGS__)
#define pprint_warn(...)   pprintf(PRI_WARN, __VA_ARGS__)
#define pprint_err(...)    pprintf(PRI_ERR, __VA_ARGS__)
#define pprint(...)        pprintf(PRI_ESSENTIAL, __VA_ARGS__)
#define pprint_verb(...)   pprintf(PRI_LOWMED, __VA_ARGS__)
#define pprint_deb(...)    pprintf(PRI_VERYLOW, __VA_ARGS__)
#define pprint_input(...)  pprintf(PRI_ESSENTIAL, __VA_ARGS__)
