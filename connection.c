#include "connection.h"
#include <math.h>

void connection_draw(Connection *conn, cairo_t *cr) {
    int x1, y1, x2, y2;
    note_get_connection_point(conn->from, conn->from_point, &x1, &y1);
    note_get_connection_point(conn->to, conn->to_point, &x2, &y2);

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 2);

    connection_draw_parallel_arrow(cr, (Vec2){x1, y1}, (Vec2){x2, y2},
                                  conn->from_point, conn->to_point);
}

void connection_draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip) {
    Vec2 direction = {tip.x - base.x, tip.y - base.y};
    double length = sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 1e-6) return;

    direction.x /= length;
    direction.y /= length;

    Vec2 perp1 = {-direction.y * 8, direction.x * 8};
    Vec2 perp2 = {direction.y * 8, -direction.x * 8};

    Vec2 back = {tip.x - direction.x * 12, tip.y - direction.y * 12};
    Vec2 head1 = {back.x + perp1.x, back.y + perp1.y};
    Vec2 head2 = {back.x + perp2.x, back.y + perp2.y};

    cairo_move_to(cr, tip.x, tip.y);
    cairo_line_to(cr, head1.x, head1.y);
    cairo_line_to(cr, head2.x, head2.y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

void connection_draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos) {
    Vec2 mid1, mid2;
    connection_parallel_arrow_mid(start, end, start_pos, end_pos, &mid1, &mid2);

    cairo_move_to(cr, start.x, start.y);
    cairo_line_to(cr, mid1.x, mid1.y);
    cairo_line_to(cr, mid2.x, mid2.y);
    cairo_line_to(cr, end.x, end.y);
    cairo_stroke(cr);

    connection_draw_arrow_head(cr, mid2, end);
}

void connection_parallel_arrow_mid(Vec2 start, Vec2 end, int start_pos, int end_pos, Vec2 *mid1, Vec2 *mid2) {
    Vec2 mid = vec2_div(vec2_add(start, end), 2.0);

    switch (start_pos) {
    case 0:
        switch (end_pos) {
        case 2:
            *mid1 = (Vec2){start.x, mid.y};
            *mid2 = (Vec2){end.x, mid.y};
            return;
        case 1: case 3:
            *mid1 = (Vec2){start.x, end.y};
            *mid2 = (Vec2){start.x, end.y};
            return;
        default:
            break;
        }
        break;

    case 2:
        switch (end_pos) {
        case 0:
            *mid1 = (Vec2){start.x, mid.y};
            *mid2 = (Vec2){end.x, mid.y};
            return;
        case 1: case 3:
            *mid1 = (Vec2){start.x, end.y};
            *mid2 = (Vec2){start.x, end.y};
            return;
        default:
            break;
        }
        break;

    case 3:
        switch (end_pos) {
        case 1:
            *mid1 = (Vec2){mid.x, start.y};
            *mid2 = (Vec2){mid.x, end.y};
            return;
        case 0: case 2:
            *mid1 = (Vec2){end.x, start.y};
            *mid2 = (Vec2){end.x, start.y};
            return;
        default:
            break;
        }
        break;

    case 1:
        switch (end_pos) {
        case 3:
            *mid1 = (Vec2){mid.x, start.y};
            *mid2 = (Vec2){mid.x, end.y};
            return;
        case 0: case 2:
            *mid1 = (Vec2){end.x, start.y};
            *mid2 = (Vec2){end.x, start.y};
            return;
        default:
            break;
        }
        break;
    }

    *mid1 = mid;
    *mid2 = mid;
}
