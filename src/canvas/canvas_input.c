#include "canvas_input.h"
#include "canvas_core.h"
#include "canvas_actions.h"
#include "canvas_search.h"
#include "canvas_spaces.h"
#include "canvas_space_select.h"
#include "../elements/element.h"
#include "../model.h"
#include "../elements/paper_note.h"
#include "../elements/note.h"
#include "../elements/media_note.h"
#include "../elements/inline_text.h"
#include "../elements/connection.h"
#include "../elements/space.h"
#include <pango/pangocairo.h>
#include <gtk/gtkdialog.h>
#include <math.h>
#include "../undo_manager.h"
#include "../dsl/dsl_executor.h"
#include "../elements/freehand_drawing.h"
#include "../elements/shape.h"
#include "canvas_shape_dialog.h"
#include "canvas_font_dialog.h"
#include "canvas_clone_dialog.h"
#include <graphene.h>
#include "../dsl/dsl_runtime.h"


typedef struct {
  const char *shortcut;
  const char *description;
} ShortcutInfo;

static gboolean hide_shortcuts_notification(gpointer user_data) {
  GtkWidget *box = GTK_WIDGET(user_data);
  gtk_widget_set_visible(box, FALSE);
  return G_SOURCE_REMOVE;
}

void canvas_show_shortcuts_dialog(CanvasData *data) {
  if (!data || !data->overlay) {
    return;
  }

  // Create a box to hold the grid
  GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(container, GTK_ALIGN_START);
  gtk_widget_set_margin_top(container, 20);
  gtk_widget_set_margin_start(container, 20);
  gtk_widget_set_margin_end(container, 20);

  // Create grid for two columns (keys and descriptions)
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 4);

  // Keys column
  const char *keys =
    "Ctrl+N\n"
    "Ctrl+Shift+N\n"
    "Ctrl+Shift+P\n"
    "Ctrl+Shift+S\n"
    "Ctrl+O\n"
    "Ctrl+S\n"
    "Ctrl+E\n"
    "Ctrl+R\n"
    "Ctrl+D\n"
    "Ctrl+L\n"
    "Ctrl+V\n"
    "Ctrl+C\n"
    "Ctrl+Z\n"
    "Ctrl+Y\n"
    "Ctrl+A\n"
    "Ctrl+Plus\n"
    "Ctrl+Minus\n"
    "Ctrl+Right\n"
    "Ctrl+Left\n"
    "Delete\n"
    "Backspace\n"
    "Ctrl+J\n"
    "Ctrl+T\n"
    "Ctrl+Shift+T\n"
    "Ctrl+Click\n"
    "Enter\n"
    "Tab";

  // Descriptions column
  const char *descriptions =
    "Create inline text\n"
    "Create rich text\n"
    "Create paper note\n"
    "Create nested space\n"
    "Open shape library\n"
    "Open search\n"
    "Open DSL executor\n"
    "Open AI chat\n"
    "Toggle drawing mode\n"
    "Reset view (center, zoom 100%)\n"
    "Paste from clipboard\n"
    "Copy selected elements\n"
    "Undo\n"
    "Redo\n"
    "Select all\n"
    "Increase stroke width\n"
    "Decrease stroke width\n"
    "Next presentation slide\n"
    "Previous presentation slide\n"
    "Delete selected elements\n"
    "Return to parent space\n"
    "Toggle space tree\n"
    "Toggle toolbar\n"
    "Toggle toolbar auto-hide\n"
    "Perform main action (edit/open/play)\n"
    "Finish text editing\n"
    "Finish editing and create new inline text";

  GtkWidget *keys_label = gtk_label_new(keys);
  gtk_label_set_xalign(GTK_LABEL(keys_label), 0.0);
  gtk_widget_add_css_class(keys_label, "shortcuts-keys");

  GtkWidget *desc_label = gtk_label_new(descriptions);
  gtk_label_set_xalign(GTK_LABEL(desc_label), 0.0);

  gtk_grid_attach(GTK_GRID(grid), keys_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), desc_label, 1, 0, 1, 1);

  gtk_box_append(GTK_BOX(container), grid);

  // Style the notification
  gtk_widget_add_css_class(container, "shortcuts-notification");

  // Add CSS for the notification style
  GtkCssProvider *provider = gtk_css_provider_new();
  const char *css =
    ".shortcuts-notification { "
    "  background-color: rgba(0, 0, 0, 0.85); "
    "  color: white; "
    "  padding: 16px 24px; "
    "  border-radius: 8px; "
    "  font-size: 13px; "
    "  font-family: monospace; "
    "} "
    ".shortcuts-keys { "
    "  font-weight: bold; "
    "}";
  gtk_css_provider_load_from_data(provider, css, -1);
  gtk_style_context_add_provider_for_display(
    gdk_display_get_default(),
    GTK_STYLE_PROVIDER(provider),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  // Add to overlay
  gtk_overlay_add_overlay(GTK_OVERLAY(data->overlay), container);

  // Auto-hide after 10 seconds
  g_timeout_add(10000, hide_shortcuts_notification, container);
}



