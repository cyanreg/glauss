glauss
######

############################
a physics simulations engine
############################

:Copyright: GPLv3
:Manual section: 1
:Manual group: science

SYNOPSIS
========

| **glauss_client** -f [file] [flags]

DESCRIPTION
===========

`glauss`, or The `G`\ eneralized `L`\ ua-`AU`\ gmented `S`\ imulation `S`\ ystem,
is a physics simulations engine written in C, capable of being
used in a variety of situations, from n-body simulations on planetary orbits
to barnes-hut galactical simulations. Configured and controlled using a Lua
script. An API to create algorithms entirely in Lua is available.

CONTROLS
========

The following list of controls has power when the program is run in graphical mode.

*Keyboard Control*
------------------

[ and ]
    Halve/double time constant.

, and .
    Select next/previous object.

ENTER
    Press and type a number to jump to a specific object.

R
    Reset view.

Z
    Save current system to an *XYZ* file.

S
    Create a *PNG* screenshot.

F
    Switch between fullscreen and windowed mode.

D
    Advance one step forward.

C
    Reverse back one step(buffered).

V
    Forward one step(buffered).

SPACEBAR
    Pause/unpause.

ESC or Q
    Quit program.

BACKSPACE
    Shut the physics threads completely.

MINUS
    Change the physics algorithm(threads need to be completely off).

*Mouse controls*
----------------

LMB
    Hold and drag to rotate camera.

SCROLL
    Zoom in and out.

MMB
    Hold and drag to move camera.

FLAGS
=====
The following flags can be used:

``--novid`` - No argument
    Will run the program without any video outputting enabled.

``--bench`` - No argument
    Will run the program in benchmark mode. Quits and reports stats after ``timer`` runs once.

``--version``, ``-V`` - No argument
    Will report version as well as compile time and date.

``--help``, ``-h`` - *String* (Optional)
    Will report on flags and usage and try to convince the user to read this manual.
    If supplied with an argument (like ``--help=<algorithm>``) will list the algorithm's options and default values.

``--file``, ``-f`` - *String*
    Input file. See the section below for more information.

``--final_frame``, ``-e-`` - *Unsigned Long Long Integer*
    Sets the last frame to which to simulate until.

``--timer``, ``-r`` - *Float*
    Configures how often to update the frames per second and ``benchmark`` duration.

``--algorithm``, ``-a`` - *String*
    Sets the algorithm to use. Use "help" to list all available.

``--threads``, ``-t`` - *Unsigned Integer*
    Sets the amount of threads to use. "0" will try to autodetect and use that.

``-log``, ``-l`` - *String*
    Sets the log file to which to save all command line output at maximum ``verbosity``.

``--verb``, ``-v`` - *Short Unsigned Integer*
    Sets how much information to output to *STDOUT*.

``--lua_val``, ``-u`` - *String*
    String to send as an argument to Lua's spawn_objects function.


These flags will always override anything set inside the configuration input file.


COMMAND LINE INTERFACE
======================

Syntax
------

To get the current value of a variable:
    ``<variable name>``

To set a variable, use:
    ``<variable name> <value>``

To print all variables and their values:
    ``help``

To list all commands available:
    ``list_cmd``

Commands
--------

``save``
    Saves the system to a file.

``load <file>``
    Loads the system from a file. Must still use a valid configuration file!

``element <ID> <R,G,B,A> <VAL>``
    Sets the color for a single element.

``set_view <X,Y,Z> <VAL>``
    Rotates the camera around.

``quit, restart, stop, start, pause, etc.``
    Self explanatory.

``win_draw_mode <MODE>``
    Sets drawing mode. Call with incorrect arguments to list all.

``win_create, win_destroy``
    Self explanatory.

``clear``
    Clears the command line window. Arguably the most used/useful command.

``phys_check_collisions``
    Checks for identical positions among objects. Use the Lua API instead.


CONFIGURATION FILES
===================

Syntax
------

The program takes input in the form of a Lua script. It's used to configure both
the system being simulated as well as the programs by setting variables. The Lua
script **has** to contain a table named "settings", which is the only hardcoded object.
See below to set other Lua function names.

*Settings variables*
--------------------
Used to toggle and adjust options. Some may intersect with command line arguments,
however most do not. For those that do, command line arguments take priority.
All of the variables below are settable via the command line interpreter.

``threads`` - *Unsigned Integer*
    Set the amount of threads to use. Overridden by argument.
