#include "connection.h"
#include "element.h"
#include <math.h>

static ElementVTable connection_vtable = {
  .draw = connection_draw,
  .get_connection_point = connection_get_connection_point,
  .pick_resize_handle = connection_pick_resize_handle,
  .pick_connection_point = connection_pick_connection_point,
  .start_editing = NULL, // Connections don't support editing
  .update_position = NULL, // Position is determined by connected elements
  .update_size = NULL,
  .free = NULL // Connections are freed by canvas
};

Vec2 vec2_add(Vec2 a, Vec2 b) {
  return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_div(Vec2 v, double scalar) {
  return (Vec2){v.x / scalar, v.y / scalar};
}

void connection_determine_optimal_points(ConnectionRect from_rect,
                                         ConnectionRect to_rect,
                                         int *from_point,
                                         int *to_point) {
  if (!from_point || !to_point) return;

  double from_center_x = from_rect.x + from_rect.width / 2.0;
  double from_center_y = from_rect.y + from_rect.height / 2.0;
  double to_center_x = to_rect.x + to_rect.width / 2.0;
  double to_center_y = to_rect.y + to_rect.height / 2.0;

  double dx = to_center_x - from_center_x;
  double dy = to_center_y - from_center_y;

  double angle = atan2(dy, dx) * 180.0 / M_PI;
  if (angle < 0) angle += 360.0;

  if (angle >= 45.0 && angle < 135.0) {
    *from_point = 2;
    *to_point = 0;
  } else if (angle >= 135.0 && angle < 225.0) {
    *from_point = 3;
    *to_point = 1;
  } else if (angle >= 225.0 && angle < 315.0) {
    *from_point = 0;
    *to_point = 2;
  } else {
    *from_point = 1;
    *to_point = 3;
  }
}

Connection* connection_create(ElementConnection connection_config,
                              ElementColor bg_color,
                              int z,
                              CanvasData *data){
  Connection *conn = g_new0(Connection, 1);
  conn->base.type = ELEMENT_CONNECTION;
  conn->base.vtable = &connection_vtable;
  conn->base.z = z;
  conn->base.bg_r = bg_color.r;
  conn->base.bg_g = bg_color.g;
  conn->base.bg_b = bg_color.b;
  conn->base.bg_a = bg_color.a;
  conn->from = connection_config.from_element;
  conn->from_point = connection_config.from_point;
  conn->to = connection_config.to_element;
  conn->to_point = connection_config.to_point;
  conn->connection_type = connection_config.connection_type;
  conn->arrowhead_type = connection_config.arrowhead_type;
  // Calculate initial position and size based on connected elements
  int x1, y1, x2, y2;
  element_get_connection_point(connection_config.from_element, connection_config.from_point, &x1, &y1);
  element_get_connection_point(connection_config.to_element, connection_config.to_point, &x2, &y2);

  conn->base.x = MIN(x1, x2);
  conn->base.y = MIN(y1, y2);
  conn->base.width = ABS(x2 - x1);
  conn->base.height = ABS(y2 - y1);
  conn->base.canvas_data = data;


  return conn;
}

void connection_draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip) {
  connection_draw_arrow_head_at_pos(cr, base, tip, ARROWHEAD_SINGLE);
}

void connection_draw_arrow_head_at_pos(cairo_t *cr, Vec2 base, Vec2 tip, ArrowheadType type) {
  if (type == ARROWHEAD_NONE) return;

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

  // Draw arrowhead at tip
  cairo_move_to(cr, tip.x, tip.y);
  cairo_line_to(cr, head1.x, head1.y);
  cairo_line_to(cr, head2.x, head2.y);
  cairo_close_path(cr);
  cairo_fill(cr);

  // For double arrowhead, draw another at the base
  if (type == ARROWHEAD_DOUBLE) {
    Vec2 reverse_direction = {-direction.x, -direction.y};
    Vec2 reverse_perp1 = {-reverse_direction.y * 8, reverse_direction.x * 8};
    Vec2 reverse_perp2 = {reverse_direction.y * 8, -reverse_direction.x * 8};

    Vec2 base_back = {base.x - reverse_direction.x * 12, base.y - reverse_direction.y * 12};
    Vec2 base_head1 = {base_back.x + reverse_perp1.x, base_back.y + reverse_perp1.y};
    Vec2 base_head2 = {base_back.x + reverse_perp2.x, base_back.y + reverse_perp2.y};

    cairo_move_to(cr, base.x, base.y);
    cairo_line_to(cr, base_head1.x, base_head1.y);
    cairo_line_to(cr, base_head2.x, base_head2.y);
    cairo_close_path(cr);
    cairo_fill(cr);
  }
}

