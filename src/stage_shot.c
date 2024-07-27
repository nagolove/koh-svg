// vim: set colorcolumn=85
// vim: fdm=marker
#include "stage_shot.h"

#define NANOSVG_IMPLEMENTATION

// includes {{{
#include "koh_cleanup.h"
#include <emmintrin.h>  // Заголовочный файл для SSE2
#include <immintrin.h>  // AVX
#include <nmmintrin.h>  // popcnt
#include "koh_lua_tools.h"
#include "svg.h"
#include "koh_input.h"
#include "cimgui.h"
#include "koh_b2.h"
#include "koh_b2_world.h"
#include "koh_common.h"
#include "koh_common.h"
#include "koh_components.h"
#include "koh_destral_ecs.h"
#include "koh_hotkey.h"
#include "koh_stages.h"
#include "nanosvg.h"
#include "raylib.h"
#include "rlgl.h"
#include "koh_b2_world.h"
#include <assert.h>
#include <stdlib.h>
#include "koh_timerman.h"
#include "koh_reasings.h"
#include "koh_table.h"
#include "koh_routine.h"
#include "koh_inotifier.h"
#include "koh_cammode.h"

#ifndef KOH_NO_RLWR
#include "rlwr.h"
#endif

// }}}

#define cleanup(func) __attribute__((cleanup(func)))

typedef struct Sensor {
    char name[128];
} Sensor;

static const uint64_t stage_shot_uniq_id = 4294967311;

