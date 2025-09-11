#include "image_note.h"
#include "canvas.h"
#include "canvas_core.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>
#include <math.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static ElementVTable image_note_vtable = {
  .draw = image_note_draw,
  .get_connection_point = image_note_get_connection_point,
  .pick_resize_handle = image_note_pick_resize_handle,
  .pick_connection_point = image_note_pick_connection_point,
  .start_editing = image_note_start_editing,
  .update_position = image_note_update_position,
  .update_size = image_note_update_size,
  .free = image_note_free
};

ImageNote* image_note_create(ElementPosition position,
                             ElementColor bg_color,
                             ElementSize size,
                             const unsigned char *image_data,
                             int image_size,
                             const char *text,
                             CanvasData *data) {
  ImageNote *image_note = g_new0(ImageNote, 1);
  image_note->base.type = ELEMENT_IMAGE_NOTE;
  image_note->base.vtable = &image_note_vtable;
  image_note->base.x = position.x;
  image_note->base.y = position.y;
  image_note->base.z = position.z;

  image_note->base.bg_r = bg_color.r;
  image_note->base.bg_g = bg_color.g;
  image_note->base.bg_b = bg_color.b;
  image_note->base.bg_a = bg_color.a;

  image_note->base.width = size.width;
  image_note->base.height = size.height;
  image_note->base.canvas_data = data;
  image_note->text = g_strdup(text ? text : "");
  image_note->text_view = NULL;
  image_note->editing = FALSE;

  if (image_data && image_size > 0) {
    GInputStream *stream = g_memory_input_stream_new_from_data(
                                                               image_data, image_size, NULL);
    image_note->pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    g_object_unref(stream);
  }

  return image_note;
}

void image_note_on_text_view_focus_leave(GtkEventController *controller, gpointer user_data) {
  ImageNote *image_note = (ImageNote*)user_data;
  image_note_finish_editing((Element*)image_note);
}

gboolean image_note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  ImageNote *image_note = (ImageNote*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) return FALSE;
    image_note_finish_editing((Element*)image_note);
    return TRUE;
  }
  return FALSE;
}

void image_note_finish_editing(Element *element) {
  ImageNote *image_note = (ImageNote*)element;
  if (!image_note->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(image_note->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  g_free(image_note->text);
  image_note->text = new_text;

  Model* model = image_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_text(model, model_element, new_text);

  image_note->editing = FALSE;
  gtk_widget_hide(image_note->text_view);

  // Queue redraw using the stored canvas data
  if (image_note->base.canvas_data && image_note->base.canvas_data->drawing_area) {
    canvas_sync_with_model(image_note->base.canvas_data);
    gtk_widget_queue_draw(image_note->base.canvas_data->drawing_area);
  }
}

void image_note_start_editing(Element *element, GtkWidget *overlay) {
  ImageNote *image_note = (ImageNote*)element;
  image_note->editing = TRUE;

  if (!image_note->text_view) {
    image_note->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(image_note->text_view), GTK_WRAP_WORD);

    // Make text view smaller for the bottom-right corner
    gtk_widget_set_size_request(image_note->text_view,
                                element->width / 3,
                                element->height / 6);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), image_note->text_view);
    gtk_widget_set_halign(image_note->text_view, GTK_ALIGN_START);
    gtk_widget_set_valign(image_note->text_view, GTK_ALIGN_START);

    GtkEventController *focus_controller = gtk_event_controller_focus_new();
    g_signal_connect(focus_controller, "leave", G_CALLBACK(image_note_on_text_view_focus_leave), image_note);
    gtk_widget_add_controller(image_note->text_view, focus_controller);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(image_note_on_textview_key_press), image_note);
    gtk_widget_add_controller(image_note->text_view, key_controller);
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(image_note->text_view));
  gtk_text_buffer_set_text(buffer, image_note->text, -1);

  // Position the text view in the bottom right corner of the actual image
  if (image_note->pixbuf) {
    int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
    int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

    double scale_x = element->width / (double)pixbuf_width;
    double scale_y = element->height / (double)pixbuf_height;
    double scale = MIN(scale_x, scale_y);

    int draw_width = (int) pixbuf_width * scale;
    int draw_height = (int) pixbuf_height * scale;
    int draw_x = element->x + (element->width - draw_width) / 2;
    int draw_y = element->y + (element->height - draw_height) / 2;

    int text_view_width, text_view_height;
    gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data,
                            draw_x + draw_width - text_view_width - 10,
                            draw_y + draw_height - text_view_height - 10,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(image_note->text_view, screen_x);
    gtk_widget_set_margin_top(image_note->text_view, screen_y);
  } else {
    // Fallback to element bounds if no pixbuf
    int text_view_width, text_view_height;
    gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data,
                            element->x + element->width - text_view_width - 10,
                            element->y + element->height - text_view_height - 10,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(image_note->text_view, screen_x);
    gtk_widget_set_margin_top(image_note->text_view, screen_y);
  }

  gtk_widget_show(image_note->text_view);
  gtk_widget_grab_focus(image_note->text_view);
}

