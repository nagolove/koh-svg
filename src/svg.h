#pragma once

#include "nanosvg.h"
#include "raylib.h"

typedef void (*SvgParseFunc)(float x, float y, void *udata);
typedef void (*SvgParseSegmentFunc)(Vector2 p1, Vector2 p2, void *udata);

void svg_parse(NSVGimage *img, float px, SvgParseFunc func, void *udata);
void svg_parse_segments(
    NSVGimage *img, float scale,
    Vector2 *points, 
    int *points_num,
    int points_cap,
    SvgParseSegmentFunc func,
    void *udata
);
