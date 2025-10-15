#include "inline_text.h"
#include "dsl/dsl_runtime.h"
#include "canvas.h"
#include "canvas_core.h"
#include "canvas_actions.h"
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
  inline_text->editing = FALSE;

  inline_text->base.canvas_data = data;

  inline_text->text_r = text.text_color.r;
  inline_text->text_g = text.text_color.g;
  inline_text->text_b = text.text_color.b;
  inline_text->text_a = text.text_color.a;
  inline_text->font_description = g_strdup(text.font_description ? text.font_description : "Ubuntu Mono 12");
  inline_text->strikethrough = text.strikethrough;

  inline_text->layout = NULL;
  inline_text->text_view = NULL;
  inline_text->scrolled_window = NULL;

  // Update layout to calculate proper size based on text
  inline_text_update_layout(inline_text);

  return inline_text;
}

void inline_text_update_layout(InlineText *text) {
  if (!text) {
    return;
  }

  int old_width = text->base.width;
  int old_height = text->base.height;

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

  pango_layout_set_text(text->layout, text->text, -1);

  // Get text dimensions
  int text_width, text_height;
  pango_layout_get_pixel_size(text->layout, &text_width, &text_height);

  // Update element size to fit text with padding
  int padding = 8;
  text->base.width = MAX(text_width + padding * 2, text->min_width);
  text->base.height = MAX(text_height + padding * 2, 20);

  gboolean size_changed = (text->base.width != old_width) || (text->base.height != old_height);

  if (size_changed) {
    Element *element = (Element*)text;
    CanvasData *canvas_data = element->canvas_data;

    if (canvas_data && canvas_data->model) {
      ModelElement *model_element = model_get_by_visual(canvas_data->model, element);
      if (model_element) {
        model_update_size(canvas_data->model, model_element, text->base.width, text->base.height);
      }
    }

    if (canvas_data && canvas_data->quadtree) {
      canvas_rebuild_quadtree(canvas_data);
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void inline_text_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  InlineText *text = (InlineText*)element;

  // Update layout before drawing
  inline_text_update_layout(text);

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw background only if not editing and has background color
  if (!text->editing && element->bg_a > 0.1) {
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_fill(cr);
  }

  // Draw border only when selected (not when editing)
  if (is_selected && !text->editing) {
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5); // Gray border when selected
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_stroke(cr);
  }

  // Draw text only when not editing (to avoid double text with TextView)
  if (text->layout && !text->editing) {
    // Apply strikethrough if enabled
    if (text->strikethrough) {
      PangoAttrList *attrs = pango_attr_list_new();
      PangoAttribute *strike_attr = pango_attr_strikethrough_new(TRUE);
      pango_attr_list_insert(attrs, strike_attr);
      pango_layout_set_attributes(text->layout, attrs);
      pango_attr_list_unref(attrs);
    }

    cairo_set_source_rgba(cr, text->text_r, text->text_g, text->text_b, text->text_a);
    cairo_move_to(cr, element->x + 8, element->y + 8);
    pango_cairo_show_layout(cr, text->layout);
  }

  // Restore cairo state before drawing selection UI
  cairo_restore(cr);

  // Draw connection points when selected
  if (is_selected && !text->editing) {
    cairo_save(cr);
    if (element->rotation_degrees != 0.0) {
      double center_x = element->x + element->width / 2.0;
      double center_y = element->y + element->height / 2.0;
      cairo_translate(cr, center_x, center_y);
      cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
      cairo_translate(cr, -center_x, -center_y);
    }

    for (int i = 0; i < 4; i++) {
      int cx, cy;
      inline_text_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.6);
      cairo_fill(cr);
    }
    cairo_restore(cr);

    // Draw rotation handle (without rotation)
    element_draw_rotation_handle(element, cr);
  }
}