``dt`` - *Float*
    Set the time constant.
``rng_seed`` - *Unsigned Integer*
    Sets the RNG seed. Set to 0 to generate a new one on every physics start.
``algorithm`` - *String*
    Set the algorithm to use. Specify help here or in argument to list all.
``spawn_funct`` - *String*
    Name of function to read objects from
``timestep_funct`` - *String*
    Function to execute upon timestep completion
``exec_funct_freq`` - *Integer*
    Auto timestep_funct run frequency
``lua_expose_obj_array`` - *Boolean*
    Expose the object array(updated upon timestep completion), may affect performance.
``screenshot_template`` - *String*
    Template to use in screenshot file creation. Standard sprintf syntax.
``file_template`` - *String*
    Template to use in state file(XYZ) creation. Standard sprintf syntax.
``fontname`` - *String*
    Specify the font type to be used. Example: "Liberation Sans".
``final_frame`` - *Unsigned Long Long Integer*
    Specifies the last frame to simulate to.
``fontsize`` - *Unsigned Integer*
    Fontsize adjustment.
``bgcolor`` - *Table of 4 integers*
    Sets the background color.
``elements_file`` - *String*
    Specify the path to external db for elements. See resources/elements.lua for example.
``dump_sshot`` - *Unsigned Integer*
    Specify the frequency of screenshots taken. 1 - every step, 2 - every two steps, 3...
``dump_xyz`` - *Unsigned Integer*
    Specify the frequency of state dumps created. 1 - every step, 2 - every two steps, 3...
``reset_stats_freq`` - *Unsigned Integer*
    Specify how often to reset global stats. 0 disables, 1 will reset stats every cycle.
``lua_gc_sweep_freq`` - *Unsigned Integer*
    Specify how often to ask Lua to run a full garbage cleaning sweep(default: 1000 cycles).
``step_back_buffer`` - *Unsigned Integer*
    Adjusts the step back buffer size.
``width`` - *Integer*
    Set window width in pixels.
``height`` - *Integer*
    Set window height in pixels.
``epsno`` - *Double*
    Electric force constant. Set either this or ``elcharge`` to 0 to disable.
``elcharge`` - *Double*
    Electrical unit conversion, multiplies object charge.
``gconst`` - *Double*
    Gravitational force constant. Set to 0 to disable such force calculations.
``verbosity`` - *Integer, 0 to 10.*
    Specify the amount of information being outputted to the terminal.
``skip_model_vec`` - *Unsigned Integer*
    When importing a file limit the imported objects. Increase to limit further.
``default_draw_mode`` - *String*
    Specify the default draw mode. Type in "win_draw_mode" in cmd line to get all posttible.
``custom_sprite_png`` - *String*
    Path to texture to load when using **MODE_SPRITE** in default_draw_mode/win_draw_mode.

*Built-in algorithms options*
-----------------------------
``bh_ratio`` - *Float*
    Algorithm specific. Adjusts accuracy and speed.
``bh_lifetime`` - *Short Unsigned Integer*
    Algorithm specific. Set empty cell lifetime before its deletion.
``bh_heapsize_max`` - *Unsigned Integer(bytes, size_t)*
    Algorithm specific. Set limit on maximum octrees per thread.
``bh_tree_limit`` - *Short Unsigned Integer, 1 to 8*
    Algorithm specific. Sets limit on threads per octree. Increase to spread distribution.
``bh_single_assign`` - *Boolean*
    If only a single thread is used will still split the octree normally. Debugging.
``bh_random_assign`` - *Boolean*
    Will split the octrees randomly once a layer of octrees is filled. (lvl1 -> 8, lvl2 -> 64, etc.)
``bh_balance_threshold`` - *Float*
    Sets the balance difference(most vs least populated) before an octree is considered out of balance. [0,1]. 0 = off.
``bh_balance_timeout`` - *Unsigned Integer*
    Sets the timeout on an octree after it has been balanced. Unit is steps. Balancer will not touch it.
``bh_periodic_boundary`` - *Boolean*
    Turns on periodic boundaries.
``bh_boundary_size`` - *Double*
    Sets the cube size for periodicity.

*Object specific variables*
---------------------------
To spawn the objects into the internal array, return the table containing the objects
by the function spawning the objects. The following variables set the properties of
each object.

``pos`` - *Table of 3 doubles*
    Used to position an object