// Тэг тела, что оно является игровым кружком
const de_cp_type cp_type_circle = {
    // TODO: Добавит проверку при регистрации компонента на повторение cp_id
    .cp_id = 0, 

    .cp_sizeof = sizeof(char),
    .name = "circle",
    .description = "circle tag",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 20000,
    .on_destroy = NULL,
    .on_emplace = NULL,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

// Тэг тела, что оно является сенсором который обрабатывается скриптом
const de_cp_type cp_type_sensor = {
    .cp_id = 0, 
    .cp_sizeof = sizeof(Sensor),
    .name = "sensor",
    .description = "script controlled sensor",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 20000,
    .on_destroy = NULL,
    .on_emplace = NULL,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

typedef struct Stage_shot {
    Stage             parent;
    uint64_t          uniq_id;
    // {{{

    /* 
    Уровень загружен - 
        ресурсы raylib 
        мир box2d
        nanosvg изображение
        de_ecs состояние
    */
    bool               loaded; 

    InputKbMouseDrawer *kb_drawer;
    InputGamepadDrawer *gp_drawer;

    // "sensor_name" -> int (lua reference for function() end)
    //HTable            *sensor_name2func;

    float             // зазор отсечения для области камеры
                      gap_radius,
                      // упругость шариков 0..1 
                      circle_restinition, 
                      // как быстро поворачивать уровень(изменять направление
                      // вектора гравитации)
                      rot_delta_angle,
                      // длина вектора гравитации
                      grav_len,
                      // толщина линии рисования уровня
                      line_thick,

                      circle_radius;

    bool              // выполнять запрос физического движка для рисования
                      world_query_AABB, 
                      // рисовать сенсоры в запросе физического движка
                      draw_sensors,  
                      // проверочный вывод в консоль
                      verbose,       
                      // рисовать область камеры
                      cam_draw_rect; 
    int               borders_gap_px;

#ifdef KOH_RLWR
    struct rlwr_t *rlwr; // Луа-состотяние с загруженными биндингами raylib'а
#endif
    lua_State         *l;
    char              lua_fname[256];

    Camera2D          cam;
    NSVGimage         *nsvg_img;
    Rectangle         svg_bound;
    WorldCtx          wctx;
    xorshift32_state  xrng;
    b2Vec2            gravity;
    de_ecs            *r;
    Texture2D         tex_example;
    Resource          res_list;
    de_entity         borders[4];
    uint32_t          borders_num;

    FilesSearchResult fsr_svg;
    FilesSearchSetup  fss_svg;
    bool              *selected_svg;

    // b2ShapeId при соприкосновении с которыми тела удаляются
    koh_Set           *sensors_killer;

    struct TimerMan   *tm;
    // }}}
} Stage_shot;

/*
Заменить несколь Луа модулей в koh на один файл koh_lua.
// {{{
Префикс функций в них - L_
L_stack_dump()
L_table_dump()
L_rect_get()
Обязательная возможность работы с несколькими экземплярами виртуальной машины.
Передавать явный указатель на состояние машины.

Через L_set_registry_ptr()/L_get_registry_ptr() получать доступ к сценам и
внутренним данным программы без явной передачи указателя на контекст. Данный 
способ не является автоматическим, работает на лёгких данных пользователя.
// }}}
 */
static void L_call(lua_State *l, const char *func_name);
static void load(Stage_shot *st, const char *fname);
static void unload(Stage_shot *st);

// {{{ basic callbacks
static void stage_shot_init(struct Stage_shot *st);
static void stage_shot_enter(struct Stage_shot *st);
static void stage_shot_leave(struct Stage_shot *st);
static void stage_shot_gui(struct Stage_shot *st);
static void stage_shot_update(struct Stage_shot *st);
static void stage_shot_draw(Stage_shot *st);
static void stage_shot_shutdown(struct Stage_shot *st);

Stage *stage_shot_new(HotkeyStorage *hk_store) {
    assert(hk_store);
    Stage_shot *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_shot_init;
    st->parent.enter = (Stage_callback)stage_shot_enter;
    st->parent.leave = (Stage_callback)stage_shot_leave;
    st->parent.gui = (Stage_callback)stage_shot_gui;
    st->parent.update = (Stage_callback)stage_shot_update;
    st->parent.draw = (Stage_callback)stage_shot_draw;
    st->parent.shutdown = (Stage_callback)stage_shot_shutdown;

    st->uniq_id = stage_shot_uniq_id;

    return (Stage*)st;
}
// }}}

/*
static void input_init(Stage_shot *st) {
}

static void input_shutdown(Stage_shot *st) {
}
*/

//static void input_update(Stage_shot *st) {
//}

void inotify_script_changed(const char *fname, void *udata) {
    trace("inotify_script_changed: fname '%s'\n", fname);
    inotifier_watch(fname, inotify_script_changed, udata);

    // XXX: Сперва проверить корректность загрузки скрипта, 
    // только потом выгружать сцену
    Stage_shot *st = udata;
    load(st, fname);
}

static bool query_world_draw(b2ShapeId shape_id, void* context) {
    //trace("query_world_draw:\n");
    struct Stage_shot *st = context;

    if (!st->draw_sensors) {
        if (b2Shape_IsSensor(shape_id)) {
            //trace("query_world_draw: shape is sensor\n");
            return true;
        }
    }

    switch (b2Shape_GetType(shape_id)) {
        case b2_capsuleShape: 
            trace("query_world_draw: b2_capsuleShape is not implemented\n");
            break;
        case b2_circleShape: 
            world_shape_render_circle(shape_id, &st->wctx, st->r);
            break;
        // Как сюда передавать что-то, какие-то дополнительные аргументы?
        case b2_polygonShape:
            world_shape_render_poly(shape_id, &st->wctx, st->r);
            break;
        case b2_segmentShape: 
            world_shape_render_segment(shape_id, &st->wctx, st->r);
            //trace("query_world_draw: b2_segmentShape is not implemented\n");
            break;
        default: 
            trace("query_world_draw: default branch, unknown shape type\n");
            break;
    }

    return true;
}

struct CreatorCtx {
    b2Vec2      last;
    bool        last_used;
    Stage_shot  *st;
};

static de_entity segment_create(
    de_ecs *r, WorldCtx *wctx, b2Vec2 p1, b2Vec2 p2
) {
    b2Segment seg = { p1, p2, };
    float len_sq = b2Length(b2Sub(seg.point1, seg.point2));

    if (len_sq < FLT_EPSILON) {
        trace("segment_create: too short segment\n");
        return de_null;
    }

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_staticBody;

    de_entity e = de_create(r);


    bool prev_de_ecs_verbose = de_ecs_verbose;
    de_ecs_verbose = true;
    b2BodyId *bid = de_emplace(r, e, cp_type_body);
    de_ecs_verbose = prev_de_ecs_verbose;

    *bid = b2CreateBody(wctx->world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    b2CreateSegmentShape(*bid, &sd, &seg);

    struct ShapeRenderOpts *r_opts = de_emplace(
        r, e, cp_type_shape_render_opts
    );

    r_opts->color = MAROON;
    r_opts->tex = NULL;
    r_opts->thick = 5.;

    /*trace("segment_create: created\n");*/

    return e;
}

static void body_creator_next_path(void *udata) {
    trace("body_creator_next_path:\n");
    struct CreatorCtx *ctx = udata;
    ctx->last_used = false;

    //WorldCtx *wctx = &ctx->st->wctx;
    //Stage_shot *st = ctx->st;

    //segment_create(st->r, wctx, ctx->last, (b2Vec2) { x, y });
}

static void body_creator(float x, float y, void *udata) {
    struct CreatorCtx *ctx = udata;
    WorldCtx *wctx = &ctx->st->wctx;
    Stage_shot *st = ctx->st;
    //trace("body_creator:\n");
    if (ctx->last_used) {
        segment_create(st->r, wctx, ctx->last, (b2Vec2) { x, y });
    } else {
        ctx->last_used = true;
    }

    ctx->last.x = x;
    ctx->last.y = y;
}
//*/

static void svg_begin_search(FilesSearchResult *fsr) {
    Stage_shot *st = fsr->udata;
    assert(st);

    if (st->selected_svg) {
        free(st->selected_svg);
        st->selected_svg = NULL;
    }
}

static void svg_end_search(FilesSearchResult *fsr) {
    Stage_shot *st = fsr->udata;
    assert(st);

    st->selected_svg = calloc(fsr->num, sizeof(st->selected_svg[0]));
    if (fsr->num) {
        st->selected_svg[0] = true;
    }
}

static void svg_shutdown_selected(FilesSearchResult *fsr) {
    Stage_shot *st = fsr->udata;
    assert(st);

    if (st->selected_svg) {
        free(st->selected_svg);
        st->selected_svg = NULL;
    }
}

static de_entity circle_create(de_ecs *r, WorldCtx *wctx, Rectangle rect) {
    assert(r);

    de_entity e = de_create(r);

    // Выдает память, но неправильную
    b2BodyId *bid = de_emplace(r, e, cp_type_body);

    bool prev_de_ecs_verbose = de_ecs_verbose;
    de_ecs_verbose = true;
    char *tag_circle = de_emplace(r, e, cp_type_circle);
    de_ecs_verbose = prev_de_ecs_verbose;

    /*de_emplace(r, e, cp_type_circle);*/
    *tag_circle = 1;
    ShapeRenderOpts *r_opts = de_emplace(r, e, cp_type_shape_render_opts);

    r_opts->color = GREEN;
    r_opts->tex = NULL;
    r_opts->thick = 5.;

    assert(bid);

    b2BodyDef bd = b2DefaultBodyDef();
    bd.userData = (void*)(uintptr_t)e;
    //bd.type = b2_staticBody;
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2) { rect.x, rect.y };

    *bid = b2CreateBody(wctx->world, &bd);
    b2Body_EnableSleep(*bid, false);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.userData = (void*)(uintptr_t)e;
    //b2CreateSegmentShape(bid, &sd, &seg);
    /*b2CreatePolygonShape(*bid, &sd, &poly);*/
    const b2Circle circle = {
        .center = {
            rect.x, 
            rect.y, 
        },
        .radius = rect.width,
    };
    b2ShapeId sid = b2CreateCircleShape(*bid, &sd, &circle);
    b2Shape_SetRestitution(sid, 0.15);

    /*trace("circle_create: created\n");*/
    return e;
}

/*
static de_entity box_create(de_ecs *r, WorldCtx *wctx, Rectangle rect) {
    assert(r);

    de_entity e = de_create(r);

    b2BodyId *bid = de_emplace(r, e, cp_type_body);
    ShapeRenderOpts *r_opts = de_emplace(r, e, cp_type_shape_render_opts);

    r_opts->color = GREEN;
    r_opts->tex = NULL;
    r_opts->thick = 5.;

    assert(bid);
    b2Polygon poly = b2MakeBox(rect.width, rect.height);

    b2BodyDef bd = b2DefaultBodyDef();
    //bd.type = b2_staticBody;
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2) { rect.x, rect.y };

    *bid = b2CreateBody(wctx->world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    //b2CreateSegmentShape(bid, &sd, &seg);
    b2CreatePolygonShape(*bid, &sd, &poly);

    //trace("box_create: created\n");

    return e;
}
*/

/*
static void parse_segment(Vector2 p1, Vector2 p2, void *udata) {
    Stage_shot *st = udata;

    trace("parse_segment:\n");
    segment_create(
        st->r, &st->wctx, Vector2_to_Vec2(p1), Vector2_to_Vec2(p1)
    );
}
*/
// TODO: Нужно вызвать таймер раз в n секунд. Что делать?
static bool tmr_spawn_circle_update(struct Timer *tmr) {
    //trace("tmr_spawn_circle:\n");
    //Stage_shot *st = tmr->data;
    //const float radius = 20.;
    //double now = GetTime();
    //double delta = (now - tmr->last_now);
    //trace("tmr_spawn_circle: delta %f\n", delta);
    //circle_create(st->r, &st->wctx, (Rectangle) { 50., 0, radius, 10., });
    return false;
}

// TODO: Как перезапустить таймер?
static void tmr_spawn_circle_stop(struct Timer *tmr) {
    //Stage_shot *st = tmr->data;
    //const float radius = 20.;
    //double now = GetTime();
    //double delta = (now - tmr->last_now);
    //trace("tmr_spawn_circle: delta %f\n", delta);
    //circle_create(st->r, &st->wctx, (Rectangle) { 50., 0, radius, 10., });
}

static void unload(Stage_shot *st) {
    if (!st->loaded)
        return;

    inotifier_watch_remove(st->lua_fname);

    if (st->l) {
#ifdef KOH_RLWR
        rlwr_free(st->rlwr);
        st->rlwr = NULL;
#else
        lua_close(st->l);
#endif
        st->l = NULL;
    }

    /*
    if (st->sensor_name2func) {
        htable_free(st->sensor_name2func);
        st->sensor_name2func = NULL;
    }
    */

    if (st->tm) {
        timerman_free(st->tm);
        st->tm = NULL;
    }

    if (st->nsvg_img) {
        nsvgDelete(st->nsvg_img);
        st->nsvg_img = NULL;
    }
    
    if (st->r) {
        de_ecs_destroy(st->r);
        st->r = NULL;
    }

    world_shutdown(&st->wctx);
    res_unload_all(&st->res_list);

    if (st->sensors_killer) {
        set_free(st->sensors_killer);
        st->sensors_killer = NULL;
    }

    //input_gp_free(st->gp_drawer);

    st->loaded = false;
}

static void set_gravity(Stage_shot *st, float deg_angle) {
    const float rad_angle = deg_angle * (M_PI / 180.);

    b2Vec2 gravity = {};
    gravity.x = sin(rad_angle) * st->grav_len;
    gravity.y = cos(rad_angle) * st->grav_len;

    /*
    trace(
        "calc_gravity: deg_angle %f, rad_angle %f, gravity %s\n", 
        deg_angle, rad_angle, b2Vec2_to_str(gravity)
    );
    */

    b2World_SetGravity(st->wctx.world, gravity);
}

static int search_file_printer(void *udata, const char *fmt, ...) {
    char buf[256] = {};

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    strcat(udata, buf);

    return ret;
}

static void *L_get_registry_ptr(lua_State *l, const char *name) {
    assert(l);
    assert(name);
    lua_pushstring(l, name);
    lua_gettable(l, LUA_REGISTRYINDEX);

    if (!lua_islightuserdata(l, -1)) {
        trace("L_get_registry_ptr: to a islightuserdata\n");
        exit(EXIT_FAILURE);
    }

    void *ptr = lua_touserdata(l, -1);
    lua_pop(l, 1);

    assert(ptr);

    return ptr;
}

// Вызвать Луа функцию обратного вызова по имени сенсора
/*
static void L_sensor_call(lua_State *l, const char *sensor_name) {
    trace("L_sensor_call: sensor_name '%s'\n", sensor_name);

    Stage_shot *st = L_get_registry_ptr(l, "ptr_stage_shot");
    assert(st);

    trace("L_sensor_call: 0 [%s]\n", stack_dump(l));

    char full_sensor_name[128] = {};
    sprintf(full_sensor_name, "sensor_%s", sensor_name);
    lua_pushstring(l, full_sensor_name);
    lua_gettable(l, LUA_REGISTRYINDEX);
    assert(lua_isfunction(l, -1));

    trace("L_sensor_call: 1 [%s]\n", stack_dump(l));

    //lua_pushvalue(l, 3);  // Поместить функцию на вершину стека
    //lua_pushstring(l, str);  // Поместить аргумент на вершину стека
    if (lua_pcall(l, 1, 0, 0) != LUA_OK) {
        // Обработка ошибки вызова
        trace(
            "L_sensor_call: error calling callback function: %s",
            sensor_name
        );
        exit(EXIT_FAILURE);
    }

    trace("L_sensor_call: 2 [%s]\n", stack_dump(l));

    lua_pop(l, 1);

    //lua_pushstring(l, full_sensor_name);
}
*/

static int l_gravity_set(lua_State *l) {
    assert(lua_islightuserdata(l, 1));

    Vector2 *gravity = luaL_testudata(l, 1, "Vector2");

    if (gravity) {
        trace("l_setgravity: gravity %s\n", Vector2_tostr(*gravity));

        Stage_shot *st = L_get_registry_ptr(l, "ptr_stage_shot");
        assert(st);

        b2World_SetGravity(st->wctx.world, Vector2_to_Vec2(*gravity));
    } else {
        trace("l_setgravity: gravity is NULL, type mismatch\n");
    }

    return 0;
}

static de_entity sensor_add_circle(
    de_ecs *r, float radius, b2Vec2 pos
) {
    trace("sensor_add_circle:\n");
    return de_null;
}

static int l_sensor_add(lua_State *l) {
    printf("l_sensor_add: 1 [%s]\n", stack_dump(l));

    // Проверка и получение строки (первый аргумент)
    const char *sensor_name = luaL_checkstring(l, 1);

    // Проверка и получение таблицы (второй аргумент)
    luaL_checktype(l, 2, LUA_TTABLE);
    // Можно использовать таблицу по индексу 2

    // Проверка и получение функции обратного вызова (третий аргумент)
    luaL_checktype(l, 3, LUA_TFUNCTION);

    trace(
        "l_sensor_add: 1 sensor_name '%s', [%s]\n",
        sensor_name, stack_dump(l)
    );

    char full_sensor_name[128] = {};
    sprintf(full_sensor_name, "sensor_%s", sensor_name);
    lua_pushstring(l, full_sensor_name);
    lua_pushvalue(l, 3);
    lua_settable(l, LUA_REGISTRYINDEX);

    trace(
        "l_sensor_add: 2 sensor_name '%s', [%s]\n",
        sensor_name, stack_dump(l)
    );

    Stage_shot *st = L_get_registry_ptr(l, "ptr_stage_shot");
    assert(st);

    /*
// Освобождение предыдущей ссылки на функцию, если она существует
    if (callback_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
    }

    // Сохранение новой ссылки на функцию
    lua_pushvalue(L, 1);  // Копирование функции на вершину стека
    callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);  // Сохранение ссылки
    */

    /*

    // Вызов функции обратного вызова с одним аргументом (строкой)
    lua_pushvalue(l, 3);  // Поместить функцию на вершину стека
    lua_pushstring(l, str);  // Поместить строку на вершину стека
    if (lua_pcall(l, 1, 0, 0) != LUA_OK) {
        // Обработка ошибки вызова
        luaL_error(l, "Error calling callback function: %s", lua_tostring(l, -1));
    }

    */

    printf("l_sensor_add: 2 [%s]\n", stack_dump(l));

    float radius = 0.;
    b2Vec2 pos = {};
    sensor_add_circle(st->r, radius, pos);

    // Возвращаемое значение функции (в данном примере ничего не возвращаем)
    return 0;
}

const static luaL_Reg lua_funcs[] = {
    { "sensor_add", l_sensor_add, },
    { "gravity_set", l_gravity_set, },
    { NULL, NULL},
};

static void L_set_registry_ptr(lua_State *l, const char *name, void *ptr) {

    trace(
        "L_set_registry_ptr: 1 name '%s', ptr %p, [%s]\n",
        name, ptr, stack_dump(l)
    );

    lua_pushstring(l, name);
    lua_pushlightuserdata(l, ptr);
    lua_settable(l, LUA_REGISTRYINDEX);

    trace(
        "L_set_registry_ptr: 2 name '%s', ptr %p, [%s]\n",
        name, ptr, stack_dump(l)
    );

}

static bool L_loadfile(lua_State *l, const char *lua_fname) {
    assert(l);
    assert(lua_fname);

    //trace("L_loadfile: |'%s|\n", koh_backtrace_get());

    int ret = luaL_loadfile(l, lua_fname);
    if (ret != LUA_OK)
        trace(
                "L_loadfile: file '%s' return code %d with %s\n",
                lua_fname, ret, lua_tostring(l, -1)
             );
    else {
        ret = lua_pcall(l, 0, LUA_MULTRET, 0);
        if (ret != LUA_OK) {
            trace(
                    "L_loadfile: pcall file '%s' return code %d with %s\n",
                    lua_fname, ret, lua_tostring(l, -1)
                 );
        } else
            trace("L_loadfile: script loaded\n");
    }

    return ret == LUA_OK;
}

/*
static bool L_checksyntax(const char *fname) {
    // {{{
    lua_State *l_tmp = NULL;

#ifdef KOH_RLWR
    rlwr_t * rlwr = rlwr_new();
    l_tmp = rlwr_state(rlwr);
#else
    l_tmp = luaL_newstate();
    luaL_openlibs(l_tmp);
#endif

    bool ok = L_loadfile(l_tmp, fname);
    
#ifdef KOH_RLWR
    rlwr_free(rlwr);
#else
    lua_close(l_tmp);
#endif

    return ok;
    // }}}
}
*/

// fname содержит имя файла уровня без расширения.
static void load(Stage_shot *st, const char *fname) {
    // Если файл скрипта отсутствует или содержит ошибки, то уровень 
    // не выгружать
    // XXX: Не работает
    //if (!L_checksyntax(fname)) {
        //return;
    //}

    if (st->loaded) {
        unload(st);
        st->loaded = false;
    }

    assert(st);
    assert(fname);

    st->grav_len = 9.8; 
    st->circle_radius = 20.;
    st->gap_radius = 0.;
    koh_cam_reset(&st->cam);

    st->r = de_ecs_make();

    char svg_fname[128] = {}, lua_fname[128];
    sprintf(svg_fname, "%s.svg", fname);
    sprintf(lua_fname, "%s.lua", fname);

    strncpy(st->lua_fname, lua_fname, sizeof(st->lua_fname));

    inotifier_watch(lua_fname, inotify_script_changed, st);

    trace("load: svg_fname '%s'\n", svg_fname);

    // {{{ CONSTANTS
    st->rot_delta_angle = 1.;
    st->circle_restinition = 0.;
    // }}}

    // {{{ Lua state 
    lua_State **l = &st->l;

#ifdef KOH_RLWR
    trace("load: rlwr created\n");
    st->rlwr = rlwr_new();
    *l = rlwr_state(st->rlwr);
#else
    *l = luaL_newstate();
    luaL_openlibs(*l);
#endif

    trace("load: new [%s]\n", stack_dump(*l));

    L_set_registry_ptr(*l, "ptr_stage_shot", st);

    trace("load: set stage ptr [%s]\n", stack_dump(*l));

    luaL_newlib(*l, lua_funcs); 
    lua_setglobal(*l, "mgc");

    trace("load: setup 'mgc' table [%s]\n", stack_dump(*l));

    L_loadfile(*l, lua_fname);

    trace("load: loadfile done [%s]\n", stack_dump(*l));

    //if (!ret) {
        //trace("load: script not loaded, '%s'\n", lua_tostring(st->l, -1));
    //}
    // }}}

    st->kb_drawer = input_kb_new(&(struct InputKbMouseDrawerSetup) {
        .btn_width = 70.,
    });
    st->gp_drawer = input_gp_new();

    st->tm = timerman_new(512, "shot_timers");

    st->sensors_killer = set_new(NULL);

    // {{{ File search setup
    st->fss_svg.deep = 2;
    st->fss_svg.path = "assets";
    st->fss_svg.regex_pattern = ".*\\.svg$";
    st->fss_svg.on_search_begin = svg_begin_search;
    st->fss_svg.on_search_end = svg_end_search;
    st->fss_svg.on_shutdown = svg_shutdown_selected;
    st->fss_svg.udata = st;

    koh_search_files_shutdown(&st->fsr_svg);
    st->fsr_svg = koh_search_files(&st->fss_svg);
    // }}}

    //koh_search_files_print(&st->fsr_svg);
    char buf[1024 * 5] = {};
    koh_search_files_print3(&st->fsr_svg, search_file_printer, buf);

    //exit(1);

    Resource *rl = &st->res_list;
    //st->tex_example = res_tex_load(rl, "assets/magic_circle_01.png");
    st->tex_example = res_tex_load(rl, svg_fname);

    // Как de_ecs могла работать без зарегистрированных типов?
    bool prev_de_ecs_verbose = de_ecs_verbose;
    de_ecs_verbose = true;

    // HACK:Сделать так, что-бы работало без начальной установки 
    // большой вместимости
    cp_type_body.initial_cap = 20000;
    cp_type_shape_render_opts.initial_cap = 20000;

    // Попробуй закомметировать следущую строчку.
    koh_cp_types_register(st->r);

    // Побочное поведение - следующий код регистрации должен быть до 
    // создания сущностей.
    de_ecs_register(st->r, cp_type_circle);
    de_ecs_register(st->r, cp_type_sensor);
    de_ecs_verbose = prev_de_ecs_verbose;

    st->nsvg_img = nsvgParseFromFile(svg_fname, "px", 96.0f);
    assert(st->nsvg_img);

    st->xrng = xorshift32_init();

    b2WorldDef wd = b2DefaultWorldDef();
    wd.enableContinous = true;
    wd.enableSleep = true;
    wd.gravity = st->gravity;

    world_init(&(struct WorldCtxSetup) {
        .xrng = &st->xrng,
        .width = 0,
        .height = 0,
        .wd = &wd,
    }, &st->wctx);

    set_gravity(st, st->cam.rotation);

    svg_parse(st->nsvg_img, body_creator, body_creator_next_path,
        &(struct CreatorCtx) {
            .last_used = false,
            .st = st,
    });
    
    trace(
        "load: svg size %fx%f\n",
        st->nsvg_img->width,
        st->nsvg_img->height
    );
    st->svg_bound.width = st->nsvg_img->width;
    st->svg_bound.height = st->nsvg_img->height;

    // TODO: Почему-то не отображается
    segment_create(
        st->r, &st->wctx,
        (b2Vec2) { 0., st->nsvg_img->height },
        (b2Vec2) { GetScreenWidth(), st->nsvg_img->height, }
    );


    /*
    //float y = -200.;
    box_create(st->r, &st->wctx, (Rectangle) { -50., y, 40., 10., });
    box_create(st->r, &st->wctx, (Rectangle) { 250., y, 40., 10., });
    box_create(st->r, &st->wctx, (Rectangle) { 450., y, 40., 10., });
    box_create(st->r, &st->wctx, (Rectangle) { 650., y, 40., 10., });
    */

    timerman_add(st->tm, (struct TimerDef) {
        .duration = 1.5,
        .sz = 0,
        .udata = st,
        .on_update = tmr_spawn_circle_update,
        .on_stop = tmr_spawn_circle_stop,
    });

    /*
    Camera2D *cam = &st->cam;
    cam->target.x = st->nsvg_img->width / 2.;
    cam->target.y = st->nsvg_img->height / 2.;
    */

    //st->sensor_name2func = htable_new(NULL);

    L_call(st->l, "load");

    st->loaded = true;
}

static void stage_shot_init(struct Stage_shot *st) {
    st->cam_draw_rect = false;
    st->world_query_AABB = true;
    st->draw_sensors = false,
    st->verbose = false;
    st->borders_gap_px = 0;
    st->line_thick = 10.;

    //load(st, "assets/magic_level_05");
    //load(st, "assets/magic_level_04");
    load(st, "assets/magic_level_03");
    //load(st, "assets/magic_level_03");
}

// TODO: Сделать поворот уровня(изменение вектора гравитации) с инерцией.
static void rotate(Stage_shot *st, const float dangle) {
    // {{{

    /*
    {{{
    How to use:
    The four inputs t,b,c,d are defined as follows:
    t = current time (in any unit measure, but same unit as duration)
    b = starting value to interpolate
    c = the total change in value of b that needs to occur
    d = total time it should take to complete (duration)

    Example:

    int currentTime = 0;
    int duration = 100;
    float startPositionX = 0.0f;
    float finalPositionX = 30.0f;
    float currentPositionX = startPositionX;

    while (currentPositionX < finalPositionX)
    {
        currentPositionX = EaseSineIn(currentTime, startPositionX, finalPositionX - startPositionX, duration);
        currentTime++;
    }
    }}}
 */

    /*
    static bool started = false;
    static double time;
    static double start_angle, final_angle, current_angle;
    static double duration = 2.;

    if (!started) {
        time = GetTime();
        started = true;
        start_angle = 0.;
        final_angle = fabs(dangle);
        current_angle = start_angle;
        trace(
            "rotate: started, start_angle %f,"
            "final_angle %f, current_angle %f\n",
            start_angle, final_angle, current_angle
        );
    }

    if (current_angle < final_angle) {
        double now = GetTime();
        current_angle = EaseExpoIn(
            now - time, start_angle, final_angle - start_angle, duration
        );
        time = now;
    } else {
        trace("rotate: finished\n");
        started = false;
    }

    trace(
        "rotate: current_angle %f, final_angle %f\n",
        current_angle, final_angle
    );

    */

    Camera2D *cam = &st->cam;
    // dangle должен проходить от 0 до dangle
    cam->rotation += dangle;

    set_gravity(st, cam->rotation);
    // }}}
}

// DONE:
static void L_call(lua_State *l, const char *func_name) {
    assert(l);
    //trace("L_call: 1 [%s]\n", stack_dump(l));
    lua_getglobal(l, func_name);
    //trace("L_call: 2 [%s]\n", stack_dump(l));
    if (lua_pcall(l, 0, 0, 0) != LUA_OK)  {
        trace(
                "stage_shot_update: call '%s' was failed with '%s'\n", 
                func_name, lua_tostring(l, -1)
             );
        lua_pop(l, 1); // скидываю сообщение об ошибке со стека
        exit(EXIT_FAILURE);
    }
    //trace("L_call: 3 [%s]\n", stack_dump(l));
}

static void stage_shot_update(struct Stage_shot *st) {
    /*
    de_ecs *r = st->r;
    b2SensorEvents sensor_events = b2World_GetSensorEvents(st->wctx.world);
    for (int i = 0; i < sensor_events.beginCount; i++) {
        b2SensorBeginTouchEvent event = sensor_events.beginEvents[i];

        // XXX: Или лучше сделать через проверку сущности прикрепленной к телу
        // на тип добавленного к сущности компонента?
        // Преимущества - унификация
        //size_t sz = sizeof(event.sensorShapeId);
        //if (set_exist(st->sensors_killer, &event.sensorShapeId, sz)) {
            //trace("stage_shot_update: sensors killer found\n");
        //}

        de_entity e = (intptr_t)b2Shape_GetUserData(event.sensorShapeId);

        Sensor *sensor = de_try_get(r, e, cp_type_sensor);
        if (sensor) {
            L_sensor_call(st->l, sensor->name);
        }
    }
        */

    timerman_update(st->tm);
    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
            .mouse_btn = MOUSE_BUTTON_RIGHT,
            .cam = &st->cam
    });

    // TODO: Сделать ограничие масштаба для избежания следущей ошибки:
    // camera2aabb: Assertion `b2AABB_IsValid(aabb)' failed.
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        //.mouse_btn = MOUSE_BUTTON_LEFT,
        .cam = &st->cam,
        .dscale_value = 0.05,
        .modifier_key_down = KEY_LEFT_SHIFT,
    });

    world_step(&st->wctx);

    L_call(st->l, "update");

    if (IsKeyPressed(KEY_C)) {
        float y = -200.;
        Rectangle rect = { 50., y, st->circle_radius, 10., };
        circle_create(st->r, &st->wctx, rect);
        //circle_create(st->r, &st->wctx, (Rectangle) { 250., y, radius, 10., });
        //circle_create(st->r, &st->wctx, (Rectangle) { 450., y, radius, 10., });
        //circle_create(st->r, &st->wctx, (Rectangle) { 650., y, radius, 10., });
    }

    const float dzoom = 0.001;
    if (IsKeyDown(KEY_Z)) {
        st->cam.zoom -= dzoom;
    }
    if (IsKeyDown(KEY_X)) {
        st->cam.zoom += dzoom;
    }

    // left
    if (IsKeyDown(KEY_Q)) {
        rotate(st, st->rot_delta_angle);
    } else if (IsKeyDown(KEY_W)) {
        rotate(st, -st->rot_delta_angle);
    }
}

