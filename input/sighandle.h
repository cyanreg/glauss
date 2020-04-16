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

int sig_load_destr_fn(void (*destr_fn)(void), const char description[]);
int sig_unload_destr_fn(void (*destr_fn)(void));

void on_usr1_signal(int signo);
void on_quit_signal(int signo);
void on_alrm_signal(int signo);
