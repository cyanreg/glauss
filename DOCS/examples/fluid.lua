--Physengine example config file. Syntax is standard Lua. Math libraries are included and initialized.
settings = {
    --Only the names of the variables are used. Tables are just for organization(except settings), feel free to drop them.
    physics = {
        threads = 1,
        dt = 0.00016,
        algorithm = "barnes-hut",
        bh_ratio = 0.990,
        --Lifetime of a cell before it's freed.
        bh_lifetime = 20,
        --Units are size_t(bytes)! PER THREAD!
        bh_heapsize_max = 336870912,
        --Maximum threads per octree. Reduce this to spread threads more.
        bh_tree_limit = 8, --Range is [2,8(default)]
        --If only a single thread is available, assign the entire root to it.
        bh_single_assign = true,
        --Assign the octrees of the threads randomly.
        bh_random_assign = true,
        bh_viscosity = true,
        bh_viscosity_cutoff = 10,

    },
    lua_settings = {
        spawn_funct = "spawn_objects",
        --Name of function to read objects from
        timestep_funct = "run_on_timestep",
        --Function to execute upon timestep completion
        exec_funct_freq = 100, --Auto timestep_funct run frequency
        lua_expose_obj_array = false;
        --Expose object array to the timestep_funct, slight performance decrease
        reset_stats_freq = 1;
        --Disable any averaging and reset global stats every cycle
        step_back_buffer = 0,
    },
    visual = {
        width = 1024,
        height = 600,
        screenshot_template = "sshot_%08i.png",
        file_template = "system_%0.2Lf.xyz",
        fontname = "Liberation Sans",
        fontsize = 38,
        verbosity = 8,
        dump_sshot = 0, --Auto screenshot dump frequency
        dump_xyz = 0, --Auto xyz dump frequency
        skip_model_vec = 0,
        default_draw_mode = "MODE_POINTS",
    },
    constants = {
        gconst = 6.67384*10^-8,
    },
}

tot_objects = 100000
stream_size = {300,300,1000}
stream_pos = {0,0,-575}

--Add molecules or any additional objects here
objects = {
 	{
 		import = "resources/ball.obj",
 		scale = 0.50,
 		pos = {0,0,0},
 		vel = {0,0,0},
 		mass = -1000000,
 		rot = {0,math.pi/2,math.pi/2},
 		ignore = true,
 	}
}

function spawn_objects(string_from_arg)
    print("Sent value", string_from_arg)
    math.randomseed( os.time() )
    scale_obj = 45
    for i = #objects+1, tot_objects, 1 do

        objects[i] = {
            pos = {
                (math.random()-0.5)*stream_size[1] + stream_pos[1],
                (math.random()-0.5)*stream_size[2] + stream_pos[2],
                (math.random()-0.5)*stream_size[3] + stream_pos[3]
            },
            vel = {
                0,
                0,
                100,
            },
            mass = -100,
            radius = 0.2,
            atomnumber = math.random(1,10),
            ignore = false,
        }
    end

    return objects
end

--Consult physics/physics.h for the format of struct thread_statistics and typedef data
function run_on_timestep(t_stats, obj)
    print("Current progress:", t_stats.progress)

    return nil
end
