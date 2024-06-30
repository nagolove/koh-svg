#pragma once

#include "nanosvg.h"

typedef void (*SvgParseFunc)(float x, float y, void *udata);

void svg_parse(NSVGimage *img, float px, SvgParseFunc func, void *udata);
