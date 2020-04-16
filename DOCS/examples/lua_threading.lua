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
        skip_model_vec = 200,
    },
}

--Add molecules or any additional objects here
objects = {}

function spawn_objects(string_from_arg)
    print("Sent value", string_from_arg)

    local cube_size = 5
    local z = 1
    local velocity = 10

    local objects = {}
    for i = 0, cube_size, 1 do
        for j = 0, cube_size, 1 do
            for k = 0, cube_size, 1 do
                local atomno = math.random(1,100)
                if atomno < 60 then
                    atomno = 1
                elseif atomno < 87 then
                    atomno = 2
                else
                    atomno = 3
                end
                objects[z] = {
                    pos = {
                        i-(cube_size/2),
                        j-(cube_size/2),
                        k-(cube_size/2),
                    },
                    vel = {
                        0,
                        0,
                        0,
                    },
                    acc = {
                        0,
                        0,
                        0,
                    },
                    state = 0,
                    mass = 1,
                    radius = 0.1,
                    atomnumber = atomno,
                    ignore = false,
                }
                z = z + 1
            end
        end
    end

    objects[z] = {
        pos = {
            cube_size-(cube_size/2),
            cube_size-(cube_size/2),
            cube_size-(cube_size/2),
        },
        vel = {
            0,
            0,
            0,
        },
        acc = {
            0,
            0,
            0,
        },
        state = 0,
        mass = 1,
        radius = 0.1,
        atomnumber = math.random(1,2),
        ignore = false,
    }

    return objects
end

--Consult physics/physics.h for the format of struct thread_statistics and typedef data
function run_on_timestep(t_stats, obj)
    local j = 1
    local thread_objs = {}

    if (obj == nil) then
        return nil
    end

    for i = obj.range_low, obj.range_high-1, 1 do
        thread_objs[j] = obj[i]
        thread_objs[j].pos[1] = thread_objs[j].pos[1] + (1+obj.thread_id)/100
        j = j + 1
    end

    thread_objs[j] = obj[obj.range_high-1]

    return thread_objs
end