void canvas_on_left_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_PRIMARY_PRESS;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = x;
  ui_event.data.pointer.y = y;
  ui_event.data.pointer.n_press = n_press;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}



void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_MOTION;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = x;
  ui_event.data.pointer.y = y;
  ui_event.data.pointer.n_press = 0;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}





void canvas_on_right_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_SECONDARY_RELEASE;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = x;
  ui_event.data.pointer.y = y;
  ui_event.data.pointer.n_press = n_press;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}

void canvas_on_left_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_PRIMARY_RELEASE;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = x;
  ui_event.data.pointer.y = y;
  ui_event.data.pointer.n_press = n_press;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_LEAVE;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = data->last_mouse_x;
  ui_event.data.pointer.y = data->last_mouse_y;
  ui_event.data.pointer.n_press = 0;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}

static Element* canvas_pick_element_internal(CanvasData *data, int x, int y, gboolean include_locked) {
  Element *selected_element = NULL;
  int highest_z_index = -1;

  // Use quadtree to query only nearby elements
  GList *candidates = NULL;
  if (data->quadtree) {
    candidates = quadtree_query_point(data->quadtree, x, y);
  } else {
    // Fallback to full scan if quadtree not available
    candidates = canvas_get_visual_elements(data);
  }

  for (GList *l = candidates; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;

    // Skip hidden elements, and optionally skip locked elements
    ModelElement *model_element = model_get_by_visual(data->model, element);
    if (model_element) {
      if (canvas_is_element_hidden(data, model_element->uuid)) {
        continue;
      }
      if (!include_locked && model_element->locked) {
        continue;
      }
    }

    double rotated_x = x;
    double rotated_y = y;
    if (element->rotation_degrees != 0.0) {
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;
        double dx = x - center_x;
        double dy = y - center_y;
        double angle_rad = -element->rotation_degrees * M_PI / 180.0;
        rotated_x = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
        rotated_y = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
    }

    gboolean inside = FALSE;

    if (element->type == ELEMENT_MEDIA_FILE) {
      MediaNote *media_note = (MediaNote*)element;
      int bounds_x, bounds_y, bounds_w, bounds_h;
      media_note_get_visible_bounds(media_note, &bounds_x, &bounds_y, &bounds_w, &bounds_h);
      if (rotated_x >= bounds_x && rotated_x <= bounds_x + bounds_w &&
          rotated_y >= bounds_y && rotated_y <= bounds_y + bounds_h) {
        inside = TRUE;
      }
    } else if (rotated_x >= element->x && rotated_x <= element->x + element->width &&
               rotated_y >= element->y && rotated_y <= element->y + element->height) {
      inside = TRUE;
    }

    if (!inside) {
      continue;
    }

    if (element->z > highest_z_index) {
      selected_element = element;
      highest_z_index = element->z;
    }
  }

  // Free the candidate list from quadtree query
  if (data->quadtree && candidates) {
    g_list_free(candidates);
  }

  return selected_element;
}

// Helper function to pick any element (including locked ones) - used for right-click
Element* canvas_pick_element_including_locked(CanvasData *data, int x, int y) {
  return canvas_pick_element_internal(data, x, y, TRUE);
}

Element* canvas_pick_element(CanvasData *data, int x, int y) {
  return canvas_pick_element_internal(data, x, y, FALSE);
}