void inline_text_get_connection_point(Element *element, int point, int *cx, int *cy) {
  InlineText *text = (InlineText*)element;
  // Update layout to ensure width/height are current
  inline_text_update_layout(text);

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
  // Hide connection points for small elements (< 100px on either dimension)
  if (element->width < 100 || element->height < 100) {
    return -1;
  }

  // x, y are already in canvas coordinates - no conversion needed
  for (int i = 0; i < 4; i++) {
    int px, py;
    inline_text_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    if (dx * dx + dy * dy < 64) return i; // 8 pixel radius
  }
  return -1;
}

static void on_text_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
  InlineText *text = (InlineText*)user_data;

  // Get current text to calculate new size
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *current_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  // Temporarily update the text for layout calculation
  char *old_text = text->text;
  text->text = current_text;

  // Recalculate size based on current text
  inline_text_update_layout(text);

  // Update TextView size to match the new dimensions
  if (text->scrolled_window) {
    gtk_widget_set_size_request(text->scrolled_window,
                                text->base.width + 20,
                                text->base.height + 20);
  }

  // Restore original text and free temporary text
  text->text = old_text;
  g_free(current_text);

  // Redraw to update the visual bounds
  gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
}

gboolean inline_text_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  InlineText *text = (InlineText*)user_data;

  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) {
      // Ctrl+Enter does nothing special for inline text (allow newline)
      return FALSE;
    } else {
      // Regular Enter finishes editing
      inline_text_finish_editing((Element*)text);
      return TRUE;
    }
  } else if (keyval == GDK_KEY_Tab) {
    // Tab finishes current element and creates a new one
    inline_text_finish_editing((Element*)text);
    canvas_on_add_text(NULL, text->base.canvas_data);
    return TRUE;
  }
  return FALSE;
}

void inline_text_start_editing(Element *element, GtkWidget *overlay) {
  InlineText *text = (InlineText*)element;
  text->editing = TRUE;

  if (!text->text_view) {
    // Create scrolled window with minimal scrollbar policy
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_EXTERNAL,  // No horizontal scrollbar
                                  GTK_POLICY_NEVER);    // No vertical scrollbar

    // Create text view
    text->text_view = gtk_text_view_new();

    // Make text view transparent
    gtk_widget_set_name(text->text_view, "transparent-textview");

    // Set font to match inline text using CSS
    PangoFontDescription *font_desc = pango_font_description_from_string(text->font_description);
    const char *family = pango_font_description_get_family(font_desc);
    int size = pango_font_description_get_size(font_desc);

    // Create combined CSS for font, transparency, and precise positioning
    gchar *combined_css = g_strdup_printf(
      "#transparent-textview { "
      "font-family: %s; "
      "font-size: %dpt; "
      "background-color: transparent; "
      "padding: 0px; "
      "margin: 0px; "
      "border: none; "
      "} "
      "#transparent-textview text { "
      "background-color: transparent; "
      "padding: 0px; "
      "margin: 0px; "
      "}",
      family ? family : "Ubuntu Mono",
      size > 0 ? size / PANGO_SCALE : 12);

    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, combined_css, -1);

    GtkStyleContext *style_context = gtk_widget_get_style_context(text->text_view);
    gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    pango_font_description_free(font_desc);
    g_object_unref(css_provider);
    g_free(combined_css);

    // Configure text view properties - no wrapping so it expands horizontally
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text->text_view), GTK_WRAP_NONE);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text->text_view), FALSE);

    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text->text_view);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), scrolled_window);
    gtk_widget_set_halign(scrolled_window, GTK_ALIGN_START);
    gtk_widget_set_valign(scrolled_window, GTK_ALIGN_START);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(inline_text_on_textview_key_press), text);
    gtk_widget_add_controller(text->text_view, key_controller);

    // Connect text buffer change handler for dynamic resizing
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text->text_view));
    g_signal_connect(buffer, "changed", G_CALLBACK(on_text_buffer_changed), text);

    // Store the scrolled window reference
    text->scrolled_window = scrolled_window;
  }

  // Update layout to get current size
  inline_text_update_layout(text);

  // Set initial size with some padding
  gtk_widget_set_size_request(text->scrolled_window, element->width + 20, element->height + 20);
  gtk_widget_set_hexpand(text->scrolled_window, TRUE);
  gtk_widget_set_vexpand(text->scrolled_window, FALSE);

  // Convert canvas coordinates to screen coordinates
  // Canvas text is drawn at element->x + 8, element->y + 8 (see inline_text_draw)
  int screen_x, screen_y;
  canvas_canvas_to_screen(element->canvas_data, element->x + 8, element->y + 8, &screen_x, &screen_y);

  // Position TextView exactly where canvas text would be drawn
  gtk_widget_set_margin_start(text->scrolled_window, screen_x);
  gtk_widget_set_margin_top(text->scrolled_window, screen_y);

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text->text_view));
  gtk_text_buffer_set_text(buffer, text->text, -1);

  // Position cursor at end
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer, &end_iter);
  gtk_text_buffer_place_cursor(buffer, &end_iter);

  gtk_widget_show(text->scrolled_window);
  gtk_widget_grab_focus(text->text_view);
}

