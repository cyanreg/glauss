glauss
======
The **G**eneralized **L**ua-**AU**gmented **S**imulation **S**ystem

Overview
--------
This program aims to implement a physics simulation engine capable of doing simulations on various scales, from molecular dynamics to planetary motion to Monte Carlo, all whilst maintaining maximum performance. A user-friendly Lua API is provided to experiment and quickly test and deploy algorithms. Implemented minimalistically using the SDL2, Freetype 2, Lua 5.2 and the OpenGL ES 2.0 libraries. Uses POSIX threads to implement multi-threading. Written in standard C and released under the GPLv3 License.

Compiling
---------
Complete list of dependencies:

 * gcc (4.9 or newer)  or clang (3.5 or newer)
 * OpenGL ES 2.0 development libraries (Debian: libgles2-mesa-dev, Arch: libgles(mesa))
 * Freetype 2 development library (Debian: libfreetype6-dev; Arch, Fedora: freetype2)
 * Fontconfig development library(Debian: libfontconfig1-dev, Arch: fontconfig)
 * SDL 2.0 development library (Debian: libsdl2-dev; Arch, Fedora: sdl2)
 * Lua (Luajit recommended, Lua 5.2 supported, Lua 5.1 not recommended)

All are available on any up-to-date Linux distribution's package repositories. To compile and install:

`./bootstrap.py`

`./waf configure`

`./waf build`

`./waf install`

In case program has been installed, use the `glauss_client` command to run, with appropriate arguments. To uninstall, use `./waf uninstall` in the original project folder.

The program can also be run in any directory as it has no dependence on any external files. For developer convenience, it's possible to avoid program installation. Just build and run the shell script without installing.

This program **absolutely requires** the newest stable GCC or Clang releases to build. Vector extensions have always been a tricky things in C, but in the last few years the major compilers have roughly agreed on a standard. When using GCC, check that your processors has AVX extensions or performance will probably be degraded.

Controls and Usage
------------------
Run the program with the --help flag and/or consult the [manual](DOCS/glauss.rst#controls).

Running
-------
Run the program with the argument `-f (filename)`, with an appropriate file. A demo file(usually it's for testing the latest feature) called simconf.lua is present. A demo scripts directory will be created at a later version. The executable needs to be ran into the root directory of the build. Modify the options in simconf.conf to change the system being simulated. Modify program behaviour with the arguments listed further below. Pressing 'z' will create an XYZ file snapshot of the current system, which you can open with any molecule viewer (like VMD or GDIS).

Command line
------------
A command line is included if libreadline is installed. It allows to change almost every aspect of the simulation. Type "help" to list all possible commands and variables(with values printed out). Typing a variable alone will print its current value. Typing `<variable> <value>` changes its current value. Note that for some options like `threads` a restart is needed in order for them to take effect. Using Lua to change a variable is possible as well using the `set_option("<variable>", value)` function.

Input file
----------
The filepath/name specified through the `-f` flag determines the system to simulate. It's a Lua script, so make sure to follow the syntax. All Lua libraries(including math) are included and initialized, feel free to use them. Error handling is done by Lua interpreter. You'll probably want this to be the first argument, as any option put before will get overwritten if it exists in the file.

Physengine reads the settings table first, then calls the spawn_objects function, which returns the objects table. That table is then read, counted and scanned for any files that need importing. Once the final count is known, memory is allocated and the table is read again, importing all objects.

A function to be called on every timestep can be defined, with its frequency adjusted. The statistics and optionally, all objects will be sent as arguments. The objects send to this function have the exact same structure as the ones send from Lua to C during spawning except for the addition of their internal `id` variable. A non-nil table of objects returned by this function will be read back and will overwrite the internal objects, by using the `id` variable of each object's table. Use it to program your own algorithm entirely in Lua.

For a list of all variables look in the appropriate section of the [manual](DOCS/glauss.rst#configuration-files).

Bug reports
-----------
Use either the Github Issues section of this project or the contact section below.

FAQ
---
>I've imported an object and now every ID has an offset and doesn't match the index!

The `timestep_funct` is executed once on physics startup, just before the threads are online. Use this to get the internal IDs of the objects. Nothing will have moved, so by comparing the positions of all objects with the initial positions you can know which object is which.

>The Barnes-Hut algorithm has a bug! When having more than 8 threads, there's a thread with absolutely no octrees in it!

That's a hindsight of the way the threading works here. We subdivide octrees and give them out once a layer gets filled. But if the slices are empty(for instance when simulating a galaxy we randomly give the upper part of the upper octree where there are no objects) then it's logical there would be no octrees since there are no objects. Bottomline is, to have every thread have octrees you need a uniform distribution of objects geometrically(e.g. in a cube). Or you can just wait patiently so that objects eventually creep up in the empty space and the thread gets a job. Don't like it? Turn off random slice allocation(look in options) or better yet, create an algorithm to better distribute the threads. Or better yet, turn on the EXPERIMENTAL auto-balancer which moves the octrees in threads above a certain ratio.

>The program has crashes once physics is started with imported objects!

Are you importing an `.obj` file? We interpret the vertices as points so you might get objects sharing the same coordinates. Use the Lua function `phys_check_coords` to get a table of the IDs of the matching positions of objects(check DOCS/glauss.rst for info on function) upon initialization and move them before the threads start.

>I'd like to spawn objects while the simulation is running, is that possible?

Work in progress on that feature. For now, you can just put the objects far far away, set the `ignore` flag to True, set their mass to close to 0 and bring them in action when their time comes.

Contributing
------------
Got a bug fix? New feature? Send a pull request here on Github. Demo Lua project scripts are accepted. Just please use the project's coding style (pretty much the Linux kernel coding style, indents are 4 spaces). You can use the `--bench` option to benchmark the performance and see how it's affected.
