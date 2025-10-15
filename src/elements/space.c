#include "space.h"
#include "element.h"
#include "../canvas/canvas.h"
#include "../canvas/canvas_core.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration
static void space_element_get_connection_point(Element *element, int point, int *cx, int *cy);

void space_element_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  SpaceElement *space_elem = (SpaceElement*)element;

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw rectangle with rounded corners
  double radius = 20.0; // Corner radius
  double x = element->x;
  double y = element->y;
  double width = element->width;
  double height = element->height;

  // Create rounded rectangle path
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + width - radius, y + radius, radius, -G_PI_2, 0);
  cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, G_PI_2);
  cairo_arc(cr, x + radius, y + height - radius, radius, G_PI_2, G_PI);
  cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI_2);
  cairo_close_path(cr);

  if (is_selected) {
    cairo_set_source_rgb(cr, 0.7, 0.7, 1.0);  // Light blue when selected
  } else {
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
  }
  cairo_fill_preserve(cr);

  cairo_set_source_rgb(cr, 0.2, 0.2, 0.8);  // Dark blue border
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);

  // Draw space name
  PangoLayout *layout = pango_cairo_create_layout(cr);
  PangoFontDescription *font_desc = pango_font_description_from_string(space_elem->font_description);
  pango_layout_set_font_description(layout, font_desc);
  pango_font_description_free(font_desc);

  pango_layout_set_text(layout, space_elem->text, -1);

  // Set text width to fit within the rounded rectangle (with padding)
  pango_layout_set_width(layout, (width - 40) * PANGO_SCALE); // 20px padding on each side
  pango_layout_set_alignment(layout, element_get_pango_alignment(space_elem->alignment));
  pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

  // Apply strikethrough if enabled
  if (space_elem->strikethrough) {
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *strike_attr = pango_attr_strikethrough_new(TRUE);
    pango_attr_list_insert(attrs, strike_attr);
    pango_layout_set_attributes(layout, attrs);
    pango_attr_list_unref(attrs);
  }

  int text_width, text_height;
  pango_layout_get_pixel_size(layout, &text_width, &text_height);

  int padding = 20;
  double text_x = x + padding;
  double text_y;

  // Calculate vertical position based on alignment
  VerticalAlign valign = element_get_vertical_alignment(space_elem->alignment);
  switch (valign) {
  case VALIGN_TOP:
    text_y = y + padding;
    break;
  case VALIGN_BOTTOM:
    text_y = y + height - padding - text_height;
    break;
  case VALIGN_CENTER:
  default:
    text_y = y + (height - text_height) / 2;
    break;
  }

  cairo_move_to(cr, text_x, text_y);
  cairo_set_source_rgba(cr, space_elem->text_r, space_elem->text_g, space_elem->text_b, space_elem->text_a);
  pango_cairo_show_layout(cr, layout);

  g_object_unref(layout);

  // Restore cairo state
  cairo_restore(cr);

  // Draw connection points and rotation handle when selected
  if (is_selected) {
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      space_element_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
      cairo_fill(cr);
    }
    element_draw_rotation_handle(element, cr);
  }
}

static void space_element_get_connection_point(Element *element, int point, int *cx, int *cy) {
  int unrotated_x, unrotated_y;
  // Same as note elements
  switch(point) {
  case 0: unrotated_x = element->x + element->width/2; unrotated_y = element->y; break;
  case 1: unrotated_x = element->x + element->width; unrotated_y = element->y + element->height/2; break;
  case 2: unrotated_x = element->x + element->width/2; unrotated_y = element->y + element->height; break;
  case 3: unrotated_x = element->x; unrotated_y = element->y + element->height/2; break;
  }

  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = unrotated_x - center_x;
    double dy = unrotated_y - center_y;
    double angle_rad = element->rotation_degrees * M_PI / 180.0;
    *cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    *cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  } else {
    *cx = unrotated_x;
    *cy = unrotated_y;
  }
}