void canvas_update_cursor(CanvasData *data, int x, int y) {
  if (data->drawing_mode) {
    // Use appropriate cursor based on shift state
    if (data->modifier_state & GDK_SHIFT_MASK) {
      canvas_set_cursor(data, data->line_cursor);
    } else {
      canvas_set_cursor(data, data->draw_cursor);
    }
    return;
  }

  int cx, cy;
  canvas_screen_to_canvas(data, x, y, &cx, &cy);

  if (data->selected_elements) {
      GList *l;
      for (l = data->selected_elements; l != NULL; l = l->next) {
          Element *selected_element = (Element *)l->data;
          // Skip locked elements for rotation handle cursor
          ModelElement *model_element = model_get_by_visual(data->model, selected_element);
          if (model_element && model_element->locked) {
              continue;
          }
          if (element_pick_rotation_handle(selected_element, cx, cy)) {
              canvas_set_cursor(data, gdk_cursor_new_from_name("crosshair", NULL));
              return;
          }
      }
  }

  Element *element = canvas_pick_element(data, cx, cy);

  if (element) {
    int rh = element_pick_resize_handle(element, cx, cy);
    if (rh >= 0) {
      switch (rh) {
      case 0: case 2: // Top-left, Bottom-right
        canvas_set_cursor(data, gdk_cursor_new_from_name("nwse-resize", NULL));
        break;
      case 1: case 3: // Top-right, Bottom-left
        canvas_set_cursor(data, gdk_cursor_new_from_name("nesw-resize", NULL));
        break;
      }
      return;
    }

    int cp = element_pick_connection_point(element, cx, cy);
    if (cp >= 0) {
      // If we have an active connection and this is a different element, show completion cursor
      if (data->connection_start && element != data->connection_start) {
        canvas_set_cursor(data, gdk_cursor_new_from_name("alias", NULL)); // Different cursor for connection completion
      } else {
        canvas_set_cursor(data, gdk_cursor_new_from_name("crosshair", NULL));
      }
      return;
    }

    // Check for DSL click handlers
    ModelElement *model_element = model_get_by_visual(data->model, element);
    if (model_element) {
      const gchar *element_id = dsl_runtime_lookup_element_id(data, model_element);
      if (element_id) {
        GHashTable *click_handlers = dsl_runtime_get_click_handlers(data);
        if (click_handlers) {
          GPtrArray *handlers = (GPtrArray *)g_hash_table_lookup(click_handlers, element_id);
          if (handlers && handlers->len > 0) {
            canvas_set_cursor(data, data->pointer_cursor);
            return;
          }
        }
      }
    }

    canvas_set_cursor(data, gdk_cursor_new_from_name("move", NULL));
    return;
  } else {
    canvas_set_cursor(data, gdk_cursor_new_from_name("default", NULL));
  }
}

void canvas_set_cursor(CanvasData *data, GdkCursor *cursor) {
  if (data->current_cursor != cursor) {
    gtk_widget_set_cursor(data->drawing_area, cursor);
    data->current_cursor = cursor;
  }
}


void canvas_on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_POINTER_SECONDARY_PRESS;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.pointer.x = x;
  ui_event.data.pointer.y = y;
  ui_event.data.pointer.n_press = n_press;
  ui_event.data.pointer.modifiers = data->modifier_state;

  ui_event_bus_emit(&ui_event);
}

void on_clipboard_texture_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  CanvasData *data = (CanvasData *)user_data;
  GError *error = NULL;


  GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);

  GdkTexture *texture = gdk_clipboard_read_texture_finish(clipboard, res, &error);
  if (!texture) {
    if (error) g_error_free(error);
    return;
  }

  // Convert to GdkPixbuf
  GdkPixbuf *pixbuf = gdk_pixbuf_get_from_texture(texture);
  if (!pixbuf) {
    g_print("Failed to convert texture to pixbuf: %s\n",
            error ? error->message : "unknown");
    if (error) g_error_free(error);
    g_object_unref(texture);
    return;
  }

  // Convert pixbuf to raw image data
  gsize buffer_size;
  gchar *buffer = NULL;

  if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size, "png", &error, NULL)) {
    ElementPosition position = {
      .x = 100,
      .y = 100,
      .z = data->next_z_index++,
    };
    ElementColor bg_color = {
      .r = 1.0,
      .g = 1.0,
      .b = 1.0,
      .a = 1.0,
    };
    ElementColor text_color = {
      .r = 1.0,
      .g = 1.0,
      .b = 1.0,
      .a = 1.0,
    };
    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(data->drawing_area));
    ElementSize size = {
      .width = gdk_pixbuf_get_width(pixbuf) / scale,
      .height = gdk_pixbuf_get_height(pixbuf) / scale,
    };
    ElementMedia media = { .type = MEDIA_TYPE_IMAGE, .image_data = (unsigned char*) buffer, .image_size = buffer_size, .video_data = NULL, .video_size = 0, .duration = 0 };
    ElementConnection connection = {
      .from_element_uuid = NULL,
      .to_element_uuid = NULL,
      .from_point = -1,
      .to_point = -1,
    };
    ElementDrawing drawing = {
      .drawing_points = NULL,
      .stroke_width = 0,
    };
    ElementText text = {
      .text = "",
      .text_color = text_color,
      .font_description = g_strdup("Ubuntu Mono 12"),
    };
    ElementConfig config = {
      .type = ELEMENT_MEDIA_FILE,
      .bg_color = bg_color,
      .position = position,
      .size = size,
      .media = media,
      .drawing = drawing,
      .connection = connection,
      .text = text,
    };


    ModelElement *model_element = model_create_element(data->model, config);

    model_element->visual_element = create_visual_element(model_element, data);

    undo_manager_push_create_action(data->undo_manager, model_element);

    gtk_widget_queue_draw(data->drawing_area);

    g_free(buffer);
  }


  // Queue redraw
  gtk_widget_queue_draw(data->drawing_area);

  g_object_unref(pixbuf);
  g_object_unref(texture);
}

