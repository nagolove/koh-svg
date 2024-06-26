// vim: set colorcolumn=85
// vim: fdm=marker
#include "stage_shot.h"

#define NANOSVG_IMPLEMENTATION

// includes {{{
#include "cimgui.h"
#include "koh_common.h"
#include "koh_hotkey.h"
#include "koh_stages.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include "koh_common.h"
#include "nanosvg.h"
#include "rlgl.h"
#include "koh_b2.h"
#include "koh_b2_world.h"
#include <GL/gl.h>              // OpenGL 1.1 library
// }}}

typedef struct Stage_shot {
    Stage             parent;
    Camera2D          cam;
    NSVGimage         *nsvg_img;
    WorldCtx          wctx;
    xorshift32_state  xrng;
    b2Vec2            gravity;
} Stage_shot;

static void stage_shot_init(struct Stage_shot *st) {
    trace("stage_shot_init:\n");
    st->cam.zoom = 1.;

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

static void stage_shot_gui(struct Stage_shot *st) {
    assert(st);
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

static const unsigned char bgColor[4] = {205,202,200,255};
static const unsigned char lineColor[4] = {0,160,192,255};

void draw_path(
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

    trace("draw_path: points_num %d\n", *points_num);

    if (closed) {
        rlVertex2f(pts[0], pts[1]);

        //trace("++\n");
        if (*points_num + 1 < points_cap)
            points[(*points_num)++] = (Vector2) { pts[0], pts[1] };
    }
    rlEnd();
}


static void stage_shot_draw(struct Stage_shot *st) {
	NSVGshape *shape;
	NSVGpath  *path;
    NSVGimage *img = st->nsvg_img;

    BeginMode2D(st->cam);

    Vector2 points[1024] = {};
    int points_cap = sizeof(points) / sizeof(points[0]);
    int points_num = 0;

    const float px = 1.;

    trace("stage_shot_draw:\n");

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
    trace("stage_shot_draw: points_num %d\n", points_num);
    for (int i = 0; i + 1 < points_num; i++) {
        DrawLineEx(points[i], points[i + 1], thick, color);
    }

    EndMode2D();
}

static void stage_shot_shutdown(struct Stage_shot *st) {
    trace("stage_shot_shutdown:\n");
    nsvgDelete(st->nsvg_img);
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