static void box2d_actions_gui(struct Stage_shot *st) {
    // {{{
    assert(st);
    struct WorldCtx *wctx = &st->wctx;
    assert(wctx);
    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("box2d actions", &wnd_open, wnd_flags);

    static int num = 50;
    static float quad_height = 50.;
    static float triangle_radius = 50.;

    if (igButton("reset all velocities", (ImVec2){})) {
        e_reset_all_velocities(st->r);
    }

    if (igSliderFloat("gravity y", &st->gravity.y, -100., 100., "%f", 0)) {
        trace(
            "box2d_actions_gui: gravity changed to %s\n",
            b2Vec2_to_str(st->gravity)
        );
        b2World_SetGravity(st->wctx.world, st->gravity);
    }

    igSliderInt("figures number", &num, 0, 1000, "%d", 0);
    igSliderFloat("quad height", &quad_height, 0., 500., "%f", 0);
    igSliderFloat("triangle radius", &triangle_radius, 0., 500., "%f", 0);

    //igSliderInt("borders gap", &borders_gap_px, -200, 200, "%d", 0);

    igCheckbox("draw sensors", &st->draw_sensors);
    igCheckbox("render_verbose", &render_verbose);
    igCheckbox("verbose", &st->verbose);

    if (igButton("apply random impulse to all dyn bodies", (ImVec2){})) {
        bool prev = koh_components_verbose;
        koh_components_verbose = true;
        e_apply_random_impulse_to_bodies(st->r, &st->wctx);
        koh_components_verbose = prev;
    }

    static bool use_static = false;
    igCheckbox("static bodies in 'spawn'", &use_static);

    if (igButton("spawn some quads", (ImVec2){})) {
        spawn_polygons(wctx, (struct PolySetup) {
            .r = st->r,
            .poly = b2MakeSquare(quad_height),
            .use_static = use_static,
            .r_opts = (struct ShapeRenderOpts) {
                .color = WHITE,
                //.tex = &st->tex_rt_template.texture,
                //.src = rect_by_texture(st->tex_rt_template.texture),
                .tex = &st->tex_example,
                .src = rect_by_texture(st->tex_example),
            },
        }, num, NULL);    
    }

    igSameLine(0., 5.);

    if (igButton("spawn one quad", (ImVec2){})) {
        spawn_poly(wctx, (struct PolySetup) {
            .r = st->r,
            .poly = b2MakeSquare(quad_height),
            .use_static = use_static,
            .r_opts = (struct ShapeRenderOpts) {
                .color = WHITE,
                //.tex = &st->tex_rt_template.texture,
                //.src = rect_by_texture(st->tex_rt_template.texture),
                .tex = &st->tex_example,
                .src = rect_by_texture(st->tex_example),
            },
        });    
    }

    igSameLine(0., 5.);

    if (igButton("spawn triangles", (ImVec2){})) {
        spawn_triangles(wctx, (struct TriangleSetup) {
            .r = st->r,
            .use_static = use_static,
            .r_opts = (struct ShapeRenderOpts) {
                .color = WHITE,
                //.tex = &st->tex_rt_template.texture,
                //.src = rect_by_texture(st->tex_rt_template.texture),
                .tex = &st->tex_example,
                .src = rect_by_texture(st->tex_example),
            },
            .radius = triangle_radius,
        }, num, NULL);
    }

    /*
    if (igButton("delete all dymanic bodies", (ImVec2){})) {
        if (delete_all_bodies(st, b2_dynamicBody, b2_nullShapeId
            )) {
            //st->shape_hightlight = b2_nullShapeId;
        }
    }
    */

    /*
    igSameLine(0., 5.);
    if (igButton("delete all static bodies", (ImVec2){})) {
        if (delete_all_bodies(st, b2_staticBody, b2_nullShapeId)) {
            //st->shape_hightlight = b2_nullShapeId;
        }
    }
    */

    if (igButton("spawn borders", (ImVec2){})) {
        remove_borders(&st->wctx, st->r, st->borders);
        spawn_borders(&st->wctx, st->r, st->borders, st->borders_gap_px);
    }

    igSameLine(0., 5.);

    if (igButton("delete borders", (ImVec2){})) {
        remove_borders(&st->wctx, st->r, st->borders);
    }

    if (igButton("reset camera", (ImVec2){})) {
        koh_cam_reset(&st->cam);
    }

    igSameLine(0., 5.);

    if (igButton("place some segments", (ImVec2){})) {
        spawn_segment(
            &st->wctx,
            &(struct SegmentSetup) {
                .start = (b2Vec2){ 0., 0. },
                .end = (b2Vec2){ st->wctx.width, st->wctx.height}, 
                //.color = pallete_get_random32(&st->pallete, st->wctx.xrng),
                .color = BROWN,
                .r = st->r,
        });
        spawn_segment(
            &st->wctx,
            &(struct SegmentSetup) {
                .start = (b2Vec2){ 
                    GetScreenWidth() / 2., GetScreenHeight() / 2.
                },
                .end = (b2Vec2){ st->wctx.width, st->wctx.height}, 
                //.color = pallete_get_random32(&st->pallete, st->wctx.xrng),
                .color = BROWN,
                .r = st->r,
        });
    }

    /*
    igCheckbox("draw bodies positions", &st->use_bodies_pos_drawer);
    igSliderFloat(
        "position center radius", &st->bodies_pos_drawer.radius,
        1.f, 50.f, "%f", 0
    );

    Color color = gui_color_combo(&st->bodies_drawer_color_combo, NULL);
    st->bodies_pos_drawer.color = color;
    */

    igEnd();
    // }}}
}

