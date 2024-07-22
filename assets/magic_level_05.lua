-- vim: set colorcolumn=85
-- vim: fdm=marker

package.path = package.path .. ";./*.lua"

print("fith level script, your are welcome!")

local inspect = require "inspect"
--local tabular = require "tabular".show

local function on_sensor()
    print("sensor_start:")
end

-- Вызывает один раз после загрузки уровня
function load()
    print("load in script");

    -- Добавляет сенсор с функцией обратного вызова
    mgc.sensor_add(
        "start",
        { x = 100, y = 100, radius = 100 },
        on_sensor
    )
    mgc.sensor_add(
        "intermediate",
        { x = 1000, y = 700, radius = 700 },
        on_sensor
    )
    mgc.sensor_add(
        "end",
        { x = 1000, y = 700, radius = 700 },
        on_sensor
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