void image_note_update_position(Element *element, int x, int y, int z) {
  ImageNote *image_note = (ImageNote*)element;
  element->x = x;
  element->y = y;
  element->z = z;

  // Update text view position if editing
  if (image_note->text_view && image_note->editing) {
    if (image_note->pixbuf) {
      int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
      int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

      double scale_x = element->width / (double)pixbuf_width;
      double scale_y = element->height / (double)pixbuf_height;
      double scale = MIN(scale_x, scale_y);

      int draw_width = pixbuf_width * scale;
      int draw_height = pixbuf_height * scale;
      int draw_x = element->x + (element->width - draw_width) / 2;
      int draw_y = element->y + (element->height - draw_height) / 2;

      int text_view_width, text_view_height;
      gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

      int screen_draw_x, screen_draw_y;
      canvas_canvas_to_screen(element->canvas_data, draw_x, draw_y, &screen_draw_x, &screen_draw_y);

      gtk_widget_set_margin_start(image_note->text_view,
                                  screen_draw_x + draw_width - text_view_width - 10);
      gtk_widget_set_margin_top(image_note->text_view,
                           screen_draw_y + draw_height - text_view_height - 10);
    } else {
      // Fallback to element bounds if no pixbuf
      int text_view_width, text_view_height;
      gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

      int screen_x, screen_y;
      canvas_canvas_to_screen(element->canvas_data,
                              element->x + element->width - text_view_width - 10,
                              element->y + element->height - text_view_height - 10,
                              &screen_x, &screen_y);

      gtk_widget_set_margin_start(image_note->text_view, screen_x);
      gtk_widget_set_margin_top(image_note->text_view, screen_y);
    }
  }

  // Update model
  Model* model = image_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_position(model, model_element, x, y, z);
}

void image_note_update_size(Element *element, int width, int height) {
  ImageNote *image_note = (ImageNote*)element;
  element->width = width;
  element->height = height;

  if (image_note->text_view) {
    // Resize text view proportionally
    gtk_widget_set_size_request(image_note->text_view, width / 3, height / 6);

    // Reposition text view if editing
    if (image_note->editing) {
      if (image_note->pixbuf) {
        int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
        int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

        double scale_x = element->width / (double)pixbuf_width;
        double scale_y = element->height / (double)pixbuf_height;
        double scale = MIN(scale_x, scale_y);

        int draw_width = pixbuf_width * scale;
        int draw_height = pixbuf_height * scale;
        int draw_x = element->x + (element->width - draw_width) / 2;
        int draw_y = element->y + (element->height - draw_height) / 2;

        int text_view_width, text_view_height;
        gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

        gtk_widget_set_margin_start(image_note->text_view, draw_x + draw_width - text_view_width - 10);
        gtk_widget_set_margin_top(image_note->text_view, draw_y + draw_height - text_view_height - 10);
      } else {
        // Fallback to element bounds if no pixbuf
        int text_view_width, text_view_height;
        gtk_widget_get_size_request(image_note->text_view, &text_view_width, &text_view_height);

        gtk_widget_set_margin_start(image_note->text_view, element->x + element->width - text_view_width - 10);
        gtk_widget_set_margin_top(image_note->text_view, element->y + element->height - text_view_height - 10);
      }
    }
  }

  // Update model
  Model* model = image_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_size(model, model_element, width, height);
}

void image_note_free(Element *element) {
  ImageNote *image_note = (ImageNote*)element;
  if (image_note->pixbuf) g_object_unref(image_note->pixbuf);
  if (image_note->text) g_free(image_note->text);
  if (image_note->text_view && GTK_IS_WIDGET(image_note->text_view) && gtk_widget_get_parent(image_note->text_view)) {
    gtk_widget_unparent(image_note->text_view);
  }
  g_free(image_note);
}