void inline_text_finish_editing(Element *element) {
  InlineText *text = (InlineText*)element;
  if (!text->editing || !text->text_view) return;

  // Save old text for undo
  char *old_text = g_strdup(text->text);

  // Extract text from text view
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  // Update text
  g_free(text->text);
  text->text = g_strdup(new_text);

  // Update layout with new text BEFORE changing editing state to prevent jumping
  inline_text_update_layout(text);

  // Update model
  Model *model = text->base.canvas_data->model;
  ModelElement *model_element = model_get_by_visual(model, element);
  if (model_element) {
    undo_manager_push_text_action(text->base.canvas_data->undo_manager, model_element, old_text, text->text);
    model_update_text(model, model_element, text->text);
  }

  text->editing = FALSE;

  // Hide the text view
  if (text->scrolled_window) {
    gtk_widget_hide(text->scrolled_window);
  }

  // Sync with model and redraw
  if (text->base.canvas_data && text->base.canvas_data->drawing_area) {
    canvas_sync_with_model(text->base.canvas_data);
    gtk_widget_queue_draw(text->base.canvas_data->drawing_area);
    gtk_widget_grab_focus(text->base.canvas_data->drawing_area);

    // Notify DSL runtime about updated text (e.g., for bindings)
    dsl_runtime_inline_text_updated(text->base.canvas_data, element, text->text);
  }

  g_free(old_text);
  g_free(new_text);
}

void inline_text_update_position(Element *element, int x, int y, int z) {
  element->x = x;
  element->y = y;
  element->z = z;

  InlineText *text = (InlineText*)element;
  if (text->editing && text->scrolled_window) {
    // Update text view position to match canvas text position
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, element->x + 8, element->y + 8, &screen_x, &screen_y);
    gtk_widget_set_margin_start(text->scrolled_window, screen_x);
    gtk_widget_set_margin_top(text->scrolled_window, screen_y);
  }
}

void inline_text_update_size(Element *element, int width, int height) {
  // Inline text auto-sizes, but we can set a minimum width
  InlineText *text = (InlineText*)element;
  text->min_width = MAX(width, 50);
  inline_text_update_layout(text);

  // Update text view size if editing
  if (text->editing && text->scrolled_window) {
    gtk_widget_set_size_request(text->scrolled_window, element->width + 20, element->height + 20);
  }
}

void inline_text_free(Element *element) {
  InlineText *text = (InlineText*)element;
  if (text->text) g_free(text->text);
  if (text->font_description) g_free(text->font_description);
  if (text->layout) g_object_unref(text->layout);

  // Clean up GTK widgets if they exist
  if (text->scrolled_window) {
    gtk_widget_unparent(text->scrolled_window);
  }

  g_free(text);
}
