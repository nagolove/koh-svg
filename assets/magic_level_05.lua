
package.path = package.path .. ";./*.lua"

print("fith level script, your are welcome!")

local inspect = require "inspect"
local tabular = require "tabular".show

local function sensor_start()
    print("sensor_start:")
end

-- Вызывает один раз после загрузки уровня
function load()
    print("load");


    -- Добавляет сенсор с функцией обратного вызова
    --sensor_add("circle", { x = 100, y = 100, radius = 100 }, sensor_start)
    --sensor_add("circle", { x = 1000, y = 700, radius = 700 }, sensor_start)
end

--print("_G", inspect(_G))
print("_G", tabular(_G))

-- Вызывает один раз после завершения уровня
-- XXX: Где находится завершение уровня?
function unload()
    print("unload")
end

function update()
    --print("update: dt", GetFrameTime())
end