void image_note_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  ImageNote *image_note = (ImageNote*)element;

  if (!image_note->pixbuf) return;

  // Draw the image scaled to fit the entire element
  int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

  // Calculate scaling to fit the entire element
  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  // Save the current state
  cairo_save(cr);

  // Create a rectangle for the image and clip to it
  cairo_rectangle(cr, draw_x, draw_y, draw_width, draw_height);
  cairo_clip(cr);

  // Translate to the draw position
  cairo_translate(cr, draw_x, draw_y);

  // Scale the image
  cairo_scale(cr, scale, scale);

  // Draw the pixbuf at the origin (since we already translated)
  gdk_cairo_set_source_pixbuf(cr, image_note->pixbuf, 0, 0);
  cairo_paint(cr);

  // Restore the state (back to original coordinate system)
  cairo_restore(cr);

  if (!image_note->editing && image_note->text) {
    cairo_save(cr);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans 12");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, image_note->text, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    int text_x = draw_x + draw_width - text_width - 10;
    int text_y = draw_y + draw_height - text_height - 10;

    // Draw white text
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, text_x, text_y);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_restore(cr);
  }

  // Draw resize handles and connection points (only when selected)
  if (is_selected) {
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.8);
    cairo_set_line_width(cr, 2.0);

    // Draw resize handles at the corners of the actual image
    int handles[4][2] = {
      {draw_x, draw_y},
      {draw_x + draw_width, draw_y},
      {draw_x + draw_width, draw_y + draw_height},
      {draw_x, draw_y + draw_height}
    };

    for (int i = 0; i < 4; i++) {
      cairo_rectangle(cr, handles[i][0] - 4, handles[i][1] - 4, 8, 8);
      cairo_fill(cr);
    }

    // Draw connection points on the actual image borders
    int connection_points[4][2] = {
      {draw_x + draw_width/2, draw_y},                    // top center
      {draw_x + draw_width, draw_y + draw_height/2},      // right center
      {draw_x + draw_width/2, draw_y + draw_height},      // bottom center
      {draw_x, draw_y + draw_height/2}                    // left center
    };

    for (int i = 0; i < 4; i++) {
      cairo_arc(cr, connection_points[i][0], connection_points[i][1], 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
      cairo_fill(cr);
    }
  }
}

void image_note_get_connection_point(Element *element, int point, int *cx, int *cy) {
  ImageNote *image_note = (ImageNote*)element;

  if (!image_note->pixbuf) {
    // Fallback to element bounds if no pixbuf
    switch(point) {
    case 0: *cx = element->x + element->width/2; *cy = element->y; break;
    case 1: *cx = element->x + element->width; *cy = element->y + element->height/2; break;
    case 2: *cx = element->x + element->width/2; *cy = element->y + element->height; break;
    case 3: *cx = element->x; *cy = element->y + element->height/2; break;
    }
    return;
  }

  // Calculate actual image position and size
  int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  // Return connection points on the actual image borders
  switch(point) {
  case 0: *cx = draw_x + draw_width/2; *cy = draw_y; break;                    // top center
  case 1: *cx = draw_x + draw_width; *cy = draw_y + draw_height/2; break;      // right center
  case 2: *cx = draw_x + draw_width/2; *cy = draw_y + draw_height; break;      // bottom center
  case 3: *cx = draw_x; *cy = draw_y + draw_height/2; break;                   // left center
  }
}

int image_note_pick_resize_handle(Element *element, int x, int y) {
  ImageNote *image_note = (ImageNote*)element;
  int cx, cy;
  canvas_screen_to_canvas(element->canvas_data, x, y, &cx, &cy);

  if (!image_note->pixbuf) {
    // Fallback to element bounds if no pixbuf
    int size = 8;
    int handles[4][2] = {
      {element->x, element->y},
      {element->x + element->width, element->y},
      {element->x + element->width, element->y + element->height},
      {element->x, element->y + element->height}
    };

    for (int i = 0; i < 4; i++) {
      if (abs(cx - handles[i][0]) <= size && abs(cy - handles[i][1]) <= size) {
        return i;
      }
    }
    return -1;
  }

  // Calculate actual image position and size
  int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  int size = 8;
  int handles[4][2] = {
    {draw_x, draw_y},
    {draw_x + draw_width, draw_y},
    {draw_x + draw_width, draw_y + draw_height},
    {draw_x, draw_y + draw_height}
  };

  for (int i = 0; i < 4; i++) {
    if (abs(cx - handles[i][0]) <= size && abs(cy - handles[i][1]) <= size) {
      return i;
    }
  }
  return -1;
}

int image_note_pick_connection_point(Element *element, int x, int y) {
  ImageNote *image_note = (ImageNote*)element;
  int cx, cy;
  canvas_screen_to_canvas(element->canvas_data, x, y, &cx, &cy);

  if (!image_note->pixbuf) {
    // Fallback to element bounds if no pixbuf
    for (int i = 0; i < 4; i++) {
      int px, py;
      image_note_get_connection_point(element, i, &px, &py);
      int dx = cx - px, dy = cy - py;
      if (dx * dx + dy * dy < 36) return i;
    }
    return -1;
  }

  // Calculate actual image position and size
  int pixbuf_width = gdk_pixbuf_get_width(image_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(image_note->pixbuf);

  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  int connection_points[4][2] = {
    {draw_x + draw_width/2, draw_y},                    // top center
    {draw_x + draw_width, draw_y + draw_height/2},      // right center
    {draw_x + draw_width/2, draw_y + draw_height},      // bottom center
    {draw_x, draw_y + draw_height/2}                    // left center
  };

  for (int i = 0; i < 4; i++) {
    int dx = cx - connection_points[i][0], dy = cy - connection_points[i][1];
    if (dx * dx + dy * dy < 36) return i;
  }
  return -1;
}
