#ifndef FREEHAND_DRAWING_H
#define FREEHAND_DRAWING_H

#include "element.h"

#define INITIAL_DRAWING_COLOR {0.0, 0.8, 0.0, 1.0};

typedef struct {
  Element base;
  GArray *points;
  int stroke_width;
} FreehandDrawing;

void freehand_drawing_add_point(FreehandDrawing *drawing, int x, int y);
FreehandDrawing* freehand_drawing_create(ElementPosition position,
                                        ElementColor stroke_color,
                                        int stroke_width,
                                        CanvasData *data);

#endif