void connection_draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos) {
  Vec2 mid1, mid2;
  connection_parallel_arrow_mid(start, end, start_pos, end_pos, &mid1, &mid2);

  cairo_move_to(cr, start.x, start.y);
  cairo_line_to(cr, mid1.x, mid1.y);
  cairo_line_to(cr, mid2.x, mid2.y);
  cairo_line_to(cr, end.x, end.y);
  cairo_stroke(cr);
}

void connection_draw_straight_arrow(cairo_t *cr, Vec2 start, Vec2 end) {
  cairo_move_to(cr, start.x, start.y);
  cairo_line_to(cr, end.x, end.y);
  cairo_stroke(cr);
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

void connection_update_bounds(Element *element) {
  Connection *conn = (Connection*)element;
  if (!conn->from || !conn->to) return;

  int x1, y1, x2, y2;
  element_get_connection_point(conn->from, conn->from_point, &x1, &y1);
  element_get_connection_point(conn->to, conn->to_point, &x2, &y2);

  // Update connection position and size based on current element positions
  conn->base.x = MIN(x1, x2);
  conn->base.y = MIN(y1, y2);
  conn->base.width = ABS(x2 - x1);
  conn->base.height = ABS(y2 - y1);
}

void connection_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  Connection *conn = (Connection*)element;

  // Update bounds before drawing
  connection_update_bounds(element);

  int x1, y1, x2, y2;
  element_get_connection_point(conn->from, conn->from_point, &x1, &y1);
  element_get_connection_point(conn->to, conn->to_point, &x2, &y2);

  // Draw connection line
  if (is_selected) {
    cairo_set_source_rgb(cr, 0.0, 0.4, 1.0); // Blue when selected
    cairo_set_line_width(cr, 3);
  } else {
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
    cairo_set_line_width(cr, 2);
  }

  // Draw connection based on type
  Vec2 start = {x1, y1};
  Vec2 end = {x2, y2};

  switch (conn->connection_type) {
  case CONNECTION_TYPE_PARALLEL:
    connection_draw_parallel_arrow(cr, start, end, conn->from_point, conn->to_point);
    break;
  case CONNECTION_TYPE_STRAIGHT:
    connection_draw_straight_arrow(cr, start, end);
    break;
  }

  // Draw arrowheads based on type
  if (conn->connection_type == CONNECTION_TYPE_PARALLEL) {
    Vec2 mid1, mid2;
    connection_parallel_arrow_mid(start, end, conn->from_point, conn->to_point, &mid1, &mid2);

    // Draw arrowhead at end point
    connection_draw_arrow_head_at_pos(cr, mid2, end,
                                      conn->arrowhead_type == ARROWHEAD_DOUBLE ? ARROWHEAD_SINGLE : conn->arrowhead_type);

    // For double arrowheads, also draw arrowhead at start point
    if (conn->arrowhead_type == ARROWHEAD_DOUBLE) {
      connection_draw_arrow_head_at_pos(cr, mid1, start, ARROWHEAD_SINGLE);
    }
  } else {
    connection_draw_arrow_head_at_pos(cr, start, end, conn->arrowhead_type);
  }

  // Control points removed - no longer drawn
}

void connection_get_connection_point(Element *element, int point, int *cx, int *cy) {
  Connection *conn = (Connection*)element;
  // Connections don't have their own connection points, return midpoint
  *cx = conn->base.x + conn->base.width / 2;
  *cy = conn->base.y + conn->base.height / 2;
}

int connection_pick_resize_handle(Element *element, int x, int y) {
  // Connections can't be resized
  return -1;
}

int connection_pick_connection_point(Element *element, int x, int y) {
  // Connections don't have connection points to pick
  return -1;
}

