-- vim: fdm=marker

package.path = package.path .. ";./*.lua"
local random = math.random
local inspect = require "inspect"

local function on_sensor_1(e)
    print(e_tosting(e))
end

function load()
    -- {{{
        
    print('load')

    mgc.sensor_create("sensor_01", {
        x = 100,
        y = 200,
        radius = 200,
    }, on_sensor_1)

    --[[
    -- TODO: Передавать все аргументы в виде таблицы
    mgc.sensor_create({
        name = "sensor_01",
        x = 100,
        y = 200,
        radius = 200,
        on_enter = on_sensor_1,
    })
    --]]

    mgc.emmiter_create({
        name = "emitter_01",
        x = 1000,
        y = 0,
        -- радиуса нет, так как эммитер - точка
        on_emit = function(e)
            print("enitity created", e_tosting(e));
        end,
    })

    -- }}}
end

function unload()
    print('unload')
end

local updated = false

local rad = 200
local drad, ddrad = 1, 0.01

function update()
    --[[
    rad = rad + drad
    if rad > 1000 or rad <= 0 then
        drad = -drad
    end

    drad = drad + drad

    if drad > 3 or ddrad <= 1 then
        ddrad = -ddrad
    end
    --]]

    if not updated then
        print('UPDATE')
        updated = true
    end

end

function draw_pre()
    DrawCircle(100, 100, rad, RED)
end

function draw_post()
    --DrawCircle(100, 100, rad, RED)
end

return {
    name = "Пробный уровень",
    description = "Провести хотя-бы один шар из точки А в точку Б.",
}
