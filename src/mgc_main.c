// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_script.h"
#include "koh_stages.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#else
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <dirent.h>
#endif

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

// include {{{

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh.h"
#include "raylib.h"
#include <assert.h>
#include "stage_shot.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// }}}

Color color_background_clear = GRAY;

struct Input2 *input2 = NULL;
//struct t80_args app_args;

struct Script {
    char *name, *description, *fname;
};

struct ScriptsStore {
    struct Script   *scripts;
    int             num;
};

//#define LAPTOP  1

/*
// {{{
//Возможность загружать спрайты, раскладку из Aseprite.
//Установка координат объектов(корпус, колеса), привязка имени к выделенным 
//объектам. Связь объектов.
struct Sprite {
};

struct SpritesStore {
    struct Sprite   *sprites;
    int             num;
};
// }}}
*/

static struct dotool_ctx *testing_ctx = NULL;

//static struct ScriptsStore scripts_store = {};

static double last_time = 0.;

#ifdef LAPTOP
static const int screen_width_desk = 1920 * 1;
static const int screen_height_desk = 1080 * 1;
#else
static const int screen_width_desk = 1920 * 2;
static const int screen_height_desk = 1080 * 2;
#endif

#ifdef PLATFORM_WEB
static const int screen_width_web = 1920;
static const int screen_height_web = 1080;
#endif
static HotkeyStorage hk_store = {};
static StagesStore *ss = NULL;

static void console_on_enable(HotkeyStorage *hk_store, void *udata) {
    trace("console_on_enable:\n");
    //hotkey_group_enable(hk_store, HOTKEY_GROUP_FIGHT, false);
}

static void console_on_disable(HotkeyStorage *hk_store, void *udata) {
    trace("console_on_disable:\n");
    //hotkey_group_enable(hk_store, HOTKEY_GROUP_FIGHT, true);
}

/*
static void gui_scripts_loader() {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("scripts loader", &open, flags);

    struct Script *scripts = scripts_store.scripts;
    for (int i = 0; i < scripts_store.num; i++) {
        igSetNextItemOpen(true, ImGuiCond_Once);
        if (igTreeNode_Ptr((void*)(uintptr_t)i, "%d", i)) {
            if (scripts[i].name)
                igText("name %s", scripts[i].name);
            if (scripts[i].fname)
                igText("file name %s", scripts[i].fname);
            if (scripts[i].description)
                igText("description %s", scripts[i].description);

            igText(u8"привет");
    
            igTreePop();
        }
    }

    igEnd();
}
*/

/*
static struct Script script_fill(lua_State *lua) {
    assert(lua);
    struct Script sc = {};

    lua_pushstring(lua, "description");
    lua_gettable(lua, -2);

    if (lua_isstring(lua, -1)) {
        const char *description = lua_tostring(lua, -1);
        if (description)
            sc.description = strdup(description);
    }
    lua_remove(lua, -1);

    lua_pushstring(lua, "name");
    lua_gettable(lua, -2);
    if (lua_isstring(lua, -1)) {
        const char *name = lua_tostring(lua, -1);
        if (name)
            sc.name = strdup(name);
    }
    lua_remove(lua, -1);

    return sc;
}
*/

/*
static int calc_scripts_num(DIR *dir, struct small_regex *regex) {
    int num = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir))) {
        int found = regex_matchp(regex, entry->d_name);
        if (found == -1)
            continue;
        num++;
    }
    return num;
}
*/

/*
struct ScriptsStore scripts_loaders_init() {
    assert(types_getlist());

    struct ScriptsStore ss = {};
    const char *base_path = "assets/store_return";
    assert(base_path[strlen(base_path) - 1] != '/');
    DIR *dir = opendir(base_path);

    if (!dir) 
        return ss;

    //const char *regex_pattern = ".*\\.lua";
    const char *regex_pattern = ".*lua$";
    trace("scripts_loaders_init: regex_pattern %s\n", regex_pattern);
    struct small_regex *regex = regex_compile(regex_pattern);
    assert(regex);

    int scripts_num = calc_scripts_num(dir, regex);
    if (!scripts_num)
        goto _exit;

    trace("scripts_loaders_init: scripts_num %d\n", scripts_num);
    ss.scripts = calloc(scripts_num, sizeof(ss.scripts[0]));

    rewinddir(dir);
    struct dirent *entry = NULL;
    while ((entry = readdir(dir))) {
        int found = regex_matchp(regex, entry->d_name);
        if (found == -1)
            continue;

        char fname[512] = {};
        strcat(fname, base_path);
        strcat(fname, "/");
        strcat(fname, entry->d_name);
        trace("scripts_loaders_init: fname %s\n", fname);

        lua_State *lua = luaL_newstate();
        assert(lua);

        if (luaL_dofile(lua, fname) == LUA_OK) {
            trace("scripts_loaders_init: loaded %s\n", fname);
        }
        trace("scripts_loaders_init: [%s]\n", stack_dump(lua));
        table_print(lua, 1);

        trace(
            "scripts_loaders_init: top typename %s\n",
            lua_typename(lua, lua_type(lua, 1))
        );
        if (lua_istable(lua, 1)) {
            trace("scripts_loaders_init: script_fill '%s'\n", fname);
            struct Script sc = script_fill(lua);
            ss.scripts[ss.num] = sc;
            ss.scripts[ss.num].fname = strdup(fname);
            ss.num++;
        }

        lua_close(lua);
    }
    closedir(dir);
    regex_free(regex);

_exit:
    return ss;
}
*/

