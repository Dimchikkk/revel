#ifndef CONNECTION_H
#define CONNECTION_H

#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  double x, y;
} Vec2;

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_div(Vec2 v, double scalar);


typedef struct {
  Element base;
  Element *from;
  int from_point;
  Element *to;
  int to_point;
} Connection;

Connection* connection_create(Element *from, int from_point,
                              Element *to, int to_point,
                              ElementColor bg_color,
                              int z,
                              CanvasData *data);
void connection_draw(Element *element, cairo_t *cr, gboolean is_selected);
void connection_get_connection_point(Element *element, int point, int *cx, int *cy);
int connection_pick_resize_handle(Element *element, int x, int y);
int connection_pick_connection_point(Element *element, int x, int y);
void connection_draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip);
void connection_draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos);
void connection_parallel_arrow_mid(Vec2 start, Vec2 end, int start_pos, int end_pos, Vec2 *mid1, Vec2 *mid2);

#endif
