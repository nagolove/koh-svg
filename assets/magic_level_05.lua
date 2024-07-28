-- vim: set colorcolumn=85
-- vim: fdm=marker

package.path = package.path .. ";./*.lua"

--[[
package.сpath = package.сpath .. ";./*.so"
-- Использовать только для отладки. Скорее всего не будет работать в WASM
local cffi = require 'cffi'
cffi.cdef("int printf(char const *fmt, ...);")
ffi.C.printf("hello %s\n", "world")
--]]

print("fith level script, your are welcome!")

local inspect = require "inspect"
--local tabular = require "tabular".show

-- TODO: Поддержка работы с сущностями.
local function on_sensor_1()
    print("on_sensor_start_1:")
end

local function on_sensor_2()
    print("on_sensor_start_2:")
end

local function on_sensor_3()
    print("on_sensor_3:")
end

-- e - lightuserdata?
local function on_sensor_kill(e)
    -- Какая-то работа с obj
    -- obj - proxy obj для связки скрипта с ecs и убийства сущности

    e_kill()

    print("on_sensor_kill:")
end

-- Вызывает один раз после загрузки уровня
function load()
    print("load in script");

    -- Добавляет сенсор с функцией обратного вызова
    mgc.sensor_add(
        "start",
        { x = 100, y = 100, radius = 100 },
        on_sensor_1
    )
    mgc.sensor_add(
        "intermediate",
        { x = 1000, y = 700, radius = 700 },
        on_sensor_2
    )
    mgc.sensor_add(
        "end",
        { x = 1000, y = 700, radius = 700 },
        on_sensor_3
    )
    mgc.sensor_add(
        "kill",
        { x = 100, y = 700, radius = 700 },
        on_sensor_kill
    )
end

--print("_G", inspect(_G))
--print("_G", tabular(_G))

-- Вызывает один раз после завершения уровня
-- XXX: Где находится завершение уровня?
function unload()
    print("unload")
end

function update()
    --print("update: dt", GetFrameTime())
end

local fnt_size = 200
--local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf", fnt_size)
local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf")
local pos = Vector2(200, 200)
local rotation = 0
local msg = "Congrats! You created your first window!";
--RLAPI Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);    // Measure string size for Font
local msg_width = MeasureTextEx(fnt, msg, fnt_size, 0.)

function draw_pre()
    -- {{{
    --RLAPI void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint); // Draw text using font and additional parameters
    --pos.x
--RLAPI void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint); // Draw text using Font and pro parameters (rotation)
    DrawTextPro(
        fnt, 
        msg,
        pos, 
        Vector2(msg_width.x / 2., fnt.baseSize / 2.),
        rotation, -- rotation
        fnt_size,  -- fontSize
        0,    -- spacing
        --LIGHTGRAY
        RED
    )

    --[[
    --[[
    DrawTextEx(
        --GetFontDefault(),
        fnt,
        "Congrats! You created your first window!",
        pos,
        80.,  -- font size
        0.,   -- spacing
        LIGHTGRAY
    )
    --]]

    rotation = (rotation + 1) % 360
    -- }}}
end

function draw_post()

end