void scripts_loaders_shutdown(struct ScriptsStore *ss) {
    assert(ss);
    if (ss->scripts) {
        for (int i = 0; i < ss->num; ++i) {
            struct Script *sc = &ss->scripts[i];
            if (sc->description)
                free(sc->description);
            if (sc->name)
                free(sc->name);
            if (sc->fname)
                free(sc->fname);
        }
        free(ss->scripts);
        ss->scripts = NULL;
    }
    memset(ss, 0, sizeof(*ss));
}

/*
static void gui_sprites_window() {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("sprites loader", &open, flags);

    //struct Script *scripts = scripts_store.scripts;
    //for (int i = 0; i < scripts_store.num; i++) {
        //igSetNextItemOpen(true, ImGuiCond_Once);
        //if (igTreeNode_Ptr((void*)(uintptr_t)i, "%d", i)) {
            //if (scripts[i].name)
                //igText("name %s", scripts[i].name);
            //if (scripts[i].fname)
                //igText("file name %s", scripts[i].fname);
            //if (scripts[i].description)
                //igText("description %s", scripts[i].description);

            //igText(u8"привет");
    
            //igTreePop();
        //}
    //}

    igEnd();
}
*/

static void gui_render() {
    rlImGuiBegin();

    //gui_scripts_loader();
    //gui_sprites_window();
    stages_gui_window(ss);
    dotool_gui(testing_ctx);
    stage_active_gui_render(ss);

    /*
    if (input2)
        input_gui_update(input2);
        */

    bool open = false;
    igShowDemoWindow(&open);

    rlImGuiEnd();
}

static void update(void) {
    /*
    double now = GetTime();
    if (now - last_time >= 1.) {
        last_time = now;
    }
    */
    koh_fpsmeter_frame_begin();

    inotifier_update();

    BeginDrawing();
    ClearBackground(color_background_clear);

    hotkey_process(&hk_store);
    console_check_editor_mode();
    dotool_update(testing_ctx);
    stage_active_update(ss);

    // XXX: Почему не отображается?
    console_write("fps %d\n", GetFPS());

    koh_fpsmeter_draw();

    //input_gp_print_changed_buttons();
    //input_gp_print_changed_axises();
    
    Vector2 mp = Vector2Add(
        GetMousePosition(), GetMonitorPosition(GetCurrentMonitor())
    );
    //DrawText(mouse_pos_str, 0, 0, 32, RED);
    console_write("%s", Vector2_tostr(mp));

    // FIXMEL: Возможность рисовать консоль после gui_render()
    console_update();
    gui_render();

    EndDrawing();

    koh_fpsmeter_frame_end();
}

/*
struct t80_args parse_args(int argc, char **argv) {
    struct t80_args ret_args = {};
    for(int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-e")) {
            for (int rest = i + 1; rest < argc; rest++) {
                int len = strlen(ret_args.lua_string) + strlen(argv[rest]); 
                if (len < sizeof(ret_args.lua_string)) {
                    strcat(ret_args.lua_string, argv[rest]);
                }
            }
            ret_args.is_do_string = true;
        } else if (!strcmp(argv[i], "-xdotool")) {
            printf("parse_args: -xdotool\n");
            const char *next_arg = argv[i + 1];
            printf("parse_args: next argument '%s'\n", next_arg);
            ret_args.is_xdotool = true;
            strncpy(
                ret_args.xdotool_fname,
                next_arg,
                sizeof(ret_args.xdotool_fname) - 1
            );
        }
    }

    return ret_args;
}
*/

/*
static void do_script() {
    if (app_args.is_do_string && strlen(app_args.lua_string) > 0) {
        sc_dostring(app_args.lua_string);
    }
}
*/

#if !defined(PLATFORM_WEB)

void sig_handler(int sig) {
    printf("sig_handler: %d signal catched\n", sig);
    /*
    // XXX:
    if (__STDC_VERSION__ >=201710L) {
    } else
        printf("sig_handler: %s signal catched\n", strsignal(sig));
    */
    koh_backtrace_print();
    KOH_EXIT(EXIT_FAILURE);
}
#endif

/*
static void check_dotool() {
    // Переменная CAUSTIC_XDOTOOL_FNAME указывает на скрипт для xdotool
    const char *script_fname = NULL;

    script_fname = getenv("CAUSTIC_XDOTOOL_FNAME");
    if (!script_fname) 
        script_fname = getenv("KOH_XDOTOOL");
    if (!script_fname) 
        script_fname = getenv("KOH_DOTOOL");

    if (script_fname) {
        printf("check_dotool: script_fname %s\n", script_fname);
        dotool_exec_script(testing_ctx, script_fname); 
    }
    if (app_args.is_xdotool) {
        dotool_exec_script(testing_ctx, app_args.xdotool_fname); 
    }
}
*/

