#include "note.h"
#include "canvas.h"
#include "canvas_core.h"
#include "element.h"
#include <pango/pangocairo.h>
#include <math.h>
#include "undo_manager.h"

static ElementVTable note_vtable = {
  .draw = note_draw,
  .get_connection_point = note_get_connection_point,
  .pick_resize_handle = note_pick_resize_handle,
  .pick_connection_point = note_pick_connection_point,
  .start_editing = note_start_editing,
  .update_position = note_update_position,
  .update_size = note_update_size,
  .free = note_free
};

Note* note_create(ElementPosition position,
                  ElementColor bg_color,
                  ElementSize size,
                  ElementText text,
                  CanvasData *data) {
  Note *note = g_new0(Note, 1);
  note->base.type = ELEMENT_NOTE;
  note->base.vtable = &note_vtable;
  note->base.x = position.x;
  note->base.y = position.y;
  note->base.z = position.z;

  note->base.bg_r = bg_color.r;
  note->base.bg_g = bg_color.g;
  note->base.bg_b = bg_color.b;
  note->base.bg_a = bg_color.a;

  note->base.width = size.width;
  note->base.height = size.height;
  note->text = g_strdup(text.text);
  note->text_view = NULL;
  note->editing = FALSE;
  note->base.canvas_data = data;

  note->text_r = text.text_color.r;
  note->text_g = text.text_color.g;
  note->text_b = text.text_color.b;
  note->text_a = text.text_color.a;
  note->font_description = g_strdup(text.font_description);
  note->strikethrough = text.strikethrough;
  note->alignment = g_strdup(text.alignment ? text.alignment : "center");

  return note;
}

static void note_update_text_view_position(Note *note) {
  if (!note->scrolled_window || !GTK_IS_WIDGET(note->scrolled_window)) return;

  int screen_x, screen_y;
  canvas_canvas_to_screen(note->base.canvas_data,
                          note->base.x, note->base.y,
                          &screen_x, &screen_y);
  gtk_widget_set_margin_start(note->scrolled_window, screen_x - 10);
  gtk_widget_set_margin_top(note->scrolled_window, screen_y - 10);
  gtk_widget_set_size_request(note->scrolled_window,
                              note->base.width + 20,
                              note->base.height + 20);
}

gboolean note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  Note *note = (Note*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) {
      // Ctrl+Enter inserts a newline
      GtkTextView *text_view = GTK_TEXT_VIEW(note->text_view);
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_insert(buffer, &iter, "\n", 1);

      return TRUE; // Handled - prevent default behavior
    } else {
      // Enter finishes editing
      note_finish_editing((Element*)note);
      return TRUE;
    }
  }
  return FALSE;
}

void note_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  Note *note = (Note*)element;
  double radius = 12.0;
  double x = element->x;
  double y = element->y;
  double width = element->width;
  double height = element->height;

  if (note->editing) {
    note_update_text_view_position(note);
  }

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    // Rotate around element center
    double center_x = x + width / 2.0;
    double center_y = y + height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Create rounded rectangle path
  cairo_new_path(cr);
  cairo_move_to(cr, x + radius, y);
  cairo_line_to(cr, x + width - radius, y);
  cairo_arc(cr, x + width - radius, y + radius, radius, -M_PI/2, 0);
  cairo_line_to(cr, x + width, y + height - radius);
  cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, M_PI/2);
  cairo_line_to(cr, x + radius, y + height);
  cairo_arc(cr, x + radius, y + height - radius, radius, M_PI/2, M_PI);
  cairo_line_to(cr, x, y + radius);
  cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3*M_PI/2);
  cairo_close_path(cr);

  // Fill with white background
  if (is_selected) {
    cairo_set_source_rgb(cr, 0.9, 0.9, 1.0); // Light blue when selected
  } else {
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
  }
  cairo_fill_preserve(cr);

  // Draw border
  cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);

  // Restore cairo state before drawing selection UI
  cairo_restore(cr);

  // Draw connection points and rotation handle (only when selected)
  if (is_selected) {
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      note_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
      cairo_fill(cr);
    }

    // Draw rotation handle (not rotated with element)
    element_draw_rotation_handle(element, cr);
  }

  // Save state again for text
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw text if not editing
  if (!note->editing) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string(note->font_description);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, note->text, -1);
    pango_layout_set_width(layout, (element->width - 20) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, element_get_pango_alignment(note->alignment));

    // Apply strikethrough if enabled
    if (note->strikethrough) {
      PangoAttrList *attrs = pango_attr_list_new();
      PangoAttribute *strike_attr = pango_attr_strikethrough_new(TRUE);
      pango_attr_list_insert(attrs, strike_attr);
      pango_layout_set_attributes(layout, attrs);
      pango_attr_list_unref(attrs);
    }

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    cairo_set_source_rgba(cr, note->text_r, note->text_g, note->text_b, note->text_a);

    int padding = 10;
    int available_height = element->height - (2 * padding);
    int text_x = element->x + padding;
    int text_y;

    // Calculate vertical position based on alignment
    VerticalAlign valign = element_get_vertical_alignment(note->alignment);
    switch (valign) {
    case VALIGN_TOP:
      text_y = element->y + padding;
      break;
    case VALIGN_BOTTOM:
      text_y = element->y + element->height - padding - text_height;
      if (text_y < element->y + padding) text_y = element->y + padding;
      break;
    case VALIGN_CENTER:
    default:
      text_y = element->y + padding + (available_height - text_height) / 2;
      if (text_y < element->y + padding) text_y = element->y + padding;
      break;
    }

    if (text_height <= available_height) {
      cairo_move_to(cr, text_x, text_y);
      pango_cairo_show_layout(cr, layout);
    } else {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_height(layout, available_height * PANGO_SCALE);
      cairo_move_to(cr, text_x, element->y + padding);
      pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
  }

  // Restore cairo state
  cairo_restore(cr);
}

