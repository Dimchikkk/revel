#include "inline_text.h"
#include "canvas.h"
#include "canvas_core.h"
#include "model.h"
#include "undo_manager.h"
#include <math.h>
#include <string.h>

static ElementVTable inline_text_vtable = {
  .draw = inline_text_draw,
  .get_connection_point = inline_text_get_connection_point,
  .pick_resize_handle = inline_text_pick_resize_handle,
  .pick_connection_point = inline_text_pick_connection_point,
  .start_editing = inline_text_start_editing,
  .update_position = inline_text_update_position,
  .update_size = inline_text_update_size,
  .free = inline_text_free
};

InlineText* inline_text_create(ElementPosition position,
                               ElementColor bg_color,
                               ElementSize size,
                               ElementText text,
                               CanvasData *data) {
  InlineText *inline_text = g_new0(InlineText, 1);
  inline_text->base.type = ELEMENT_INLINE_TEXT;
  inline_text->base.vtable = &inline_text_vtable;
  inline_text->base.x = position.x;
  inline_text->base.y = position.y;
  inline_text->base.z = position.z;

  inline_text->base.bg_r = bg_color.r;
  inline_text->base.bg_g = bg_color.g;
  inline_text->base.bg_b = bg_color.b;
  inline_text->base.bg_a = bg_color.a;

  inline_text->base.width = MAX(size.width, 50);  // Minimum width
  inline_text->base.height = MAX(size.height, 20); // Minimum height
  inline_text->min_width = 50;

  inline_text->text = g_strdup(text.text ? text.text : "");
  inline_text->edit_text = g_strdup(inline_text->text);
  inline_text->editing = FALSE;
  inline_text->cursor_pos = 0;
  inline_text->cursor_x = 0;
  inline_text->cursor_y = 0;

  inline_text->base.canvas_data = data;

  inline_text->text_r = text.text_color.r;
  inline_text->text_g = text.text_color.g;
  inline_text->text_b = text.text_color.b;
  inline_text->text_a = text.text_color.a;
  inline_text->font_description = g_strdup(text.font_description ? text.font_description : "Ubuntu Mono 12");

  inline_text->layout = NULL;

  return inline_text;
}

void inline_text_update_layout(InlineText *text) {
  if (text->layout) {
    g_object_unref(text->layout);
  }

  // Create a temporary surface to get a cairo context for layout creation
  cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t *cr = cairo_create(surface);

  text->layout = pango_cairo_create_layout(cr);
  PangoFontDescription *font_desc = pango_font_description_from_string(text->font_description);
  pango_layout_set_font_description(text->layout, font_desc);
  pango_font_description_free(font_desc);

  const char *display_text = text->editing ? text->edit_text : text->text;
  pango_layout_set_text(text->layout, display_text, -1);

  // Get text dimensions
  int text_width, text_height;
  pango_layout_get_pixel_size(text->layout, &text_width, &text_height);

  // Update element size to fit text with padding
  int padding = 8;
  text->base.width = MAX(text_width + padding * 2, text->min_width);
  text->base.height = MAX(text_height + padding * 2, 20);

  // Update cursor position if editing
  if (text->editing) {
    PangoRectangle cursor_rect;
    pango_layout_get_cursor_pos(text->layout, text->cursor_pos, &cursor_rect, NULL);
    text->cursor_x = text->base.x + padding + PANGO_PIXELS(cursor_rect.x);
    text->cursor_y = text->base.y + padding + PANGO_PIXELS(cursor_rect.y);
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void inline_text_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  InlineText *text = (InlineText*)element;

  // Update layout before drawing
  inline_text_update_layout(text);

  // Draw background only if not editing and has background color
  if (!text->editing && element->bg_a > 0.1) {
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_fill(cr);
  }

  // Draw border when editing or selected
  if (text->editing || is_selected) {
    if (text->editing) {
      cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.8); // Blue border when editing
    } else {
      cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5); // Gray border when selected
    }
    cairo_set_line_width(cr, text->editing ? 2.0 : 1.0);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_stroke(cr);
  }

  // Draw text
  if (text->layout) {
    cairo_set_source_rgba(cr, text->text_r, text->text_g, text->text_b, text->text_a);
    cairo_move_to(cr, element->x + 8, element->y + 8);
    pango_cairo_show_layout(cr, text->layout);
  }

  // Draw cursor when editing
  if (text->editing) {
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, text->cursor_x, text->cursor_y);
    cairo_line_to(cr, text->cursor_x, text->cursor_y + 16); // Cursor height
    cairo_stroke(cr);
  }

  // Draw connection points when selected
  if (is_selected && !text->editing) {
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      inline_text_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.6);
      cairo_fill(cr);
    }
  }
}

void inline_text_get_connection_point(Element *element, int point, int *cx, int *cy) {
  switch(point) {
  case 0: *cx = element->x + element->width/2; *cy = element->y; break;
  case 1: *cx = element->x + element->width; *cy = element->y + element->height/2; break;
  case 2: *cx = element->x + element->width/2; *cy = element->y + element->height; break;
  case 3: *cx = element->x; *cy = element->y + element->height/2; break;
  }
}

int inline_text_pick_resize_handle(Element *element, int x, int y) {
  // No resize handles for inline text - it auto-sizes
  return -1;
}

int inline_text_pick_connection_point(Element *element, int x, int y) {
  int cx, cy;
  canvas_screen_to_canvas(element->canvas_data, x, y, &cx, &cy);

  for (int i = 0; i < 8; i++) {
    int px, py;
    inline_text_get_connection_point(element, i, &px, &py);
    int dx = cx - px, dy = cy - py;
    if (dx * dx + dy * dy < 64) return i; // 8 pixel radius
  }
  return -1;
}

