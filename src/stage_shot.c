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
#include "koh_b2_world.h"
#include <assert.h>
#include <stdlib.h>
// }}}

typedef struct Stage_shot {
    // {{{
    Stage             parent;
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

    // }}}
} Stage_shot;

static bool world_query_AABB = true;
static bool draw_sensors = false,
            verbose = false;

static int borders_gap_px = 0;

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
    float len_sq = b2LengthSquared(b2Sub(seg.point1, seg.point2));

    if (len_sq > FLT_EPSILON) {
        b2BodyDef bd = b2DefaultBodyDef();
        bd.type = b2_staticBody;

        b2BodyId bid = b2CreateBody(wctx->world, &bd);

        b2ShapeDef sd = b2DefaultShapeDef();
        b2CreateSegmentShape(bid, &sd, &seg);
        trace("segment_create: created\n");
    } else {
        trace("segment_create: too short segment\n");
    }
    return de_null;
}

__attribute_maybe_unused__
static void body_creator(float x, float y, void *udata) {
    struct CreatorCtx *ctx = udata;
    WorldCtx *wctx = &ctx->st->wctx;
    Stage_shot *st = ctx->st;
    /*trace("body_creator:\n");*/

    if (ctx->last_used) {
        trace(
            "body_creator: start %s, end %s\n",
            b2Vec2_to_str(ctx->last),
            b2Vec2_to_str((b2Vec2) { x, y })
        );

        /*
        spawn_segment(
            wctx,
            &(struct SegmentSetup) {
                .start = ctx->last,
                .end = (b2Vec2) { x, y },
                //.color = pallete_get_random32(&st->pallete, st->wctx.xrng),
                .color = RED,
                .r = st->r,
        });
        */

        segment_create(st->r, wctx, ctx->last, (b2Vec2) { x, y });
    } else {
        ctx->last_used = true;
    }

    ctx->last.x = x;
    ctx->last.y = y;
}

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
}

static void svg_shutdown_selected(FilesSearchResult *fsr) {
    Stage_shot *st = fsr->udata;
    assert(st);

    if (st->selected_svg) {
        free(st->selected_svg);
        st->selected_svg = NULL;
    }
}

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
    bd.type = b2_staticBody;
    bd.position = (b2Vec2) { rect.x, rect.y };

    *bid = b2CreateBody(wctx->world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    //b2CreateSegmentShape(bid, &sd, &seg);
    b2CreatePolygonShape(*bid, &sd, &poly);
    trace("box_create: created\n");
    return e;
}

static void stage_shot_init(struct Stage_shot *st) {
    trace("stage_shot_init:\n");

    st->fss_svg.deep = 2;
    st->fss_svg.path = "assets";
    st->fss_svg.regex_pattern = ".*\\.svg$";
    st->fss_svg.on_search_begin = svg_begin_search;
    st->fss_svg.on_search_end = svg_end_search;
    st->fss_svg.on_shutdown = svg_shutdown_selected;
    st->fss_svg.udata = st;

    Resource *rl = &st->res_list;
    st->tex_example = res_tex_load(rl, "assets/magic_circle_01.png");

    st->cam.zoom = 1.;
    st->r = de_ecs_make();

    const char *path = "assets/magic_level_04.svg";
    st->nsvg_img = nsvgParseFromFile(path, "px", 96.0f);
    assert(st->nsvg_img);

    st->xrng = xorshift32_init();
    //st->gravity = (b2Vec2) { 0., -9.8 };
    st->gravity = (b2Vec2) { 0., -0.1 };

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

    /*
    svg_parse(st->nsvg_img, body_creator, &(struct CreatorCtx) {
        .last_used = false,
        .st = st,
    });
    */
    
    trace(
        "stage_shot_init: svg size %fx%f\n",
        st->nsvg_img->width,
        st->nsvg_img->height
    );
    st->svg_bound.width = st->nsvg_img->width;
    st->svg_bound.height = st->nsvg_img->height;

    segment_create(
        st->r, &st->wctx,
        (b2Vec2) { 0., 0. },
        (b2Vec2) { GetScreenWidth(), GetScreenHeight(), }
    );
    segment_create(
        st->r, &st->wctx,
        (b2Vec2) { 0., 0. },
        (b2Vec2) { GetScreenWidth(), 0., }
    );

    box_create(st->r, &st->wctx, (Rectangle) { 500., 500., 500., 500., });
    box_create(st->r, &st->wctx, (Rectangle) { 50., 1500., 500., 500., });
    box_create(st->r, &st->wctx, (Rectangle) { 50., 500., 500., 500., });
    box_create(st->r, &st->wctx, (Rectangle) { 500., 1500., 500., 500., });

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

static void shot_gui(Stage_shot *st) {
    // {{{
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
    igSameLine(0., 5.);

    ImVec2 zero = {};
    if (igButton("switch", zero)) {
        koh_search_files_shutdown(&st->fsr_svg);
        koh_search_files(&st->fss_svg);
    }

    igEnd();
    // }}}
}

static void stage_shot_gui(struct Stage_shot *st) {
    assert(st);
    box2d_gui(&st->wctx);
    box2d_actions_gui(st);
    shot_gui(st);
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
        /*rlVertex2f(x4, y4);*/
            if (*points_num + 1 < points_cap)
                points[(*points_num)++] = (Vector2) { x4, y4, };
    }
}

//static const unsigned char bgColor[4] = {205,202,200,255};
/*static const unsigned char lineColor[4] = {0,160,192,255};*/

static void draw_path(
    float* pts, int npts, char closed, float tol,
    Vector2 *points, int *points_num, int points_cap
) {
    int i;
    //rlBegin(GL_LINE_STRIP);
    /*rlBegin(RL_LINES);*/
    //rlColor4ubv(lineColor);
    /*rlColor4ub(lineColor[0], lineColor[1], lineColor[2], lineColor[3]);*/
    /*rlVertex2f(pts[0], pts[1]);*/

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
        /*rlVertex2f(pts[0], pts[1]);*/

        //trace("++\n");
        if (*points_num + 1 < points_cap)
            points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    }
    /*rlEnd();*/
}

static void pass_bound(Stage_shot *st) {
    float thick = 5.;
    Color color = BLACK;
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
	NSVGshape *shape;
	NSVGpath  *path;
    NSVGimage *img = st->nsvg_img;

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

            const float thick = 5.;
            unsigned char *components = ((unsigned char*)&shape->stroke.color);

            /*
            trace(
                "pass_svg: components { %d, %d, %d, %d }\n",
                components[0],
                components[1],
                components[2],
                components[3]
            );
            */

            Color color = {
                .r = components[0],
                .g = components[1],
                .b = components[2],
                .a = 255,
            };

            //trace("stage_shot_draw: points_num %d\n", points_num);

            for (int i = 0; i + 1 < points_num; i++) {
                DrawLineEx(points[i], points[i + 1], thick, color);
            }
            points_num = 0;
        }
    }

}

static void stage_shot_draw(Stage_shot *st) {
    BeginMode2D(st->cam);

    pass_svg(st);
    pass_box2d(st);
    pass_bound(st);

    WorldCtx *wctx = &st->wctx;
    if (wctx->is_dbg_draw)
        b2World_Draw(wctx->world, &wctx->world_dbg_draw);

    EndMode2D();
}

static void stage_shot_shutdown(struct Stage_shot *st) {
    trace("stage_shot_shutdown:\n");
    nsvgDelete(st->nsvg_img);
    de_ecs_destroy(st->r);
    world_shutdown(&st->wctx);
    res_unload_all(&st->res_list);
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
