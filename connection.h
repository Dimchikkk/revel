#ifndef CONNECTION_H
#define CONNECTION_H

#include "element.h"
#include "vector.h"

typedef struct {
    Element *from;
    int from_point;
    Element *to;
    int to_point;
    gboolean hidden;
} Connection;

void connection_draw(Connection *conn, cairo_t *cr);
void connection_draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip);
void connection_draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos);
void connection_parallel_arrow_mid(Vec2 start, Vec2 end, int start_pos, int end_pos, Vec2 *mid1, Vec2 *mid2);

#endif