int main(int argc, char **argv) {
#if !defined(PLATFORM_WEB)
    signal(SIGSEGV, sig_handler);
#endif

    /*export_height2colors();*/
    /*return 0;*/

    /*
    Сделать что-то вроде 
    koh_system_init();
    koh_system_shutdown();

    typedef void (koh_SystemCallback)(void *arg);

    koh_system_add(cb_init, cb_shutdown);
    koh_system_print_order();
     */

    koh_hashers_init();
    logger_init();
    testing_ctx = dotool_new();
    //app_args = parse_args(argc, argv);

    // вызывается как можно раньше после запуска программы
    //check_dotool();

    const char *wnd_name = "magic";

    SetTraceLogLevel(LOG_WARNING);
    
#ifdef PLATFORM_WEB
    SetConfigFlags(FLAG_MSAA_4X_HINT);  // Set MSAA 4X hint before windows creation
    InitWindow(screenWidth_web, screenHeight_web, wnd_name);
    SetTraceLogLevel(LOG_ALL);
#else
    //SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_FULLSCREEN_MODE);  // Set MSAA 4X hint before windows creation
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);  // Set MSAA 4X hint before windows creation
    InitWindow(screen_width_desk, screen_height_desk, wnd_name);
    SetTraceLogLevel(LOG_ALL);
    // FIXME: Работает только на моей конфигурации, сделать опцией
    // К примеру отрабатывать только на флаг -DDEV
#ifndef LAPTOP
    SetWindowPosition(GetMonitorPosition(1).x, 0);
#endif
    dotool_setup_display(testing_ctx);
#endif

    SetExitKey(KEY_NULL);

    sc_init();
    inotifier_init();
    logger_register_functions();

    koh_fpsmeter_init();
    sc_init_script();
    // TODO: Уничтожать lua машину на каждый вызов stage_fight_load()
    // Как быть с глобальными зарегистрированными функциями?
    koh_common_init();
    //input_init();

    ss = stage_new(&(struct StageStoreSetup) {
        .stage_store_name = "stage_main",
        .l = sc_get_state(),
    });

    hotkey_init(&hk_store);

    // XXX: Требуется включение и выключение
    InitAudioDevice();
    CloseAudioDevice();

    sfx_init();
    koh_music_init();
    koh_render_init();

    struct igSetupOptions opts = {
        .dark = false,
        .font_size_pixels = 35,
        .font_path = "assets/fonts/jetbrainsmono.ttf",
        //.font_path = "assets/fonts/dejavusansmono.ttf",
        .ranges = (ImWchar[]){
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x044F, // Cyrillic
            // XXX: symbols not displayed
            // media buttons like record/play etc. Used in dotool_gui()
            0x23CF, 0x23F5, 
            0,
        },
    };
    rlImGuiSetup(&opts);

    //input2 = input_new();

    stage_add(ss, stage_shot_new(&hk_store), "shot");

    stage_init(ss);
    stage_active_set(ss, "shot");

    console_init(&hk_store, &(struct ConsoleSetup) {
        .on_enable = console_on_enable,
        .on_disable = console_on_disable,
        .udata = NULL,
        .color_text = BLACK,
        .color_cursor = BLUE,
    });
    //sc_dostring("inspect = require 'inspect'");
    console_buf_write_c(BLACK, "for help messages type \"types.help()\"");

    //net_init(&st->net);

    last_time = GetTime();

    //stage_print(ss);
    trace("main: active stage: %s\n", stage_active_name_get(ss));

    //scripts_store = scripts_loaders_init();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update, 60, 1);
#else
    //do_script();
    dotool_send_signal(testing_ctx);
    // TODO: Движение танка и вращение турелей сделать с учетом GetFrameTime()
    SetTargetFPS(120); 
    while (!WindowShouldClose() && !koh_cmn()->quit) {
        update();
    }
#endif

    koh_music_shutdown();       // добавить в систему инициализации
    koh_fpsmeter_shutdown(); // добавить в систему инициализации
    koh_render_shutdown();// добавить в систему инициализации
    console_shutdown();// добавить в систему инициализации
    hotkey_shutdown(&hk_store);// добавить в систему инициализации, void*
    stage_shutdown(ss);// добавить в систему инициализации
    if (ss) {
        stage_free(ss);
        ss = NULL;
    }
    //input_shutdown();// добавить в систему инициализации
    //input_free(input2);
    koh_common_shutdown();// добавить в систему инициализации
    sc_shutdown();// добавить в систему инициализации
    sfx_shutdown();// добавить в систему инициализации
    inotifier_shutdown();// добавить в систему инициализации
    //t80_objects_types_shutdown();// добавить в систему инициализации
    //scripts_loaders_shutdown(&scripts_store);
    rlImGuiShutdown();// добавить в систему инициализации
    CloseWindow();// добавить в систему инициализации

    dotool_free(testing_ctx);
    logger_shutdown();

    return EXIT_SUCCESS;
}
