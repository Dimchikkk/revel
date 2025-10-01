#ifndef SHAPE_H
#define SHAPE_H

#include "element.h"

typedef enum {
  SHAPE_CIRCLE,
  SHAPE_RECTANGLE,
  SHAPE_TRIANGLE,
  SHAPE_CYLINDER_VERTICAL,
  SHAPE_CYLINDER_HORIZONTAL,
  SHAPE_DIAMOND,
  SHAPE_ROUNDED_RECTANGLE,
  SHAPE_LINE,
  SHAPE_ARROW
} ShapeType;

typedef enum {
  STROKE_STYLE_SOLID = 0,
  STROKE_STYLE_DASHED = 1,
  STROKE_STYLE_DOTTED = 2
} StrokeStyle;

typedef enum {
  FILL_STYLE_SOLID = 0,
  FILL_STYLE_HACHURE = 1,
  FILL_STYLE_CROSS_HATCH = 2
} FillStyle;

typedef struct {
  Element base;
  ShapeType shape_type;
  int stroke_width;
  gboolean filled;
  StrokeStyle stroke_style;
  FillStyle fill_style;
  double stroke_r, stroke_g, stroke_b, stroke_a;
  char *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;
  char* alignment;
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  gboolean editing;
  gboolean has_line_points;
  double line_start_u;
  double line_start_v;
  double line_end_u;
  double line_end_v;
} Shape;

Shape* shape_create(ElementPosition position,
                   ElementSize size,
                   ElementColor color,
                   int stroke_width,
                   ShapeType shape_type,
                   gboolean filled,
                   ElementText text,
                   ElementShape shape_config,
                   const ElementDrawing *drawing_config,
                   CanvasData *data);
void shape_free(Element *element);

void shape_finish_editing(Element *element);

#endif
