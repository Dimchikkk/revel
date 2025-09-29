#ifndef SHAPE_H
#define SHAPE_H

#include "element.h"

typedef enum {
  SHAPE_CIRCLE,
  SHAPE_RECTANGLE,
  SHAPE_TRIANGLE,
  SHAPE_CYLINDER_VERTICAL,
  SHAPE_CYLINDER_HORIZONTAL,
  SHAPE_DIAMOND
} ShapeType;

typedef struct {
  Element base;
  ShapeType shape_type;
  int stroke_width;
  gboolean filled;
  char *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  gboolean editing;
} Shape;

Shape* shape_create(ElementPosition position,
                   ElementSize size,
                   ElementColor color,
                   int stroke_width,
                   ShapeType shape_type,
                   gboolean filled,
                   ElementText text,
                   ElementShape shape_config,
                   CanvasData *data);
void shape_free(Element *element);

void shape_finish_editing(Element *element);

#endif