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

//static struct dotool_ctx *testing_ctx = NULL;

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

static void gui_render() {
    rlImGuiBegin();

    stages_gui_window(ss);
    stage_active_gui_render(ss);

    bool open = false;
    igShowDemoWindow(&open);

    rlImGuiEnd();
}

static void update(void) {
    koh_fpsmeter_frame_begin();

    inotifier_update();

    BeginDrawing();
    ClearBackground(color_background_clear);

    hotkey_process(&hk_store);
    console_check_editor_mode();
    stage_active_update(ss);

    // XXX: Почему не отображается?
    console_write("fps %d\n", GetFPS());

    koh_fpsmeter_draw();

    Vector2 mp = Vector2Add(
        GetMousePosition(), GetMonitorPosition(GetCurrentMonitor())
    );
    console_write("%s", Vector2_tostr(mp));

    console_update();
    gui_render();

    EndDrawing();

    koh_fpsmeter_frame_end();
}

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

int main(int argc, char **argv) {
#if !defined(PLATFORM_WEB)
    signal(SIGSEGV, sig_handler);
#endif

    /*
TODO: Сделать что-то вроде 
    koh_system_init();
    koh_system_shutdown();

    typedef void (koh_SystemCallback)(void *arg);

    koh_system_add(cb_init, cb_shutdown);
    koh_system_print_order();
     */
    SetTraceLogCallback(koh_log_custom);

    koh_hashers_init();
    logger_init();
    //testing_ctx = dotool_new();

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
    //dotool_setup_display(testing_ctx);
#endif

    SetExitKey(KEY_NULL);

    // TODO: Убрать sc_init()
    sc_init();
    inotifier_init();
    logger_register_functions();

    koh_fpsmeter_init();
    sc_init_script();
    koh_common_init();

    ss = stage_new(&(struct StageStoreSetup) {
        .stage_store_name = "main",
        .l = sc_get_state(), // TODO: Зачем передавать Lua состояние?
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

    stage_add(ss, stage_shot_new(&hk_store), "shot");

    stage_init(ss);
    stage_active_set(ss, "shot");

    console_init(&hk_store, &(struct ConsoleSetup) {
        .on_enable = console_on_enable,
        .on_disable = console_on_disable,
        .udata = NULL,
        .color_text = BLACK,
        .color_cursor = BLUE,
        .fnt_size = 32,
        .fnt_path = "assets/fonts/jetbrainsmono.ttf",
    });

    last_time = GetTime();

    trace("main: active stage: %s\n", stage_active_name_get(ss));

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update, 60, 1);
#else
    //dotool_send_signal(testing_ctx);

    SetTargetFPS(120); 
    while (!WindowShouldClose() && !koh_cmn()->quit) {
        update();
    }

#endif

    stage_shutdown(ss);// добавить в систему инициализации
    koh_music_shutdown();       // добавить в систему инициализации
    koh_fpsmeter_shutdown(); // добавить в систему инициализации
    koh_render_shutdown();// добавить в систему инициализации
    console_shutdown();// добавить в систему инициализации
    hotkey_shutdown(&hk_store);// добавить в систему инициализации, void*
    if (ss) {
        stage_free(ss);
        ss = NULL;
    }
    koh_common_shutdown();// добавить в систему инициализации
    sc_shutdown();// добавить в систему инициализации
    sfx_shutdown();// добавить в систему инициализации
    inotifier_shutdown();// добавить в систему инициализации
    rlImGuiShutdown();// добавить в систему инициализации
    CloseWindow();// добавить в систему инициализации

    //dotool_free(testing_ctx);
    logger_shutdown();

    return EXIT_SUCCESS;
}
