#pragma once

#include "nanosvg.h"

typedef void (*SvgParseFunc)(float x, float y, void *udata);

void svg_parse(NSVGimage *img, SvgParseFunc func, void *udata);
