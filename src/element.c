#include "element.h"
#include "paper_note.h"
#include "note.h"
#include "model.h"
#include "canvas.h"
#include "canvas_core.h"

void element_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  if (element && element->vtable && element->vtable->draw) {
    element->vtable->draw(element, cr, is_selected);
  }
}

void element_get_connection_point(Element *element, int point, int *cx, int *cy) {
  if (element && element->vtable && element->vtable->get_connection_point) {
    element->vtable->get_connection_point(element, point, cx, cy);
  }
}

int element_pick_resize_handle(Element *element, int x, int y) {
  if (element && element->vtable && element->vtable->pick_resize_handle) {
    return element->vtable->pick_resize_handle(element, x, y);
  }
  return -1;
}

int element_pick_connection_point(Element *element, int x, int y) {
  if (element && element->vtable && element->vtable->pick_connection_point) {
    return element->vtable->pick_connection_point(element, x, y);
  }
  return -1;
}

void element_start_editing(Element *element, GtkWidget *overlay) {
  if (element && element->vtable && element->vtable->start_editing) {
    element->vtable->start_editing(element, overlay);
  }
}

void element_update_position(Element *element, int x, int y, int z) {
  Model* model = element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_position(model, model_element, x, y, z);

  element->x = x;
  element->y = y;
  element->z = z;
  if (element && element->vtable && element->vtable->update_position) {
    element->vtable->update_position(element, x, y, z);
  }
}

void element_update_size(Element *element, int width, int height) {
  element->width = width;
  element->height = height;

  Model* model = element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_size(model, model_element, width, height);

  if (element && element->vtable && element->vtable->update_size) {
    element->vtable->update_size(element, width, height);
  }
}

void element_free(Element *element) {
  if (element && element->vtable && element->vtable->free) {
    element->vtable->free(element);
  }
}

void element_bring_to_front(Element *element, int *next_z) {
  element->z = (*next_z)++;
  Model* model = element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_position(model, model_element, model_element->position->x, model_element->position->y, element->z);
}

const char* element_get_type_name(ElementType type) {
  switch (type) {
    case ELEMENT_NOTE: return "Note";
    case ELEMENT_PAPER_NOTE: return "Paper";
    case ELEMENT_INLINE_TEXT: return "Text";
    case ELEMENT_SPACE: return "Space";
    case ELEMENT_CONNECTION: return "Connection";
    case ELEMENT_FREEHAND_DRAWING: return "Drawing";
    case ELEMENT_SHAPE: return "Shape";
    case ELEMENT_MEDIA_FILE: return "Media";
    default: return "Unknown";
  }
}

void element_get_rotation_handle_position(Element *element, int *hx, int *hy) {
  // Position rotation handle above the element center
  double center_x = element->x + element->width / 2.0;
  double center_y = element->y + element->height / 2.0;
  double handle_distance = element->height / 2.0 + 30; // 30 pixels above element

  // Apply rotation to handle position
  double angle_rad = element->rotation_degrees * M_PI / 180.0;
  *hx = center_x + handle_distance * sin(angle_rad);
  *hy = center_y - handle_distance * cos(angle_rad);
}

int element_pick_rotation_handle(Element *element, int x, int y) {
  // Get rotation handle position in canvas coordinates
  int hx, hy;
  element_get_rotation_handle_position(element, &hx, &hy);

  // Check distance in canvas coordinates
  int dx = x - hx;
  int dy = y - hy;
  int distance = sqrt(dx * dx + dy * dy);

  return distance <= 8; // 8 pixel radius for picking
}

void element_draw_rotation_handle(Element *element, cairo_t *cr) {
  int hx, hy;
  element_get_rotation_handle_position(element, &hx, &hy);

  // Draw handle circle
  cairo_arc(cr, hx, hy, 8, 0, 2 * M_PI);
  cairo_set_source_rgba(cr, 0.3, 0.6, 1.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);

  // Draw rotation icon (circular arrow)
  cairo_arc(cr, hx, hy, 4, M_PI/4, M_PI * 1.75);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);
}

const char* element_alignment_to_string(TextAlignment alignment) {
  switch (alignment) {
    case TEXT_ALIGN_TOP_LEFT: return "top-left";
    case TEXT_ALIGN_TOP_CENTER: return "top-center";
    case TEXT_ALIGN_TOP_RIGHT: return "top-right";
    case TEXT_ALIGN_CENTER: return "center";
    case TEXT_ALIGN_BOTTOM_LEFT: return "bottom-left";
    case TEXT_ALIGN_BOTTOM_RIGHT: return "bottom-right";
    default: return "center";
  }
}

TextAlignment element_string_to_alignment(const char *alignment) {
  if (!alignment) return TEXT_ALIGN_CENTER;
  if (g_strcmp0(alignment, "top-left") == 0) return TEXT_ALIGN_TOP_LEFT;
  if (g_strcmp0(alignment, "top-center") == 0) return TEXT_ALIGN_TOP_CENTER;
  if (g_strcmp0(alignment, "top-right") == 0) return TEXT_ALIGN_TOP_RIGHT;
  if (g_strcmp0(alignment, "bottom-left") == 0) return TEXT_ALIGN_BOTTOM_LEFT;
  if (g_strcmp0(alignment, "bottom-right") == 0) return TEXT_ALIGN_BOTTOM_RIGHT;
  return TEXT_ALIGN_CENTER;
}

PangoAlignment element_get_pango_alignment(const char *alignment) {
  if (!alignment) return PANGO_ALIGN_CENTER;
  if (g_str_has_suffix(alignment, "left")) return PANGO_ALIGN_LEFT;
  if (g_str_has_suffix(alignment, "right")) return PANGO_ALIGN_RIGHT;
  return PANGO_ALIGN_CENTER;
}

VerticalAlign element_get_vertical_alignment(const char *alignment) {
  if (!alignment) return VALIGN_CENTER;
  if (g_str_has_prefix(alignment, "top-")) return VALIGN_TOP;
  if (g_str_has_prefix(alignment, "bottom-")) return VALIGN_BOTTOM;
  return VALIGN_CENTER;
}