void on_clipboard_text_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  CanvasData *data = (CanvasData *)user_data;
  GError *error = NULL;
  GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);

  char *text = gdk_clipboard_read_text_finish(clipboard, res, &error);
  if (text && g_utf8_strlen(text, -1) > 0) {
    // We have text! Create an inline text element at cursor position
    // Get current pointer position
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_widget_get_root(data->drawing_area)));
    GdkDevice *device = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default()));
    double pointer_x, pointer_y;
    gdk_surface_get_device_position(surface, device, &pointer_x, &pointer_y, NULL);

    // Convert mouse position to canvas coordinates
    int canvas_x, canvas_y;
    canvas_screen_to_canvas(data, (int)pointer_x, (int)pointer_y, &canvas_x, &canvas_y);

    ElementSize size = {
      .width = 100,
      .height = 20,
    };
    ElementPosition position = {
      .x = canvas_x - size.width / 2,  // Center horizontally at cursor
      .y = canvas_y - size.height / 2,  // Center vertically at cursor
      .z = data->next_z_index++,
    };
    ElementColor bg_color = {
      .r = 0.0,
      .g = 0.0,
      .b = 0.0,
      .a = 0.0,
    };
    ElementColor text_color = {
      .r = 0.9,
      .g = 0.9,
      .b = 0.9,
      .a = 1.0,
    };
    ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
    ElementConnection connection = {
      .from_element_uuid = NULL,
      .to_element_uuid = NULL,
      .from_point = -1,
      .to_point = -1,
    };
    ElementDrawing drawing = {
      .drawing_points = NULL,
      .stroke_width = 0,
    };
    ElementText element_text = {
      .text = g_strdup(text),
      .text_color = text_color,
      .font_description = g_strdup("Ubuntu Mono 14"),
    };
    ElementConfig config = {
      .type = ELEMENT_INLINE_TEXT,
      .bg_color = bg_color,
      .position = position,
      .size = size,
      .media = media,
      .drawing = drawing,
      .connection = connection,
      .text = element_text,
    };

    ModelElement *model_element = model_create_element(data->model, config);
    model_element->visual_element = create_visual_element(model_element, data);
    undo_manager_push_create_action(data->undo_manager, model_element);
    gtk_widget_queue_draw(data->drawing_area);

    g_free(text);
    if (error) g_error_free(error);
    return;
  }

  // No text, try image instead
  if (error) g_error_free(error);
  if (text) g_free(text);

  gdk_clipboard_read_texture_async(
                                   clipboard,
                                   NULL,                     // GCancellable
                                   on_clipboard_texture_ready, // Callback function
                                   data                       // user_data
                                   );
}