static char *get_selected_svg(Stage_shot *st) {
    for (int i = 0; i < st->fsr_svg.num; i++) 
        if (st->selected_svg[i])
            return st->fsr_svg.names[i];
    return NULL;
}

static void set_circles_restinition(
    de_ecs *r, WorldCtx *wctx, float circle_restinition
) {
    assert(r);
    assert(wctx);
    assert(circle_restinition >= 0 && circle_restinition <= 1.);

    /*
    de_view_single v = de_view_create_single(r, cp_type_circle);
    for (; de_view_single_valid(&v); de_view_single_next(&v)) {
        b2BodyId *bid = de_try_get(r, de_view_single_entity(&v), cp_type_body);
        if (!bid) continue;

        b2ShapeId shapes[16] = {};
        size_t shapes_num = sizeof(shapes) / sizeof(shapes[0]);
        int num = b2Body_GetShapes(*bid, shapes, shapes_num);

        for (int i = 0; i < num; i++) {
            b2Shape_SetRestitution(shapes[i], circle_restinition);
        }
    }
    */

    de_cp_type types[1] = { cp_type_circle };
    size_t types_num = sizeof(types) / sizeof(types[0]);
    de_view v = de_view_create(r, types_num, types);
    for (; de_view_valid(&v); de_view_next(&v)) {
        /*b2BodyId *bid = de_try_get(r, de_view_entity(&v), cp_type_body);*/
        b2BodyId *bid = de_get(r, de_view_entity(&v), cp_type_body);
        if (!bid) continue;

        assert(b2Body_IsValid(*bid));

        b2ShapeId shapes[16] = {};
        size_t shapes_num = sizeof(shapes) / sizeof(shapes[0]);
        int num = b2Body_GetShapes(*bid, shapes, shapes_num);

        for (int i = 0; i < num; i++) {
            b2Shape_SetRestitution(shapes[i], circle_restinition);
        }
    }
}