void inline_text_start_editing(Element *element, GtkWidget *overlay) {
  InlineText *text = (InlineText*)element;
  text->editing = TRUE;

  // Set cursor to end of text
  text->cursor_pos = g_utf8_strlen(text->edit_text, -1);

  // Grab keyboard focus to ensure input goes to the canvas
  gtk_widget_grab_focus(element->canvas_data->drawing_area);

  // Update layout and redraw
  inline_text_update_layout(text);
  gtk_widget_queue_draw(element->canvas_data->drawing_area);
}

void inline_text_finish_editing(Element *element) {
  InlineText *text = (InlineText*)element;
  if (!text->editing) return;

  // Save old text for undo
  char *old_text = g_strdup(text->text);

  // Update text
  g_free(text->text);
  text->text = g_strdup(text->edit_text);

  // Update model
  Model *model = text->base.canvas_data->model;
  ModelElement *model_element = model_get_by_visual(model, element);
  if (model_element) {
    undo_manager_push_text_action(text->base.canvas_data->undo_manager, model_element, old_text, text->text);
    model_update_text(model, model_element, text->text);
  }

  text->editing = FALSE;

  // Update layout and redraw
  inline_text_update_layout(text);
  if (text->base.canvas_data && text->base.canvas_data->drawing_area) {
    canvas_sync_with_model(text->base.canvas_data);
    gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
  }

  g_free(old_text);
}

void inline_text_update_position(Element *element, int x, int y, int z) {
  element->x = x;
  element->y = y;
  element->z = z;

  InlineText *text = (InlineText*)element;
  if (text->editing) {
    inline_text_update_layout(text);
  }
}

void inline_text_update_size(Element *element, int width, int height) {
  // Inline text auto-sizes, but we can set a minimum width
  InlineText *text = (InlineText*)element;
  text->min_width = MAX(width, 50);
  inline_text_update_layout(text);
}

void inline_text_free(Element *element) {
  InlineText *text = (InlineText*)element;
  if (text->text) g_free(text->text);
  if (text->edit_text) g_free(text->edit_text);
  if (text->font_description) g_free(text->font_description);
  if (text->layout) g_object_unref(text->layout);
  g_free(text);
}

// Text editing functions
void inline_text_insert_char(InlineText *text, const char *utf8_char) {
  if (!text->editing) return;

  // Convert cursor position to byte offset
  const char *text_start = text->edit_text;
  const char *cursor_ptr = g_utf8_offset_to_pointer(text_start, text->cursor_pos);
  int byte_offset = cursor_ptr - text_start;

  // Create new text with inserted character
  GString *new_text = g_string_new("");
  g_string_append_len(new_text, text->edit_text, byte_offset);
  g_string_append(new_text, utf8_char);
  g_string_append(new_text, cursor_ptr);

  g_free(text->edit_text);
  text->edit_text = g_string_free(new_text, FALSE);

  // Move cursor forward
  text->cursor_pos++;

  inline_text_update_layout(text);
  gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
}

void inline_text_delete_char(InlineText *text, gboolean backward) {
  if (!text->editing) return;

  if (backward && text->cursor_pos > 0) {
    // Backspace - delete character before cursor
    const char *text_start = text->edit_text;
    const char *cursor_ptr = g_utf8_offset_to_pointer(text_start, text->cursor_pos);
    const char *prev_ptr = g_utf8_prev_char(cursor_ptr);

    int start_offset = prev_ptr - text_start;

    GString *new_text = g_string_new("");
    g_string_append_len(new_text, text->edit_text, start_offset);
    g_string_append(new_text, cursor_ptr);

    g_free(text->edit_text);
    text->edit_text = g_string_free(new_text, FALSE);

    text->cursor_pos--;
  } else if (!backward && text->cursor_pos < g_utf8_strlen(text->edit_text, -1)) {
    // Delete - delete character after cursor
    const char *text_start = text->edit_text;
    const char *cursor_ptr = g_utf8_offset_to_pointer(text_start, text->cursor_pos);
    const char *next_ptr = g_utf8_next_char(cursor_ptr);

    int byte_offset = cursor_ptr - text_start;

    GString *new_text = g_string_new("");
    g_string_append_len(new_text, text->edit_text, byte_offset);
    g_string_append(new_text, next_ptr);

    g_free(text->edit_text);
    text->edit_text = g_string_free(new_text, FALSE);
  }

  inline_text_update_layout(text);
  gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
}

void inline_text_move_cursor(InlineText *text, int direction) {
  if (!text->editing) return;

  int text_len = g_utf8_strlen(text->edit_text, -1);

  if (direction < 0 && text->cursor_pos > 0) {
    text->cursor_pos--;
  } else if (direction > 0 && text->cursor_pos < text_len) {
    text->cursor_pos++;
  }

  inline_text_update_layout(text);
  gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
}

void inline_text_set_cursor_from_position(InlineText *text, int x, int y) {
  if (!text->editing || !text->layout) return;

  // Convert screen coordinates to text-relative coordinates
  int text_x = x - text->base.x - 8; // Account for padding
  int text_y = y - text->base.y - 8;

  // Find cursor position from coordinates
  int index, trailing;
  pango_layout_xy_to_index(text->layout, text_x * PANGO_SCALE, text_y * PANGO_SCALE, &index, &trailing);

  // Convert byte index to character position
  const char *text_ptr = text->edit_text + index;
  text->cursor_pos = g_utf8_pointer_to_offset(text->edit_text, text_ptr) + trailing;

  inline_text_update_layout(text);
  gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
}
