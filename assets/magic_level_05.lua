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
    print("on_sensor_1:")
end

local function on_sensor_2()
    print("on_sensor_2:")
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

local   frag_background,   -- userdata by Sol
        frag_shadertoy_ctx -- userdata

-- Вызывает один раз после загрузки уровня
function load()
    frag_background = LoadShader(
        nil, "assets/frag/raymarched_hexagonal_truchet.glsl"
    )
    frag_shadertoy_ctx = mgc.shadertoy_init(frag_background);

--RLAPI int GetShaderLocation(Shader shader, const char *uniformName);       // Get shader uniform location
--RLAPI int GetShaderLocationAttrib(Shader shader, const char *attribName);  // Get shader attribute location
--RLAPI void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType);               // Set shader uniform value

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

local fnt_size = 200
local fnt_size_actual = fnt_size
--local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf", fnt_size)
local fnt = LoadFont("assets/fonts/jetbrainsmono.ttf")
local pos = Vector2(200, 200)
local rotation = 0
local msg = "Congrats! You created your first window!";
--RLAPI Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);    // Measure string size for Font

function draw_pre()
    -- {{{
        
    local w, h = GetScreenWidth(), GetScreenHeight()
    BeginShaderMode(frag_background)
    mgc.shadertoy_pass(frag_shadertoy_ctx, w, h);
    DrawRectangle(0, 0, w, h)
    EndShaderMode()

    local msg_width = MeasureTextEx(fnt, msg, fnt_size_actual, 0.)
    local shadow_delta = Vector2(2., 1.);

    DrawTextPro(
        fnt, msg, pos, Vector2(msg_width.x / 2., fnt.baseSize / 2.),
        rotation, -- rotation
        fnt_size_actual,  -- fontSize
        0,   BLACK
    )
    DrawTextPro(
        fnt, msg, Vector2Add(pos, shadow_delta), 
        Vector2(msg_width.x / 2., fnt.baseSize / 2.), rotation, -- rotation
        fnt_size_actual,  -- fontSize
        0,    -- spacing
        RED
    )

    rotation = (rotation + 1) % 360
    -- }}}
end

function draw_post()

end

local fnt_delta = -0.5

function update()
    fnt_size_actual = fnt_size_actual + fnt_delta
    if (fnt_size_actual <= 0 or fnt_size_actual >= fnt_size) then
        fnt_delta = - fnt_delta
    end

    --print("fnt_size_actual", fnt_size_actual)
    --print("update: dt", GetFrameTime())
end


return {
    name = "Торжество изолиний",
    description = "Провести хотя-бы один шар из позиции А в позицию Б.",
}
