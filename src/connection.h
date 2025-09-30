#ifndef CONNECTION_H
#define CONNECTION_H

#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  double x, y;
} Vec2;

typedef struct {
  double x;
  double y;
  double width;
  double height;
} ConnectionRect;

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_div(Vec2 v, double scalar);

typedef enum {
  CONNECTION_TYPE_PARALLEL,
  CONNECTION_TYPE_STRAIGHT
} ConnectionType;

typedef enum {
  ARROWHEAD_NONE,
  ARROWHEAD_SINGLE,
  ARROWHEAD_DOUBLE
} ArrowheadType;

typedef struct {
  Element base;
  Element *from;
  int from_point;
  Element *to;
  int to_point;
  ConnectionType connection_type;
  ArrowheadType arrowhead_type;
} Connection;

Connection* connection_create(ElementConnection connection_config,
                              ElementColor bg_color,
                              int z,
                              CanvasData *data);
void connection_draw(Element *element, cairo_t *cr, gboolean is_selected);
void connection_get_connection_point(Element *element, int point, int *cx, int *cy);
int connection_pick_resize_handle(Element *element, int x, int y);
int connection_pick_connection_point(Element *element, int x, int y);
void connection_draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip);
void connection_draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos);
void connection_draw_straight_arrow(cairo_t *cr, Vec2 start, Vec2 end);
void connection_parallel_arrow_mid(Vec2 start, Vec2 end, int start_pos, int end_pos, Vec2 *mid1, Vec2 *mid2);
void connection_draw_arrow_head_at_pos(cairo_t *cr, Vec2 base, Vec2 tip, ArrowheadType type);
void connection_determine_optimal_points(ConnectionRect from_rect,
                                         ConnectionRect to_rect,
                                         int *from_point,
                                         int *to_point);

#endif