void canvas_on_paste(GtkWidget *widget, CanvasData *data) {
  // First check if we have copied elements to paste
  if (data->copied_elements) {
    // Save copied elements first (in case they're MODEL_STATE_NEW)
    model_save_elements(data->model);

    // Get current pointer position
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_widget_get_root(data->drawing_area)));
    GdkDevice *device = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default()));
    double pointer_x, pointer_y;
    gdk_surface_get_device_position(surface, device, &pointer_x, &pointer_y, NULL);

    // Calculate bounding box of copied elements
    int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
    for (GList *l = data->copied_elements; l != NULL; l = l->next) {
      ModelElement *element = (ModelElement*)l->data;
      if (element->position->x < min_x) min_x = element->position->x;
      if (element->position->y < min_y) min_y = element->position->y;
      int elem_max_x = element->position->x + (element->size ? element->size->width : 100);
      int elem_max_y = element->position->y + (element->size ? element->size->height : 100);
      if (elem_max_x > max_x) max_x = elem_max_x;
      if (elem_max_y > max_y) max_y = elem_max_y;
    }

    // Calculate center of bounding box
    int bbox_center_x = (min_x + max_x) / 2;
    int bbox_center_y = (min_y + max_y) / 2;

    // Convert mouse position to canvas coordinates
    int canvas_mouse_x, canvas_mouse_y;
    canvas_screen_to_canvas(data, (int)pointer_x, (int)pointer_y, &canvas_mouse_x, &canvas_mouse_y);

    // Calculate offset to center at mouse position
    int offset_x = canvas_mouse_x - bbox_center_x;
    int offset_y = canvas_mouse_y - bbox_center_y;

    // Create a hash table to map old UUIDs to new UUIDs
    GHashTable *uuid_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    GList *forked_elements = NULL;

    // First pass: Fork all elements and build UUID mapping
    for (GList *l = data->copied_elements; l != NULL; l = l->next) {
      ModelElement *copied = (ModelElement*)l->data;
      ModelElement *forked = model_element_fork(data->model, copied);

      if (forked) {
        // Apply offset and update z-index
        int new_x = forked->position->x + offset_x;
        int new_y = forked->position->y + offset_y;
        int new_z = data->next_z_index++;
        model_update_position(data->model, forked, new_x, new_y, new_z);

        // Store UUID mapping
        g_hash_table_insert(uuid_map, g_strdup(copied->uuid), g_strdup(forked->uuid));
        forked_elements = g_list_append(forked_elements, forked);

        // Add to undo manager
        undo_manager_push_create_action(data->undo_manager, forked);
      }
    }

    // Second pass: Update connections on newly forked elements to point to other copied elements
    for (GList *l = forked_elements; l != NULL; l = l->next) {
      ModelElement *element = (ModelElement*)l->data;
      gboolean updated = FALSE;

      // Check if from_element was also copied
      if (element->from_element_uuid) {
        char *new_from_uuid = g_hash_table_lookup(uuid_map, element->from_element_uuid);
        if (new_from_uuid) {
          g_free(element->from_element_uuid);
          element->from_element_uuid = g_strdup(new_from_uuid);
          updated = TRUE;
        }
      }

      // Check if to_element was also copied
      if (element->to_element_uuid) {
        char *new_to_uuid = g_hash_table_lookup(uuid_map, element->to_element_uuid);
        if (new_to_uuid) {
          g_free(element->to_element_uuid);
          element->to_element_uuid = g_strdup(new_to_uuid);
          updated = TRUE;
        }
      }

      // Mark for saving if updated
      if (updated) {
        element->state = MODEL_STATE_UPDATED;
      }
    }

    // Save any updated connections
    model_save_elements(data->model);

    g_list_free(forked_elements);
    g_hash_table_destroy(uuid_map);

    int count = g_list_length(data->copied_elements);
    char message[64];
    snprintf(message, sizeof(message), "%d element%s pasted", count, count == 1 ? "" : "s");
    canvas_show_notification(data, message);

    // Sync with model to create visual elements
    canvas_sync_with_model(data);
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  // Otherwise, try to paste from system clipboard
  GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());

  if (!clipboard) {
    return;
  }

  // First try to read text from clipboard
  gdk_clipboard_read_text_async(
                                clipboard,
                                NULL,                     // GCancellable
                                on_clipboard_text_ready,  // Callback function
                                data                       // user_data
                                );
}

gboolean canvas_on_key_pressed(GtkEventControllerKey *controller, guint keyval,
                               guint keycode, GdkModifierType state, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_KEY_PRESS;
  ui_event.canvas = data;
  ui_event.gdk_event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  ui_event.data.key.keyval = keyval;
  ui_event.data.key.keycode = keycode;
  ui_event.data.key.modifiers = state;

  return ui_event_bus_emit(&ui_event);
}

gboolean canvas_on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  UIEvent ui_event = {0};
  ui_event.type = UI_EVENT_SCROLL;
  ui_event.canvas = data;
  ui_event.gdk_event = event;
  ui_event.data.scroll.dx = dx;
  ui_event.data.scroll.dy = dy;
  ui_event.data.scroll.modifiers = data->modifier_state;

  return ui_event_bus_emit(&ui_event);
}

gboolean on_window_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Get window height to detect bottom edge
  GtkRoot *root = gtk_widget_get_root(data->drawing_area);
  GtkWidget *window = GTK_WIDGET(root);
  int window_height = gtk_widget_get_height(window);

  // Show toolbar when mouse is near the bottom edge (within 5 pixels)
  if (data->toolbar_auto_hide && y >= (window_height - 5)) {
    show_toolbar(data);
  }

  return FALSE;
}
