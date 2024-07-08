#include "svg.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "koh_logger.h"

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
    SvgParseFunc func, void *udata
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
            x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1, func, udata
        ); 
        cubicBez(
            x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1, func, udata
        ); 
    } else {
        func(x4, y4, udata);
    }
}

void draw_path(
    float* pts, int npts, char closed, float tol, 
    SvgParseFunc func, void *udata
) {
    int i;

    for (i = 0; i < npts-1; i += 3) {
        float* p = &pts[i*2];
        cubicBez(
            p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0, func, udata
        );
    }

    if (closed) {
        func(pts[0], pts[1], udata);
    }
}

void svg_parse(
    NSVGimage *img,
    SvgParseFunc func,
    SvgParseFuncNextPath func_nextp,
    void *udata
) {
    assert(img);
    assert(func);

	NSVGshape *shape;
	NSVGpath  *path;

    const float px = 1.;

    trace("svg_parse:\n");

	for (shape = img->shapes; shape != NULL; shape = shape->next) {
		for (path = shape->paths; path != NULL; path = path->next) {
			draw_path(
                path->pts, path->npts, path->closed, px, func, udata
            );
            if (func_nextp)
                func_nextp(udata);
			//drawControlPts(path->pts, path->npts);
		}
	}
}

static void cubicBez2(
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
        cubicBez2(
            x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1,
            points, points_num, points_cap
        ); 
        cubicBez2(
            x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1,
            points, points_num, points_cap
        ); 
    } else {
        /*rlVertex2f(x4, y4);*/
            if (*points_num + 1 < points_cap)
                points[(*points_num)++] = (Vector2) { x4, y4, };
    }
}

static void draw_path2(
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
        cubicBez2(
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

void svg_parse_segments(
    NSVGimage *img, float scale,
    Vector2 *points, 
    int *points_num,
    int points_cap,
    SvgParseSegmentFunc func,
    void *udata
) {
    assert(img);
    assert(scale != 0.);

    assert(func);
    assert(points);
    assert(points_num);
    assert(points_num >= 0);
    assert(points_cap >= 0);
    int _points_num = 0;

    //trace("parse_svg_segments:\n");

    for (NSVGshape *shape = img->shapes; shape; shape = shape->next) {
        for (NSVGpath *path = shape->paths; path; path = path->next) {
            draw_path2(
                path->pts, path->npts, path->closed, 1.5, 
                scale,
                points, &_points_num, points_cap
            );
            //drawControlPts(path->pts, path->npts);

            //trace("stage_shot_draw: points_num %d\n", points_num);

            if (func) {
                for (int i = 0; i + 1 < _points_num; i++) {
                    func(points[i], points[i + 1], udata);
                }
                _points_num = 0;
            }
        }
    }

    // XXX: Действительные ли здесь данные о количестве? 
    // Ведь _points_num сбрасывается в 0
    if (points_num)
        *points_num = _points_num;
}
