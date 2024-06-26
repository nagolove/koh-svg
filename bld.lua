return {

   {
      not_dependencies = {
         "lfs",
         "resvg",
      },
      artifact = "mgc",
      main = "mgc_main.c",
      src = "src",
      exclude = {},


      debug_define = {
         ["BOX2C_SENSOR_SLEEP"] = "1",
      },
      release_define = {},



   },













}
