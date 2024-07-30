-- vim: set colorcolumn=85
-- vim: fdm=marker


-- XXX: Как для teal сделать загрузку модуля?
--local mgc = require 'mgc'

package.path = package.path .. ";./*.lua"

print("fith level script, your are welcome!")

local random = math.random
local inspect = require "inspect"
--local tabular = require "tabular".show

-- e - lightuserdata with de_entity
local function on_sensor(e)
    print("sensor_start:", e_tostring(e))
end

--print("_G", inspect(_G))
--print("_G", tabular(_G))

-- Вызывает один раз после завершения уровня
-- XXX: Где находится завершение уровня?
function unload()
    print("unload")
end

local fnt_size = 200
--local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf", fnt_size)
local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf")
local pos = Vector2(200, 200)
local rotation = 0
local msg = "Congrats! You created your first window!";
--RLAPI Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);    // Measure string size for Font
local msg_width = MeasureTextEx(fnt, msg, fnt_size, 0.)

-- {{{ Фоновые приключения шариков

-- gpt
function catmullRom(p0, p1, p2, p3, t)
    return 0.5 * ((2 * p1) +
                  (-p0 + p2) * t +
                  (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                  (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t)
end

-- gpt
function interpolateCatmullRom(trajectory, t)
    print("interpolateCatmullRom: t", t)
    local segmentCount = #trajectory - 1
    local segment = math.floor(t * segmentCount) + 1
    local localT = (t * segmentCount) % 1 -- XXX: Что за %1 ??

    local p0 = trajectory[math.max(segment - 1, 1)]
    local p1 = trajectory[segment]
    local p2 = trajectory[math.min(segment + 1, #trajectory)]
    local p3 = trajectory[math.min(segment + 2, #trajectory)]

    return {
        x = catmullRom(p0.x, p1.x, p2.x, p3.x, localT),
        y = catmullRom(p0.y, p1.y, p2.y, p3.y, localT)
    }
end

local function circles_create()
    local t = {}
    local num = 1
    local traectory_len = 10
    local radius = {100, 200}
    local color_alpha = {255, 255}

    for i = 1, num do
        local traectory = {}

        table.insert(traectory, {
            x = GetScreenWidth() / 2,
            y = GetScreenHeight() / 2,
        })

        for j = 1, traectory_len do
            table.insert(traectory, {
                --x = random(-GetScreenWidth(), GetScreenWidth()),
                --y = random(-GetScreenHeight(), GetScreenHeight()),
                
                x = random(0, GetScreenWidth()),
                y = random(0, GetScreenHeight()),
            })
        end

        table.insert(t, {
            --x = random(1, GetScreenWidth()),
            --y = random(1, GetScreenHeight()),
            
            x = GetScreenWidth() / 2,
            y = GetScreenHeight() / 2,

            radius = random(radius[1], radius[2]),
            color = Color(255, 10, 10, random(color_alpha[1], color_alpha[2])),
            traectory = traectory,
        })

        print("circles_create: ", i)
    end

    print("circles_create: t", inspect(t))
    return t
end

local function circles_draw(circles)
    for i, circ in ipairs(circles) do
        DrawCircle(circ.x, circ.y, circ.radius, circ.color)
    end
end

local circles_time = 0

local function circles_update(circles)
    assert(type(circles) == 'table')

    circles_time = circles_time + GetFrameTime() / 30.
    if circles_time >= 1 then
        circles_time = 0.
    end

    --print("circles_time", circles_time)
    --print("circles_update: #circles", #circles)

    for i, circ in ipairs(circles) do
        --print("i", i)
        --local pos = interpolateCatmullRom(circ.traectory, circles_time)
        --print("circles_update: pos", inspect(pos))
        --circ.x = pos.x
        --circ.y = pos.y

        circ.x = circ.x + random(-1, 1)
        circ.y = circ.y + random(-1, 1)
    end
end

-- }}} 

local circles = circles_create()

function draw_pre()
    -- {{{

    circles_draw(circles)

    --[[
    DrawTextPro(
        fnt, msg, pos, 
        Vector2(msg_width.x / 2., fnt.baseSize / 2.),
        rotation, -- rotation
        fnt_size,  -- fontSize
        0,    -- spacing
        --LIGHTGRAY
        RED
    )
    --]]

    rotation = (rotation + 1) % 360
    -- }}}
end

function draw_post()
end

function update()
    --print("update: dt", GetFrameTime())
    circles_update(circles)
end

-- Вызывает один раз после загрузки уровня
function load()
    print("load: in script");

    print(mgc.e_tostring(10))

    -- {{{ Добавляет сенсор с функцией обратного вызова
    mgc.sensor_create(
        "start",
        { x = 100, y = 100, radius = 100 },
        on_sensor
    )
    mgc.sensor_create(
        "intermediate",
        { x = 1000, y = 700, radius = 700 },
        on_sensor
    )
    mgc.sensor_create(
        "end",
        { x = 1000, y = 700, radius = 700 },
        on_sensor
    )
    -- }}}
    --]]

    --circles = circles_create()
end

