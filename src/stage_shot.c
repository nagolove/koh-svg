// vim: set colorcolumn=85
// vim: fdm=marker
#include "stage_shot.h"

#define NANOSVG_IMPLEMENTATION

// includes {{{
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
#include "svg.h"
#include <assert.h>
#include <stdlib.h>
// }}}

typedef struct Stage_shot {
    // {{{
    Stage             parent;
    Camera2D          cam;
    NSVGimage         *nsvg_img;
    WorldCtx          wctx;
    xorshift32_state  xrng;
    b2Vec2            gravity;
    de_ecs            *r;
    Texture2D         tex_example;
    Resource          res_list;
    // }}}
} Stage_shot;

static bool draw_sensors = false,
            verbose = false;

static bool query_world_draw(b2ShapeId shape_id, void* context) {
    //trace("query_world_draw:\n");
    struct Stage_shot *st = context;

    if (!draw_sensors) {
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
            trace("query_world_draw: b2_segmentShape is not implemented\n");
            break;
        default: 
            trace("query_world_draw: default branch, unknown shape type\n");
            break;
    }

    return true;
}

static void body_creator(float x, float y, void *udata) {
    Stage_shot *st = udata;
    b2WorldId wid = st->wctx.world;
    trace("body_creator:\n");


    spawn_segment(
        &st->wctx,
        &(struct SegmentSetup) {
            .start = (b2Vec2){ 0., 0. },
            .end = (b2Vec2){ st->wctx.width, st->wctx.height}, 
            //.color = pallete_get_random32(&st->pallete, st->wctx.xrng),
            .color = RED,
            .r = st->r,
    });

}

static void stage_shot_init(struct Stage_shot *st) {
    trace("stage_shot_init:\n");
    st->cam.zoom = 1.;
    st->r = de_ecs_make();

    const char *path = "assets/magic_level_01.svg";
    st->nsvg_img = nsvgParseFromFile(path, "px", 96.0f);
    assert(st->nsvg_img);

    st->xrng = xorshift32_init();
    st->gravity = (b2Vec2) { 0., -9.8 };

    b2WorldDef wd = b2DefaultWorldDef();
    wd.enableContinous = true;
    wd.enableSleep = true;
    wd.gravity = st->gravity;

    world_init(&(struct WorldCtxSetup) {
        .xrng = &st->xrng,
        .width = 0,  // Устанавливаются позже в terr_on_new
        .height = 0, // Устанавливаются позже в terr_on_new
        .wd = &wd,

    }, &st->wctx);

    svg_parse(st->nsvg_img, body_creator, st);
}

static void stage_shot_update(struct Stage_shot *st) {
    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
            .mouse_btn = MOUSE_BUTTON_RIGHT,
            .cam = &st->cam
    });
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        //.mouse_btn = MOUSE_BUTTON_LEFT,
        .cam = &st->cam,
        .dscale_value = 0.05,
        .modifier_key_down = KEY_LEFT_SHIFT,
    });

}

static void box2d_actions_gui(struct Stage_shot *st) {
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

    igSliderInt("figures number", &num, 0, 1000, "%d", 0);
    igSliderFloat("quad height", &quad_height, 0., 500., "%f", 0);
    igSliderFloat("triangle radius", &triangle_radius, 0., 500., "%f", 0);

    //igSliderInt("borders gap", &borders_gap_px, -200, 200, "%d", 0);

    igCheckbox("draw sensors", &draw_sensors);
    igCheckbox("render_verbose", &render_verbose);
    igCheckbox("verbose", &verbose);

    if (igButton("apply random impulse to all dyn bodies", (ImVec2){})) {
        e_apply_random_impulse_to_bodies(st->r, &st->wctx);
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

    if (igButton("delete all dymanic bodies", (ImVec2){})) {
        if (delete_all_bodies(st, b2_dynamicBody, b2_nullShapeId
            )) {
            //st->shape_hightlight = b2_nullShapeId;
        }
    }

    igSameLine(0., 5.);

    if (igButton("delete all static bodies", (ImVec2){})) {
        if (delete_all_bodies(st, b2_staticBody, b2_nullShapeId)) {
            //st->shape_hightlight = b2_nullShapeId;
        }
    }


    if (igButton("spawn borders", (ImVec2){})) {
        remove_borders(&st->wctx, st->r, st->borders);
        spawn_borders(&st->wctx, st->r, st->borders, borders_gap_px);
    }

    igSameLine(0., 5.);

    if (igButton("delete borders", (ImVec2){})) {
        remove_borders(&st->wctx, st->r, st->borders);
    }

    if (igButton("reset camera", (ImVec2){})) {
        st->cam.zoom = 1.;
        st->cam.rotation = 0.;
        //st->cam.target.x = 0.;
        //st->cam.target.y = 0.;
        st->cam.offset.x = 0.;
        st->cam.offset.y = 0.;
    }

    igSameLine(0., 5.);

    if (igButton("reset hero", (ImVec2){})) {
        hero_destroy(st->r, &st->e_hero);
        st->e_hero = hero_create(&st->wctx, st->r);
    }

    if (igButton("place some segments", (ImVec2){})) {
        spawn_segment(
            &st->wctx,
            &(struct SegmentSetup) {
                .start = (b2Vec2){ 0., 0. },
                .end = (b2Vec2){ st->wctx.width, st->wctx.height}, 
                .color = pallete_get_random32(&st->pallete, st->wctx.xrng),
                .r = st->r,
        });
        spawn_segment(
            &st->wctx,
            &(struct SegmentSetup) {
                .start = (b2Vec2){ 
                    GetScreenWidth() / 2., GetScreenHeight() / 2.
                },
                .end = (b2Vec2){ st->wctx.width, st->wctx.height}, 
                .color = pallete_get_random32(&st->pallete, st->wctx.xrng),
                .r = st->r,
        });
    }

    igCheckbox("draw bodies positions", &st->use_bodies_pos_drawer);
    igSliderFloat(
        "position center radius", &st->bodies_pos_drawer.radius,
        1.f, 50.f, "%f", 0
    );

    Color color = gui_color_combo(&st->bodies_drawer_color_combo, NULL);
    st->bodies_pos_drawer.color = color;

    igEnd();
}

static void stage_shot_gui(struct Stage_shot *st) {
    assert(st);
    box2d_gui(&st->wctx);
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

    x12 = (x1+x2)*0.5f;
    y12 = (y1+y2)*0.5f;
    x23 = (x2+x3)*0.5f;
    y23 = (y2+y3)*0.5f;
    x34 = (x3+x4)*0.5f;
    y34 = (y3+y4)*0.5f;
    x123 = (x12+x23)*0.5f;
    y123 = (y12+y23)*0.5f;
    x234 = (x23+x34)*0.5f;
    y234 = (y23+y34)*0.5f;
    x1234 = (x123+x234)*0.5f;
    y1234 = (y123+y234)*0.5f;

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
        rlVertex2f(x4, y4);
            if (*points_num + 1 < points_cap)
                points[(*points_num)++] = (Vector2) { x4, y4, };
    }
}