int space_element_pick_resize_handle(Element *element, int x, int y) {
  // Apply inverse rotation to mouse coordinates if element is rotated
  double rotated_cx = x;
  double rotated_cy = y;
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = x - center_x;
    double dy = y - center_y;
    double angle_rad = -element->rotation_degrees * M_PI / 180.0;
    rotated_cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    rotated_cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  }

  int size = 8;
  struct { int px, py; } handles[4] = {
    {element->x, element->y},
    {element->x + element->width, element->y},
    {element->x + element->width, element->y + element->height},
    {element->x, element->y + element->height}
  };

  // For small elements (< 50px), only show bottom-right handle (index 2)
  gboolean is_small = (element->width < 50 || element->height < 50);

  for (int i = 0; i < 4; i++) {
    if (is_small && i != 2) continue; // Skip all but bottom-right for small elements

    if (abs(rotated_cx - handles[i].px) <= size && abs(rotated_cy - handles[i].py) <= size) {
      return i;
    }
  }
  return -1;
}

int space_element_pick_connection_point(Element *element, int x, int y) {
  // Hide connection points for small elements (< 100px on either dimension)
  if (element->width < 100 || element->height < 100) {
    return -1;
  }

  for (int i = 0; i < 4; i++) {
    int px, py;
    space_element_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    if (dx * dx + dy * dy < 100) return i;
  }
  return -1;
}

void space_element_update_position(Element *element, int x, int y, int z) {
  // Update position with z coordinate
  element->x = x;
  element->y = y;
  element->z = z;
}

void space_element_update_size(Element *element, int width, int height) {
  // Update size
  element->width = width;
  element->height = height;
}

void space_element_free(Element *element) {
  SpaceElement *space_elem = (SpaceElement*)element;
  g_free(space_elem->text);
  g_free(space_elem->font_description);
  g_free(space_elem->alignment);
  g_free(space_elem);
}

void space_name_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  SpaceElement *space_elem = (SpaceElement*)user_data;

  if (response_id == GTK_RESPONSE_OK) {
    GtkWidget *entry = gtk_dialog_get_widget_for_response(dialog, GTK_RESPONSE_OK);
    const char *new_name = gtk_editable_get_text(GTK_EDITABLE(entry));

    // Update space name
    g_free(space_elem->text);
    space_elem->text = g_strdup(new_name);

    // Queue redraw
    if (space_elem->base.canvas_data && space_elem->base.canvas_data->drawing_area) {
      gtk_widget_queue_draw(space_elem->base.canvas_data->drawing_area);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

void space_element_start_editing(Element *element, GtkWidget *overlay) {
  return;
}

static ElementVTable space_element_vtable = {
  .draw = space_element_draw,
  .get_connection_point = space_element_get_connection_point,
  .pick_resize_handle = space_element_pick_resize_handle,
  .pick_connection_point = space_element_pick_connection_point,
  .start_editing = space_element_start_editing,
  .update_position = space_element_update_position,
  .update_size = space_element_update_size,
  .free = space_element_free
};

SpaceElement* space_element_create(ElementPosition position,
                                   ElementColor bg_color,
                                   ElementSize size,
                                   ElementText text,
                                   CanvasData *data) {
  SpaceElement *space_elem = g_new0(SpaceElement, 1);
  space_elem->base.type = ELEMENT_SPACE;
  space_elem->base.vtable = &space_element_vtable;
  space_elem->base.x = position.x;
  space_elem->base.y = position.y;
  space_elem->base.z = position.z;
  space_elem->base.bg_r = bg_color.r;
  space_elem->base.bg_g = bg_color.g;
  space_elem->base.bg_b = bg_color.b;
  space_elem->base.bg_a = bg_color.a;
  space_elem->base.width = size.width;
  space_elem->base.height = size.height;
  space_elem->base.canvas_data = data;
  space_elem->text = g_strdup(text.text);
  space_elem->text_r = text.text_color.r;
  space_elem->text_g = text.text_color.g;
  space_elem->text_b = text.text_color.b;
  space_elem->text_a = text.text_color.a;
  space_elem->font_description = g_strdup(text.font_description);
  space_elem->strikethrough = text.strikethrough;
  space_elem->alignment = g_strdup(text.alignment ? text.alignment : "center");

  return space_elem;
}