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

#include "config.h"

/* I'm surprised this works */
#include LUA_MAINHEAD
#include LUA_AUXLIB
#include LUA_LIB

#include "physics/physics.h"
#include "physics/physics_aux.h"
#include "in_file.h"

enum parser_var_type {
    /* Variables */
    VAR_FLOAT,
    VAR_DOUBLE,
    VAR_LONG_DOUBLE,
    VAR_INT,
    VAR_UINT,
    VAR_BOOL,
    VAR_USHORT,
    VAR_SHORT,
    VAR_STRING,
    VAR_LONGINT,
    VAR_LONGUINT,
    VAR_LONGLONGUINT,
    VAR_COLOR,
    VAR_NO_IDEA,
};

#define P_TYPE(x) &(x), _Generic((x),                                          \
    float: VAR_FLOAT,                 double: VAR_DOUBLE,                      \
    long double: VAR_LONG_DOUBLE,     int: VAR_INT,                            \
    unsigned int: VAR_UINT,           unsigned short int: VAR_USHORT,          \
    bool: VAR_BOOL,                   short int: VAR_SHORT,                    \
    char *: VAR_STRING,               long int: VAR_LONGINT,                   \
    unsigned long int: VAR_LONGUINT,  unsigned long long int: VAR_LONGLONGUINT,\
    float*: VAR_COLOR, default: VAR_NO_IDEA                                    \
), _Generic((x), bool: LUA_TBOOLEAN, char *: LUA_TSTRING,                      \
    float*: LUA_TTABLE, default: LUA_TNUMBER)

struct parser_map {
    const char *name;
    volatile void *val;
    int type;
    int cmd_or_lua_type;
};

extern struct parser_map *total_opt_map;

struct lua_parser_state {
    int i;
    bool nullswitch;
    bool fileset;
    bool read_id;
    bool preserve_id;
    in_file file;
    phys_obj buffer, *object;
    struct atomic_cont buffer_ele;
    struct parser_map *opt_map;
};

/* General */
int parse_lua_open_file(const char *filename);
int parse_lua_open_string(const char *script);
void parse_lua_close(void);
void free_input_parse_opts();

/* Maps */
void print_parser_map(struct parser_map *map);
struct parser_map *allocate_parser_map(struct parser_map *map);
unsigned int update_parser_map(struct parser_map *map, struct parser_map **dest);
int register_parser_map(struct parser_map *map, struct parser_map **dest);
int unregister_parser_map(struct parser_map *map, struct parser_map **dest);

/* Maps I/O */
int parser_get_value_str(struct parser_map var, char *str, size_t len);
void parser_map_free_strings(struct parser_map *map);
void parser_print_generic(struct parser_map *var);
void parser_set_generic(struct parser_map *var, const char *val);
void parser_push_generic(lua_State *L, struct parser_map *var);
void parser_read_generic(lua_State *L, struct parser_map *var);

/* Elements */
int parse_lua_simconf_elements(const char *filepath);

/* Misc lua */
unsigned long int conf_lua_getlen(lua_State *L, int pos);
size_t parser_lua_current_gc_mem(lua_State *L);
size_t parser_lua_gc_sweep(lua_State *L);

/* When we parse a Lua table in the exec function we need to add one last
* dummy object at the end so that the final last actual object doesn't get
* cut off. Why: Lua lua_next returns there's nothing next when there actually
* IS SOMETHING NEXT. So we don't get to update the object in our array.
* The dummy object doesn't get updated into the array because we offset
* Lua's retarded behaviour by one. Deja vu? Yes indeed, the very first object
* we get from Lua when parsing is always some gibberish, hence we have a bool
* check to skip inputting that object into our array(see nullswitch). Fuck. */
int parser_lua_fill_table_hack(lua_State *L, unsigned int arr_size);

/* Parsing f-ns */
int conf_lua_parse_objs(lua_State *L, struct lua_parser_state *parser_state);
int conf_lua_parse_opts(lua_State *L, struct lua_parser_state *parser_state);
int conf_lua_parse_files(lua_State *L, struct lua_parser_state *parser_state);
int conf_lua_parse_elements(lua_State *L, struct lua_parser_state *parser_state);
/* Insert any of the functions above to parse it in */
void conf_traverse_table(lua_State *L, int (rec_fn(lua_State *, struct lua_parser_state *)),
                        struct lua_parser_state *parser_state);

/* Parser abstractions */
int parse_lua_simconf_options(struct parser_map *map);
int parse_lua_simconf_objects(phys_obj **object, const char* sent_to_lua);
void parser_push_stat_array(lua_State *L, struct global_statistics *stats);
void parser_push_object_array(lua_State *L, phys_obj *obj,
                            struct parser_map *range_ind);

/* Returns number of changed objects(returned from Lua) */
unsigned int lua_exec_funct(const char *funct, phys_obj *object,
                            struct global_statistics *stats);

/* Misc parsing */
const char *parse_file_to_str(const char *filename);