static void shot_gui(Stage_shot *st) {
    // {{{
    ImVec2 zero = {};

    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("magic", &open, flags);

    ImGuiComboFlags combo_flags = 0;
    if (igBeginCombo("scenes", get_selected_svg(st), combo_flags)) {
        for (int i = 0; i < st->fsr_svg.num; ++i) {
            ImGuiSelectableFlags selectable_flags = 0;
            if (igSelectable_BoolPtr(
                    st->fsr_svg.names[i], &st->selected_svg[i],
                    selectable_flags, (ImVec2){}
                )) {
                
                for (int j = 0; j < st->fsr_svg.num; ++j) {
                    if (i != j)
                        st->selected_svg[j] = false;
                }
            }
        }
        //*/
        igEndCombo();
    }

    if (igButton("reload scene", zero)) {
        const char *selected_svg = get_selected_svg(st);
        if (selected_svg && strlen(selected_svg)) {
            char *fname_noext cleanup(cleanup_char) = koh_str_sub_alloc(
                selected_svg,
                "\\.svg",
                ""
            );

            trace(
                "shot_gui: selected_svg '%s', fname_noext '%s'\n",
                selected_svg, fname_noext
             );

            load(st, fname_noext);

        }
        /*koh_search_files_shutdown(&st->fsr_svg);*/
        /*koh_search_files(&st->fss_svg);*/
    }

    igCheckbox("draw camera rect", &st->cam_draw_rect);
    igSliderFloat("gap_radius", &st->gap_radius, -700., 700., "%.3f", 0);
    igSliderFloat(
        "circle radius", &st->circle_radius, 1., 70., "%.3f", 
        ImGuiSliderFlags_Logarithmic
    );

    if (igButton("reset camera", zero)) {
        koh_cam_reset(&st->cam);
    }

    bool changed = igSliderFloat(
        "circles restinition", &st->circle_restinition,
        0., 1., "%f", 0
    );

    if (changed) {
        set_circles_restinition(st->r, &st->wctx, st->circle_restinition);
    }

    igSliderFloat(
        "rotation velocity", &st->rot_delta_angle,
        0., 10., "%f", 0
    );

    changed = igSliderFloat(
        "grav_len", &st->grav_len,
        -100., 100., "%f", 0
    );

    if (changed) {
        set_gravity(st, st->cam.rotation);
    }

    igSliderFloat("line thick", &st->line_thick, 1., 20., "%f", 0);

    igEnd();
    // }}}
}