void note_get_connection_point(Element *element, int point, int *cx, int *cy) {
  int unrotated_x, unrotated_y;
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

int note_pick_resize_handle(Element *element, int x, int y) {
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

  for (int i = 0; i < 4; i++) {
    if (abs(rotated_cx - handles[i].px) <= size && abs(rotated_cy - handles[i].py) <= size) {
      return i;
    }
  }
  return -1;
}

int note_pick_connection_point(Element *element, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int px, py;
    note_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    if (dx * dx + dy * dy < 100) return i;
  }
  return -1;
}

void note_start_editing(Element *element, GtkWidget *overlay) {
  Note *note = (Note*)element;
  note->editing = TRUE;

  if (!note->text_view) {
    // Create scrolled window
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    // Create text view
    note->text_view = gtk_text_view_new();

    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), note->text_view);

    // Set size with some padding for scrollbars
    gtk_widget_set_size_request(scrolled_window, element->width + 20, element->height + 20);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), scrolled_window);
    gtk_widget_set_halign(scrolled_window, GTK_ALIGN_START);
    gtk_widget_set_valign(scrolled_window, GTK_ALIGN_START);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, element->x, element->y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(scrolled_window, screen_x - 10); // Adjust for padding
    gtk_widget_set_margin_top(scrolled_window, screen_y - 10);   // Adjust for padding

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(note_on_textview_key_press), note);
    gtk_widget_add_controller(note->text_view, key_controller);

    // Store the scrolled window reference if needed for later access
    note->scrolled_window = scrolled_window;
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
  gtk_text_buffer_set_text(buffer, note->text, -1);

  gtk_widget_show(note->scrolled_window ? note->scrolled_window : note->text_view);
  gtk_widget_grab_focus(note->text_view);
}

void note_finish_editing(Element *element) {
  Note *note = (Note*)element;
  if (!note->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  char* old_text = g_strdup(note->text);
  g_free(note->text);
  note->text = new_text;

  Model* model = note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  undo_manager_push_text_action(note->base.canvas_data->undo_manager, model_element, old_text, new_text);
  model_update_text(model, model_element, new_text);

  note->editing = FALSE;

  // Hide the scrolled window instead of the text view
  if (note->scrolled_window) {
    gtk_widget_hide(note->scrolled_window);
  } else {
    gtk_widget_hide(note->text_view);
  }

  // Queue redraw using the stored canvas data
  if (note->base.canvas_data && note->base.canvas_data->drawing_area) {
    canvas_sync_with_model(note->base.canvas_data);
    gtk_widget_queue_draw(note->base.canvas_data->drawing_area);
    gtk_widget_grab_focus(note->base.canvas_data->drawing_area);
  }
}

void note_update_position(Element *element, int x, int y, int z) {
  Note *note = (Note*)element;
  element->x = x;
  element->y = y;
  element->z = z;
  if (note->scrolled_window) {
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, x, y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(note->scrolled_window, screen_x - 10);
    gtk_widget_set_margin_top(note->scrolled_window, screen_y - 10);
  }
  if (note->editing) {
    note_update_text_view_position(note);
  }
}

void note_update_size(Element *element, int width, int height) {
  Note *note = (Note*)element;
  element->width = width;
  element->height = height;
  if (note->scrolled_window) {
    gtk_widget_set_size_request(note->scrolled_window, width + 20, height + 20);
  }
  if (note->editing) {
    note_update_text_view_position(note);
  }
}

void note_free(Element *element) {
  Note *note = (Note*)element;
  if (note->text) g_free(note->text);
  if (note->font_description) g_free(note->font_description);
  if (note->alignment) g_free(note->alignment);
  if (note->scrolled_window && GTK_IS_WIDGET(note->scrolled_window) &&
      gtk_widget_get_parent(note->scrolled_window)) {
    gtk_widget_unparent(note->scrolled_window);
  }
  g_free(note);
}