``vel`` - *Table of 3 doubles*
    Initial velocity
``rot`` - *Table of 3 doubles*
    Rotation of an imported object
``charge`` - *Double*
    Charge, if the object should have one.
``mass`` - *Double*
    Mass. Reqired to be non-zero for every object else the algorithms can't handle it.
``radius`` - *Float*
    Sets the radius of the object. Used only in the ball display mode, although future algorithms might use this.
``atom`` - *String*
    If the object should represent an atom. Use Short Standard Periodic table notation("O", "N", "LI", "HE", etc.).
``atomnumber`` - *Unsigned Short Integer*
    Same as the above, except takes numbers. Set to 0 to just use generic object(with white colour).
``state`` - *Integer*
    Specify the state for that particle. No effect in current algorithms, useful when writing your own algorithms.
``id`` - *Unsigned Integer*
    The ID to which the object should be in the internal array. Used only in the Lua exec function. Ignored when
    spawning objects(because there are no guarantees this will be the actual ID if the user imports a model).
``import`` - *String*
    Will import from a file. Currently, Waveform 3D *Obj*, *XYZ* and *PDB* files are supported.
``scale`` - *Float*
    Scale for imported object.
``ignore`` - *Bool*
    Set this flag to prevent the object from being moved. Will still affect others.

*Lua functions*
---------------------------
There exist several functions which you can call from Lua:

``raise(*Unsigned Integer*)``
    Sends a signal to the main program. Use Lua's system IO interface rather than this.
``phys_pause(*nil*)``
    Pauses the simulation.
``phys_check_coords(*Table of objects as specified above*)``
    Checks the coordinates of every object for conflicts. Returns the following:
    ::

    { { pos = *Table of 3 doubles*, id = {ID1, ID2, ID3, ...} },
    { pos = *Table of 3 doubles*, id = {ID4, ID5, ...} }, ...} --and so on

    It's up to you where to move them, but not moving them will possibly cause problems.
``set_option()``
    Sets an option.
``print_text(*String*)``
    Prints a text line on the SDL2 GUI.

*Tables sent to exec_funct in Lua*
----------------------------------
The maps of each algorithm and all global stats are exposed via the first argument as a table.
The second argument will contain the current object array, if enabled, with the same format as the one stated above.
Note that the rng_seed here will reflect the rng_seed used, even if it is not supplied.

FILE IMPORTING
==============
Work in progress, support for more files will be added in the future. In any case,
the user can themselves write any reader in the Lua config file if needed.

- Waveform 3D Obj files is supported. Only vertices will be imported.

- XYZ importing is also functional and will correctly import atom information as well.

- PDB file importing has been partially implemented, with several ATOM variables used.

ENVIRONMENT VARIABLES
=====================
Some libraries used in this program can be controlled using environmental variables:

``SDL_VIDEODRIVER``
    Sets which video driver to use.
``SDL_VIDEO_X11_MOUSEACCEL``
    Sets mouse sensitivity for the X11 video driver.
``SDL_DEBUG``
    Useful when debugging input problems.

For a more exaustive list consult the SDL2 library manual.

SIGNAL HANDLING
===============
The following signal functions have been implemented:

``SIGINT`` -- Will stop the threads, close all files, free all memory and quit.

``SIGUSR1`` -- Will report the current status of the simulation.

EXIT CODES
==========

Normally **physengine** returns *0* as exit code upon quitting manually. Special
cases are listed below:

    :1: General errors.
    :2: File not found.
    :3: Memory allocation errors/out of memory/memory limit reached.

EXAMPLES
========

*Loading a standard simulation:*
    ``glauss_client -f simconf.lua``

*Don't simulate anything, just display(default):*
    ``glauss_client -f simconf.lua -a none``

*Only the control thread running, Lua-only algorithm:*
    ``glauss_client -f simconf.lua -a null``

*Same as above, but pass an argument to the spawn function*
    ``glauss_client -f simconf.lua -a null -u lua_custom_option=0.412``

*Dummy load sim, will use the n-body algorithm to display stats:*
    ``glauss_client -f simconf.lua -a null_stats``

*Simulate using the n-body algorithm using 3 threads:*
    ``glauss_client -f simconf.lua -t 3 -a n-body``

*Use the Barnes-Hut algorithm with 4 cores and create a logfile:*
    ``glauss_client -f simconf.lua -t 4 -a barnes-hut -l phys.log``
