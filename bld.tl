return {

    {
        not_dependencies = {
            "lfs",
            "resvg",
        },
        artifact = "mgc",
        main = "mgc_main.c",
        src = "src",
        exclude = {
            --"t80_stage_empty.c",
        },
        debug_define = {
            ["BOX2C_SENSOR_SLEEP"] = "1",
        },
        release_define = {
            --["T80_NO_ERROR_HANDLING"] = "1",
            --["KOH_NO_ERROR_HANDLING"] = "1",
        },
    },
    --]]

    --[[
    {
        not_dependencies = {
            "lfs",
        },
        artifact = "export_height2colors",
        main = "export_height2colors.c",
        src = "src",
    },
    --]]

}
