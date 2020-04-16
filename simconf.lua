--Physengine example config file. Syntax is standard Lua. Math libraries are included and initialized.
settings = {
    --Only the names of the variables are used. Tables are just for organization(except settings), feel free to drop them.
    physics = {
        threads = 1,
        dt = 0.002322312,
        algorithm = "null",
        rng_seed = os.time(),
        bh_periodic_boundary = true,
        bh_boundary_size = 10.0,
    },
    lua = {
        spawn_funct = "spawn_objects",
        --Name of function to read objects from
        timestep_funct = "run_on_timestep",
        --Function to execute upon timestep completion
        exec_funct_freq = 1, --Auto timestep_funct run frequency
        lua_expose_obj_array = true,
        --Expose object array to the timestep_funct, slight performance decrease
    },
    visual = {
        bgcolor = {32, 32, 32, 255},
        default_draw_mode = "MODE_SPHERE",
        width = 1280,
        height = 720,
        screenshot_template = "sshot_%08i.png",
        file_template = "system_%0.2Lf.xyz",
        fontname = "Liberation Sans",
        fontsize = 38,
        verbosity = 8,
        dump_sshot = 0, --Auto screenshot dump frequency
        dump_xyz = 0, --Auto xyz dump frequency
        skip_model_vec = 40,
    },
}

--Generally useful, returns a STRING
inspect = require('TOOLS/inspect')
--Useage: print
vector = require('TOOLS/vector')

function spawn_objects(string_from_arg)
    print("Sent value", string_from_arg)
    math.randomseed( settings.physics.rng_seed )

    local cube_size = 4
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
                        10,
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

function measure_energy(obj)
    energy = 0.0

    local dist_vec = {}
    local dist = 0.0

    local epsilon11 = 1.0
    local epsilon12 = 1.0
    local epsilon22 = 1.2
    local alpha11 = 1.0
    local alpha12 = 0.5
    local alpha22 = 0.1
    local sigma11 = 1.0
    local sigma12 = 1.0
    local sigma22 = 1.0

    local sigma = 0.0
    local epsilon = 0.0
    local alpha = 0.0
    local sum = 0

    for i = 1, #obj, 1 do
        for j = 1, #obj, 1 do
            dist_vec = vector_sub(obj[j].pos, obj[i].pos)
            dist = math.sqrt(vector_det(vector_mul(dist_vec, dist_vec)))
            if dist > 2.9 then goto continue end
            sum = obj[i].atomnumber + obj[j].atomnumber
            if sum < 4 then
                --Empty
                goto continue
            elseif sum == 4 then
                epsilon = epsilon11;
                alpha = alpha11;
                sigma = sigma11;
            elseif sum == 5 then
                if obj[i].atomnumber == 1 or obj[j].atomnumber == 1 then
                    goto continue
                end
                epsilon = epsilon22;
                alpha = alpha22;
                sigma = sigma22;
            end
            energy = energy + 4*epsilon*( sigma^12 - alpha*(sigma^6) );
            ::continue::
        end
    end

    return energy
end

temperature = 2.7

rejects = 0
accepts = 0
rand_accepts = 0

--Consult physics/physics.h for the format of struct thread_statistics and typedef phys_obj
function run_on_timestep(t_stats, obj)
    --print("Current progress:", t_stats.progress)
    --print("Steps = ", t_stats.total_steps, "Time per step = ", t_stats.time_per_step)

    --if t_stats.progress > 0.0 then phys_pause() end
    if obj == nil then return nil end

    local old_energy = measure_energy(obj)

    local src_index = 1
    local dest_index = 1

    while src_index == dest_index do
        while obj[src_index].atomnumber == obj[dest_index].atomnumber do
            src_index = math.random(1, #obj)
            dest_index = math.random(1, #obj)
        end
    end

    --Remember, Lua arrays are indexed from 1(by default, but it doesn't care)
    --however our internal objects are indexed from 0. We send lua stuff indexed from 1
    --but we read the IDs of the objects, not the order in which the objects arrive.
    --Since ids are indexed from 0 and if no objects are imported follow the index
    --of the array offset the obj count such that we match the internal db.

    local temp_type = obj[src_index].atomnumber
    obj[src_index].atomnumber = obj[dest_index].atomnumber
    obj[dest_index].atomnumber = temp_type

    local newenergy = measure_energy(obj)
    local diff = newenergy-old_energy

    local text = "Accepts = " .. accepts .. "    Accepts(rand) = " .. rand_accepts .. "    Rejects = " .. rejects .. "    Ediff = " .. diff
    print_text(text)

    local rettable = {obj[src_index], obj[dest_index]}

    if newenergy < old_energy then
        accepts = accepts + 1
        return rettable
    else
        local wtrans = math.exp(-(diff/temperature));
        if math.random() < wtrans then
            rand_accepts = rand_accepts + 1
            return rettable
        else
            rejects = rejects + 1
            return nil
        end
    end
end