/*
// Imgui style selector {{{
const char *values_map[] = {

    [ImGuiCol_Text] = "ImGuiCol_Text",
    [ImGuiCol_TextDisabled] = "ImGuiCol_TextDisabled",
    [ImGuiCol_WindowBg] = "ImGuiCol_WindowBg",
    [ImGuiCol_ChildBg] = "ImGuiCol_ChildBg",
    [ImGuiCol_PopupBg] = "ImGuiCol_PopupBg",
    [ImGuiCol_Border] = "ImGuiCol_Border",
    [ImGuiCol_BorderShadow] = "ImGuiCol_BorderShadow",
    [ImGuiCol_FrameBg] = "ImGuiCol_FrameBg",
    [ImGuiCol_FrameBgHovered] = "ImGuiCol_FrameBgHovered",
    [ImGuiCol_FrameBgActive] = "ImGuiCol_FrameBgActive",
    [ImGuiCol_TitleBg] = "ImGuiCol_TitleBg",
    [ImGuiCol_TitleBgActive] = "ImGuiCol_TitleBgActive",
    [ImGuiCol_TitleBgCollapsed] = "ImGuiCol_TitleBgCollapsed",
    [ImGuiCol_MenuBarBg] = "ImGuiCol_MenuBarBg",
    [ImGuiCol_ScrollbarBg] = "ImGuiCol_ScrollbarBg",
    [ImGuiCol_ScrollbarGrab] = "ImGuiCol_ScrollbarGrab",
    [ImGuiCol_ScrollbarGrabHovered] = "ImGuiCol_ScrollbarGrabHovered",
    [ImGuiCol_ScrollbarGrabActive] = "ImGuiCol_ScrollbarGrabActive",
    [ImGuiCol_CheckMark] = "ImGuiCol_CheckMark",
    [ImGuiCol_SliderGrab] = "ImGuiCol_SliderGrab",
    [ImGuiCol_SliderGrabActive] = "ImGuiCol_SliderGrabActive",
    [ImGuiCol_Button] = "ImGuiCol_Button",
    [ImGuiCol_ButtonHovered] = "ImGuiCol_ButtonHovered",
    [ImGuiCol_ButtonActive] = "ImGuiCol_ButtonActive",
    [ImGuiCol_Header] = "ImGuiCol_Header",
    [ImGuiCol_HeaderHovered] = "ImGuiCol_HeaderHovered",
    [ImGuiCol_HeaderActive] = "ImGuiCol_HeaderActive",
    [ImGuiCol_Separator] = "ImGuiCol_Separator",
    [ImGuiCol_SeparatorHovered] = "ImGuiCol_SeparatorHovered",
    [ImGuiCol_SeparatorActive] = "ImGuiCol_SeparatorActive",
    [ImGuiCol_ResizeGrip] = "ImGuiCol_ResizeGrip",
    [ImGuiCol_ResizeGripHovered] = "ImGuiCol_ResizeGripHovered",
    [ImGuiCol_ResizeGripActive] = "ImGuiCol_ResizeGripActive",
    [ImGuiCol_Tab] = "ImGuiCol_Tab",
    [ImGuiCol_TabHovered] = "ImGuiCol_TabHovered",
    [ImGuiCol_TabActive] = "ImGuiCol_TabActive",
    [ImGuiCol_TabUnfocused] = "ImGuiCol_TabUnfocused",
    [ImGuiCol_TabUnfocusedActive] = "ImGuiCol_TabUnfocusedActive",
    [ImGuiCol_PlotLines] = "ImGuiCol_PlotLines",
    [ImGuiCol_PlotLinesHovered] = "ImGuiCol_PlotLinesHovered",
    [ImGuiCol_PlotHistogram] = "ImGuiCol_PlotHistogram",
    [ImGuiCol_PlotHistogramHovered] = "ImGuiCol_PlotHistogramHovered",
    [ImGuiCol_TableHeaderBg] = "ImGuiCol_TableHeaderBg",
    [ImGuiCol_TableBorderStrong] = "ImGuiCol_TableBorderStrong",
    [ImGuiCol_TableBorderLight] = "ImGuiCol_TableBorderLight",
    [ImGuiCol_TableRowBg] = "ImGuiCol_TableRowBg",
    [ImGuiCol_TableRowBgAlt] = "ImGuiCol_TableRowBgAlt",
    [ImGuiCol_TextSelectedBg] = "ImGuiCol_TextSelectedBg",
    [ImGuiCol_DragDropTarget] = "ImGuiCol_DragDropTarget",
    [ImGuiCol_NavHighlight] = "ImGuiCol_NavHighlight",
    [ImGuiCol_NavWindowingHighlight] = "ImGuiCol_NavWindowingHighlight",
    [ImGuiCol_NavWindowingDimBg] = "ImGuiCol_NavWindowingDimBg",
    [ImGuiCol_ModalWindowDimBg] = "ImGuiCol_ModalWindowDimBg",

};

int values_num = sizeof(values_map) / sizeof(values_map[0]);

const char *getter(void *data, int n) {
    return values_map[n];
}
int static style = 0;
// }}}
*/

