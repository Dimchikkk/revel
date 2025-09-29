#ifndef CANVAS_PLACEMENT_H
#define CANVAS_PLACEMENT_H

#include "canvas.h"

// Find closest empty space from viewport center for placing new elements
void canvas_find_empty_position(CanvasData *data, int width, int height, int *out_x, int *out_y);

#endif