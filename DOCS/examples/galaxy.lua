--Physengine example config file. Syntax is standard Lua. Math libraries are included and initialized.
settings = {
    --Only the names of the variables are used. Tables are just for organization(except settings), feel free to drop them.
    physics = {
        threads = 1,
        dt = 0.00016,
        algorithm = "barnes-hut",
        bh_ratio = 0.5,
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
        bh_periodic_boundary = true,
        bh_boundary_size = 500,
        bh_viscosity = true,
        bh_viscosity_cutoff = 3,
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
        step_back_buffer = 20,
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
        skip_model_vec = 2,
        default_draw_mode = "MODE_POINTS",
    },
    constants = {
        gconst = 6.67384*10^-8,
    },
}

--Add molecules or any additional objects here
objects = {
-- 	{
-- 		import = "./molecules/1UAO.pdb",
-- 		scale = 1,
-- 		pos = {0,0,0},
-- 		vel = {0,0,0},
-- 		rot = {0,math.pi/2,math.pi/2},
-- 		ignore = false,
-- 	}
}

-- Any objects returned to or from the main program have the following format:
-- table/array_of_objects = {
-- 	{ --object
-- 		pos = {table/vector of 3 values},
-- 		vel = {table/vector of 3 values},
-- 		chage = value,
-- 		mass = value,
-- 		radius = value,
-- 		atomnumber = value,
-- 		ignore = value,
-- 	},
-- 	{ --object
-- 		...
-- 	},
-- 	...
-- }

fuzz = 2.0
arm_rotations = 0.5
arm_width = 69.0
arm_number = 3
disk_radius = 20.0
disk_width = 1.0
disk_stars = 4000
hub_radius = 10.0
hub_stars = disk_stars/2
hub_width = 4.0

function spawn_objects(string_from_arg)
    print("Sent value", string_from_arg)
    math.randomseed( os.time() )
    scale_obj = 45
    for i = #objects+1, disk_stars+1, 1 do
        dist = (hub_radius + math.random()*disk_radius)
        theta = ((360.0*arm_rotations*(dist/disk_radius)) + math.random()*arm_width
            + (360.0/arm_number)*math.random(0, arm_number)
            + math.random()*fuzz*2.0 - fuzz )

        objects[i] = {
            pos = {
                math.cos(theta*math.pi/180.0)*dist,
                math.random()*disk_width*2.0 - disk_width,
                math.sin(theta*math.pi/180.0)*dist,
            },
            vel = {
                math.sin(theta*math.pi/180.0)*dist,
                0,
                -math.cos(theta*math.pi/180.0)*dist,
            },
            mass = 100000,
            radius = 0.2,
            atomnumber = math.random(1,10),
            ignore = false,
        }
    end
    for i = #objects+1, #objects+1+hub_stars+1, 1 do
        dist = math.random()*hub_radius
        theta = math.random()*360

        objects[i] = {
            pos = {
                math.cos(theta*math.pi/180.0)*dist,
                (math.random()*2 - 1)*(hub_width - (hub_width/(hub_radius*hub_radius))*dist*dist),
                math.sin(theta*math.pi/180.0)*dist,
            },
            vel = {
                math.sin(theta*math.pi/180.0)*dist,
                0,
                -math.cos(theta*math.pi/180.0)*dist,
            },
            mass = 100000,
            radius = 0.2,
            atomnumber = math.random(1,10),
            ignore = false,
        }
    end

    objects[#objects+1] = {
        pos = { 0,0,0 },
        vel = { 0, 100, 0 },
        mass = 100,
        radius = 0.2,
        atomnumber = math.random(1,10),
        ignore = false,
    }

    return objects
end

--Consult physics/physics.h for the format of struct thread_statistics and typedef data
function run_on_timestep(t_stats, obj)
    print("Current progress:", t_stats.progress)

    return nil
end