const char *uint64_to_str_bin(uint64_t value) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    
    //for (uint32_t i = 63; i >= 0; i--) {

    //for (int i = 63; i >= 0; i--) {
        //buf[63 - i] = (value & (1ULL << (uint64_t)i)) ? '1' : '0';
    //}

    for (int i = 0; i < 64; i++) {
        buf[i] = (value & (1ULL << (uint64_t)i)) ? '1' : '0';
    }

    buf[64] = '\0';  // Null-terminate the string
    return buf;
}

__attribute_maybe_unused__
static void bit_calculator_gui() {
    // {{{
    bool opened = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("x86_64 bit calculator", &opened, flags);

    static bool bits[64] = {};
    size_t bits_num = sizeof(bits) / sizeof(bits[0]);

    static int spacing = 19;
    //igSliderInt("spacing", &spacing, 0, 100, "%d", 0);

    for (int i = bits_num; i; i--) {
        igText("%.2d", i - 1);
        if (i - 1 != 0)
            igSameLine(0., spacing);
    }

    ImVec4  col_checkmark = {0.5, 0., 0., 1.},
            col_framebg = {0.5, 0.5, 0.5, 1.},
            col_hovered = { 0., 0.7, 0., 1.};

    //igListBox_FnStrPtr("style", &style, getter, NULL, values_num , 10);
    //trace("bit_calculator_gui: style %d\n", style);

    for (int i = 0; i < bits_num; i++) {
        char checkbox_id[32] = {};
        sprintf(checkbox_id, "##%d", i);

        //igPushStyleColor_Vec4(ImGuiCol_CheckMark, checkbox_color);
        igPushStyleColor_Vec4(ImGuiCol_FrameBg, col_framebg);
        igPushStyleColor_Vec4(ImGuiCol_FrameBgHovered, col_hovered);
        igPushStyleColor_Vec4(ImGuiCol_CheckMark, col_checkmark);

        igCheckbox(checkbox_id, &bits[i]);

        //igSameLine(0., 0.);
        //igText("|");

        igPopStyleColor(3);

        if (i + 1 != bits_num)
            igSameLine(0., 10.);
    }

    uint64_t result = 0;
    for (int i = 0; i < bits_num; i++) {
        result = (result << 1) | bits[i];
    }

    //trace("bit_calculator_gui:\n");

    const int buf_len = 128;
    char buf_bin[buf_len],
         buf_oct[buf_len],
         buf_dec[buf_len],
         buf_hex[buf_len];

    memset(buf_bin, 0, sizeof(buf_bin));
    memset(buf_oct, 0, sizeof(buf_oct));
    memset(buf_dec, 0, sizeof(buf_dec));
    memset(buf_hex, 0, sizeof(buf_hex));

    sprintf(buf_bin, "%s", uint64_to_str_bin(result));
    sprintf(buf_oct, "%llo", (unsigned long long)result);
    sprintf(buf_dec, "%llu", (unsigned long long)result);
    sprintf(buf_hex, "%llx", (unsigned long long)result);

    //size_t buf_len = sizeof(buf) / sizeof(buf[0]);
    ImGuiInputFlags input_flags = 0;

    igText("bin:");
    igSameLine(0., 10.);
    igInputText("##input", buf_bin, buf_len, input_flags, NULL, NULL);

    igText("oct:");
    igSameLine(0., 10.);
    igInputText("##input", buf_oct, buf_len, input_flags, NULL, NULL);

    igText("dec:");
    igSameLine(0., 10.);
    igInputText("##input", buf_dec, buf_len, input_flags, NULL, NULL);

    igText("hex:");
    igSameLine(0., 10.);
    igInputText("##input", buf_hex, buf_len, input_flags, NULL, NULL);

    //sscanf(buf, "%lu", &result);
    for (uint64_t i = 0; i < bits_num; i++) {
        //bool bit = !!(result & (1 << i));
        //bits[i] = bit;
    }
    // */

    igEnd();
    // }}}
}

__attribute_maybe_unused__
static void simd_bit_calculator_gui() {
    // {{{
    bool opened = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("SIMD bit calculator", &opened, flags);

    //__m256i a = {}, b = {}, c = {};
    //c = a + b;

    static bool bits[256] = {};
    size_t bits_num = sizeof(bits) / sizeof(bits[0]);

    static int spacing = 19;
    //igSliderInt("spacing", &spacing, 0, 100, "%d", 0);

    for (int i = bits_num; i; i--) {
        igText("%.2d", i - 1);
        if (i - 1 != 0 && i % 64)
            igSameLine(0., spacing);
    }

    for (int i = 0; i < bits_num; i++) {
        char checkbox_id[32] = {};
        sprintf(checkbox_id, "##%d", i);
        igCheckbox(checkbox_id, &bits[i]);
        if (i + 1 != bits_num)
            igSameLine(0., 10.);
    }

    uint64_t result = 0;
    for (int i = 0; i < bits_num; i++) {
        result = (result << 1) | bits[i];
    }

    trace("bit_calculator_gui:\n");

    char buf[128] = {};
    sprintf(buf, "%lu", result);
    size_t buf_len = sizeof(buf) / sizeof(buf[0]);
    ImGuiInputFlags input_flags = 0;
    igText("number");
    igSameLine(0., 10.);
    igInputText("##input", buf, buf_len, input_flags, NULL, NULL);

    sscanf(buf, "%lu", &result);
    for (uint64_t i = 0; i < bits_num; i++) {
        //bool bit = !!(result & (1 << i));
        //bits[i] = bit;
    }

    igEnd();
    // }}}
}