//static const unsigned char bgColor[4] = {205,202,200,255};
static const unsigned char lineColor[4] = {0,160,192,255};

static void draw_path(
    float* pts, int npts, char closed, float tol,
    Vector2 *points, int *points_num, int points_cap
) {
    int i;
    //rlBegin(GL_LINE_STRIP);
    rlBegin(RL_LINES);
    //rlColor4ubv(lineColor);
    rlColor4ub(lineColor[0], lineColor[1], lineColor[2], lineColor[3]);
    rlVertex2f(pts[0], pts[1]);

    //trace("++\n");
    if (*points_num + 1 < points_cap)
        points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    
    //rlEnd();
    //rlBegin(GL_LINE_STRIP);

    for (i = 0; i < npts-1; i += 3) {
        float* p = &pts[i*2];
        cubicBez(
            p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0,
            points, points_num, points_cap
        );
    }

    //trace("draw_path: points_num %d\n", *points_num);

    if (closed) {
        rlVertex2f(pts[0], pts[1]);

        //trace("++\n");
        if (*points_num + 1 < points_cap)
            points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    }
    rlEnd();
}

static void pass_box2d(Stage_shot *st) {
    if (!world_query_AABB)
        return;

    float gap_radius = 100;
    // Прямоугольник видимой части экрана с учетом масштаба
    b2AABB screen = camera2aabb(&st->cam, gap_radius);
    b2QueryFilter filter = b2DefaultQueryFilter();
    b2World_OverlapAABB(
        st->wctx.world, screen, filter, query_world_draw, st
    );
}

static void pass_svg(Stage_shot *st) {
    Vector2 points[1024] = {};
    int points_cap = sizeof(points) / sizeof(points[0]);
    int points_num = 0;

    const float px = 1.;

    //trace("stage_shot_draw:\n");

    for (shape = img->shapes; shape != NULL; shape = shape->next) {
        for (path = shape->paths; path != NULL; path = path->next) {
            draw_path(
                    path->pts, path->npts, path->closed, px * 1.5f, 
                    points, &points_num, points_cap
                    );
            //drawControlPts(path->pts, path->npts);
        }
    }

const float thick = 5.;
Color color = BLUE;

//trace("stage_shot_draw: points_num %d\n", points_num);

for (int i = 0; i + 1 < points_num; i++) {
    DrawLineEx(points[i], points[i + 1], thick, color);
}
}

static void stage_shot_draw(Stage_shot *st) {
	NSVGshape *shape;
	NSVGpath  *path;
    NSVGimage *img = st->nsvg_img;

    BeginMode2D(st->cam);

    pass_svg(st);
    pass_box2d(st);

    EndMode2D();
}

static void stage_shot_shutdown(struct Stage_shot *st) {
    trace("stage_shot_shutdown:\n");
    nsvgDelete(st->nsvg_img);
    de_ecs_destroy(st->r);
    world_shutdown(&st->wctx);
}

Stage *stage_shot_new(HotkeyStorage *hk_store) {
    assert(hk_store);
    struct Stage_shot *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_shot_init;
    st->parent.enter = (Stage_callback)stage_shot_enter;
    st->parent.leave = (Stage_callback)stage_shot_leave;

    st->parent.gui = (Stage_callback)stage_shot_gui;
    st->parent.update = (Stage_callback)stage_shot_update;
    st->parent.draw = (Stage_callback)stage_shot_draw;
    st->parent.shutdown = (Stage_callback)stage_shot_shutdown;

    return (Stage*)st;
}