static void stage_shot_gui(struct Stage_shot *st) {
    assert(st);

    input_gp_update(st->gp_drawer);
    input_kb_update(st->kb_drawer);
    input_kb_gui_update(st->kb_drawer);

    //simd_bit_calculator_gui();
    bit_calculator_gui();

    box2d_gui(&st->wctx);
    box2d_actions_gui(st);
    shot_gui(st);
    de_gui(st->r, de_null);
}

static void stage_shot_enter(struct Stage_shot *st) {
    trace("stage_shot_enter:\n");
}

static void stage_shot_leave(struct Stage_shot *st) {
    trace("stage_shot_leave:\n");
}

static float distPtSeg(
    float x, float y, float px, float py, float qx, float qy
) {
    float pqx, pqy, dx, dy, d, t;
    pqx = qx-px;
    pqy = qy-py;
    dx = x-px;
    dy = y-py;
    d = pqx*pqx + pqy*pqy;
    t = pqx*dx + pqy*dy;
    if (d > 0) t /= d;
    if (t < 0) t = 0;
    else if (t > 1) t = 1;
    dx = px + t*pqx - x;
    dy = py + t*pqy - y;
    return dx*dx + dy*dy;
}

static void cubicBez(
    float x1, float y1, float x2, float y2,
    float x3, float y3, float x4, float y4,
    float tol, int level,
    Vector2 *points, int *points_num, int points_cap
) {
    float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;
    float d;
    
    if (level > 12) return;

    const float coef = .5f;

    x12 = (x1 + x2) * coef;
    y12 = (y1 + y2) * coef;
    x23 = (x2 + x3) * coef;
    y23 = (y2 + y3) * coef;
    x34 = (x3 + x4) * coef;
    y34 = (y3 + y4) * coef;
    x123 = (x12 + x23) * coef;
    y123 = (y12 + y23) * coef;
    x234 = (x23 + x34) * coef;
    y234 = (y23 + y34) * coef;
    x1234 = (x123 + x234) * coef;
    y1234 = (y123 + y234) * coef;

    d = distPtSeg(x1234, y1234, x1,y1, x4,y4);
    if (d > tol*tol) {
        cubicBez(
            x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1,
            points, points_num, points_cap
        ); 
        cubicBez(
            x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1,
            points, points_num, points_cap
        ); 
    } else {
        /*rlVertex2f(x4, y4);*/
            if (*points_num + 1 < points_cap)
                points[(*points_num)++] = (Vector2) { x4, y4, };
    }
}

//static const unsigned char bgColor[4] = {205,202,200,255};
/*static const unsigned char lineColor[4] = {0,160,192,255};*/

static void draw_path(
    float* pts_in, int npts, char closed, float tol,
    float scale,
    Vector2 *points, int *points_num, int points_cap
) {
    //trace("draw_path: npts %d\n", npts);
    float pts[npts * 2];
    memset(pts, 0, sizeof(pts));
    for (int i = 0; i < npts * 2; i++)
        pts[i] = scale * pts_in[i];

    if (*points_num + 1 < points_cap)
        points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    
    for (int i = 0; i < npts-1; i += 3) {
        float* p = &pts[i*2];
        cubicBez(
            p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0,
            points, points_num, points_cap
        );
    }

    if (closed) {
        /*rlVertex2f(pts[0], pts[1]);*/

        //trace("++\n");
        if (*points_num + 1 < points_cap)
            points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    }
}

static void render_pass_bound(Stage_shot *st) {
    float thick = 5.;
    Color color = RAYWHITE;
    Rectangle r = st->svg_bound;

    DrawLineEx(
        (Vector2) { r.x, r.y, }, 
        (Vector2) { r.x + r.width, r.y}, 
        thick, color
    );

    DrawLineEx(
        (Vector2) { r.x + r.width, r.y, }, 
        (Vector2) { r.x + r.width, r.y + r.height}, 
        thick, color
    );

    DrawLineEx(
        (Vector2) { r.x + r.width, r.y + r.height}, 
        (Vector2) { r.x, r.y + r.height}, 
        thick, color
    );

    DrawLineEx(
        (Vector2) { r.x, r.y + r.height}, 
        (Vector2) { r.x, r.y, }, 
        thick, color
    );

}

static void render_pass_box2d(Stage_shot *st) {
    if (!st->world_query_AABB)
        return;

    // Прямоугольник видимой части экрана с учетом масштаба
    b2AABB screen = camera2aabb(&st->cam, st->gap_radius);

    //console_write("st->cam '%s'\n", camera2str(st->cam, false));
    //trace("render_pass_box2d: st->cam '%s'\n", camera2str(st->cam, false));

    Rectangle rect = aabb2rect(screen);
    //trace("render_pass_box2d: rect '%s'\n", rect2str(rect));
    //rect.x += st->cam.target.x;
    //rect.y += st->cam.target.y;
    if (st->cam_draw_rect)
        DrawRectangleRec(rect, BLUE);

    b2QueryFilter filter = b2DefaultQueryFilter();
    b2World_OverlapAABB(
        st->wctx.world, screen, filter, query_world_draw, st
    );
}

static void render_pass_svg(
    NSVGimage *img, float scale, float line_thick
) {
    assert(img);
    assert(scale != 0.);
	NSVGshape *shape;
	NSVGpath  *path;

    Vector2 points[1024 * 2] = {};
    int points_cap = sizeof(points) / sizeof(points[0]);
    int points_num = 0;
    assert(line_thick > 0.);

    for (shape = img->shapes; shape != NULL; shape = shape->next) {
        for (path = shape->paths; path != NULL; path = path->next) {
            draw_path(
                path->pts, path->npts, path->closed, 1.5, 
                scale,
                points, &points_num, points_cap
            );
            //drawControlPts(path->pts, path->npts);

            unsigned char *components = ((unsigned char*)&shape->stroke.color);

            Color color = {
                .r = components[0],
                .g = components[1],
                .b = components[2],
                .a = 255,
            };

            for (int i = 0; i + 1 < points_num; i++) {
                DrawLineEx(points[i], points[i + 1], line_thick, color);
            }
            for (int i = 0; i < points_num; i++) {
                DrawCircleV(points[i], line_thick / 2., color);
            }

            // TODO: Рисовать недостающий сегмент
            //DrawLineEx(points[points_num - 2], points[points_num - 1], thick, color);
            //DrawLineEx(points[0], points[points_num - 1], thick, color);
            //DrawLineEx(points[0], points[1], thick, color);

            points_num = 0;
        }
    }

}

static void render_pass_debug(Stage_shot *st) {
    Vector2 center = { st->nsvg_img->width / 2., st->nsvg_img->height / 2.};
    DrawCircleV(center, 30., WHITE);

    const float grav_scale = 10., line_thick = 10.;
    Vector2 line_end = Vector2Add(
        center, 
        Vector2Scale(b2Vec2_to_Vector2(st->gravity), grav_scale)
    );
    DrawCircleV(line_end, 20., BLACK);
    DrawLineEx(
        center,
        line_end,
        line_thick,
        BLUE
    );

    WorldCtx *wctx = &st->wctx;
    if (wctx->is_dbg_draw)
        b2World_Draw(wctx->world, &wctx->world_dbg_draw);
}

static void stage_shot_draw(Stage_shot *st) {
    BeginMode2D(st->cam);

    L_call(st->l, "draw_pre");

    render_pass_svg(st->nsvg_img, 1., st->line_thick);
    render_pass_box2d(st);
    render_pass_bound(st);

    //DrawCircle(0., 0., 40., BLACK);

    L_call(st->l, "draw_post");

    render_pass_debug(st);

    EndMode2D();
}

static void stage_shot_shutdown(struct Stage_shot *st) {
    trace("stage_shot_shutdown:\n");

    input_gp_free(st->gp_drawer);
    input_kb_free(st->kb_drawer);
    koh_search_files_shutdown(&st->fsr_svg);

    unload(st);
}

