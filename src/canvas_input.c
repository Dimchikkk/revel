#include "canvas_input.h"
#include "canvas_core.h"
#include "canvas_actions.h"
#include "canvas_search.h"
#include "canvas_spaces.h"
#include "canvas_space_select.h"
#include "element.h"
#include "model.h"
#include "paper_note.h"
#include "note.h"
#include "media_note.h"
#include "inline_text.h"
#include "connection.h"
#include "space.h"
#include <pango/pangocairo.h>
#include <gtk/gtkdialog.h>
#include <math.h>
#include "undo_manager.h"
#include "dsl_executor.h"
#include "freehand_drawing.h"
#include "shape.h"
#include "shape_dialog.h"
#include "font_dialog.h"
#include "clone_dialog.h"
#include <graphene.h>

typedef struct {
  const char *shortcut;
  const char *description;
} ShortcutInfo;

static void append_section_if_not_empty(GMenu *parent, GMenu *section) {
  if (!parent || !section) return;

  if (g_menu_model_get_n_items(G_MENU_MODEL(section)) > 0) {
    g_menu_append_section(parent, NULL, G_MENU_MODEL(section));
  }

  g_object_unref(section);
}

static void canvas_show_shortcuts_dialog(CanvasData *data) {
  if (!data || !data->drawing_area) {
    return;
  }

  static const ShortcutInfo shortcuts[] = {
    { "Ctrl+N", "Create inline text note" },
    { "Ctrl+Shift+N", "Create rich text note" },
    { "Ctrl+Shift+P", "Create paper note" },
    { "Ctrl+Shift+S", "Create nested space" },
    { "Ctrl+L", "Open shape library" },
    { "Ctrl+S", "Open search" },
    { "Ctrl+E", "Open DSL executor" },
    { "Ctrl+D", "Toggle drawing mode" },
    { "Ctrl+V", "Paste from clipboard" },
    { "Ctrl+Z", "Undo" },
    { "Ctrl+Y", "Redo" },
    { "Ctrl+A", "Select all elements" },
    { "Delete", "Delete selected elements" },
    { "Backspace", "Return to parent space" },
    { "Ctrl+J", "Toggle space tree" },
    { "Ctrl+T", "Toggle toolbar visibility" },
    { "Ctrl+Shift+T", "Toggle toolbar auto-hide" }
  };

  GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(data->drawing_area));
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Keyboard Shortcuts",
    GTK_WINDOW(root),
    GTK_DIALOG_MODAL,
    "Close",
    GTK_RESPONSE_CLOSE,
    NULL);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(root));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 420);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *header = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(header), "<span size='large' weight='bold'>Keyboard Shortcuts</span>");
  gtk_label_set_xalign(GTK_LABEL(header), 0.0);
  gtk_widget_set_margin_start(header, 8);
  gtk_widget_set_margin_end(header, 8);
  gtk_widget_set_margin_top(header, 8);
  gtk_widget_set_margin_bottom(header, 12);
  gtk_box_append(GTK_BOX(content), header);

  GtkWidget *scrolled = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_widget_set_hexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(content), scrolled);

  GtkWidget *grid = gtk_grid_new();
  gtk_widget_set_margin_start(grid, 12);
  gtk_widget_set_margin_end(grid, 12);
  gtk_widget_set_margin_bottom(grid, 12);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), grid);

  const size_t shortcut_count = G_N_ELEMENTS(shortcuts);
  for (size_t i = 0; i < shortcut_count; i++) {
    GtkWidget *shortcut_label = gtk_label_new(NULL);
    gchar *markup = g_strdup_printf("<tt>%s</tt>", shortcuts[i].shortcut);
    gtk_label_set_markup(GTK_LABEL(shortcut_label), markup);
    g_free(markup);
    gtk_label_set_xalign(GTK_LABEL(shortcut_label), 0.0);

    GtkWidget *description_label = gtk_label_new(shortcuts[i].description);
    gtk_label_set_xalign(GTK_LABEL(description_label), 0.0);
    gtk_label_set_wrap(GTK_LABEL(description_label), TRUE);

    gtk_grid_attach(GTK_GRID(grid), shortcut_label, 0, (int)i, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), description_label, 1, (int)i, 1, 1);
  }

  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
  gtk_window_present(GTK_WINDOW(dialog));
}

void canvas_on_left_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  int cx, cy;
  canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

  // Handle shape mode separately from drawing mode to avoid conflicts
  if (data->shape_mode) {
    // Handle shape creation
    if (!data->current_shape) {
      ElementPosition position = { cx, cy, data->next_z_index++ };
      ElementSize size = { 0, 0 };  // Initial size
      ElementText text = {
        .text = "",
        .text_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 },
        .font_description = "Ubuntu Mono 12"
      };
      ElementColor stroke_color = data->drawing_color;
      if (stroke_color.a <= 0.0) {
        stroke_color.a = 1.0;
      }
      // Initialize bg_color to match stroke_color for shapes
      ElementColor bg_color = stroke_color;
      ElementShape shape_config = {
        .shape_type = data->selected_shape_type,
        .stroke_width = data->drawing_stroke_width,
        .filled = data->shape_filled,
        .stroke_style = data->shape_stroke_style,
        .fill_style = data->shape_fill_style,
        .stroke_color = stroke_color
      };
      data->current_shape = shape_create(position, size, bg_color,
                                         data->drawing_stroke_width,
                                         data->selected_shape_type,
                                         data->shape_filled, text, shape_config,
                                         NULL,
                                         data);
      data->shape_start_x = cx;
      data->shape_start_y = cy;
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode) {
    // Handle freehand/line drawing
    if (!data->current_drawing) {
      ElementPosition position = { cx, cy, data->next_z_index++ };

      gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;
      data->current_drawing = freehand_drawing_create(position, data->drawing_color,
                                                      data->drawing_stroke_width, data);
      freehand_drawing_add_point(data->current_drawing, cx, cy);

      if (is_straight_line) {
        // Store the start point for straight line
        freehand_drawing_add_point(data->current_drawing, cx, cy);
      }
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  // Check if the click is on a rotation handle of a selected element
  if (data->selected_elements) {
      GList *l;
      for (l = data->selected_elements; l != NULL; l = l->next) {
          Element *selected_element = (Element *)l->data;
          if (element_pick_rotation_handle(selected_element, cx, cy)) {
              if (!(data->modifier_state & GDK_SHIFT_MASK)) {
                  // Keep the current selection
              } else {
                  // Add to selection if not already selected
                  if (!canvas_is_element_selected(data, selected_element)) {
                      data->selected_elements = g_list_append(data->selected_elements, selected_element);
                  }
              }

              element_bring_to_front(selected_element, &data->next_z_index);
              selected_element->rotating = TRUE;

              ModelElement *model_element = model_get_by_visual(data->model, selected_element);
              if (model_element) {
                  selected_element->orig_rotation = model_element->rotation_degrees;
              } else {
                  selected_element->orig_rotation = selected_element->rotation_degrees;
              }
              return; // Rotation handle has priority
          }
      }
  }

  Element *element = canvas_pick_element(data, cx, cy);

  // Handle video playback toggle on SINGLE click only
  if (element && element->type == ELEMENT_MEDIA_FILE && n_press == 2) {
    MediaNote *media_note = (MediaNote*)element;
    if (media_note->media_type == MEDIA_TYPE_VIDEO) {
      media_note_toggle_video_playback(element);
      return;
    }
  }

  // If no visible element was found, clear any potential hidden element selection
  if (!element && !(data->modifier_state & GDK_SHIFT_MASK)) {
    canvas_clear_selection(data);
  }

  if (element && element->type == ELEMENT_SPACE && n_press == 2) {
    model_save_elements(data->model);
    ModelElement *model_element = model_get_by_visual(data->model, element);
    switch_to_space(data, model_element->target_space_uuid);
    return;
  }

  // Initialize drag_start_positions if it doesn't exist
  if (!data->drag_start_positions) {
    data->drag_start_positions = g_hash_table_new(g_direct_hash, g_direct_equal);
  } else {
    // Clear any existing drag positions
    g_hash_table_remove_all(data->drag_start_positions);
  }

  // Store original positions for all selected elements
  for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
    Element *selected_element = (Element*)sel->data;
    ModelElement *model_element = model_get_by_visual(data->model, selected_element);

    if (model_element && model_element->position) {
      PositionData *pos_data = g_new0(PositionData, 1);
      pos_data->element = model_element;
      pos_data->x = model_element->position->x;
      pos_data->y = model_element->position->y;

      g_hash_table_insert(data->drag_start_positions, model_element, pos_data);
    }
  }

  if (element) {
    int rh = element_pick_resize_handle(element, cx, cy);
    if (rh >= 0) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        canvas_clear_selection(data);
      }
      if (!canvas_is_element_selected(data, element)) {
        data->selected_elements = g_list_append(data->selected_elements, element);
      }

      element_bring_to_front(element, &data->next_z_index);
      element->resizing = TRUE;
      element->resize_edge = rh;
      element->resize_start_x = cx;
      element->resize_start_y = cy;
      element->orig_x = element->x;
      element->orig_y = element->y;

      // Store original size from model for resize undo
      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && model_element->size) {
        element->orig_width = model_element->size->width;
        element->orig_height = model_element->size->height;
      } else {
        element->orig_width = element->width;
        element->orig_height = element->height;
      }

      return;
    }

    int cp = element_pick_connection_point(element, cx, cy);
    if (cp >= 0) {
      // Special handling for bezier control point dragging
      if (element->type == ELEMENT_SHAPE && canvas_is_element_selected(data, element)) {
        Shape *shape = (Shape*)element;
        if (shape->shape_type == SHAPE_BEZIER && shape->has_bezier_points) {
          // Start dragging this control point instead of creating a connection
          shape->dragging_control_point = TRUE;
          shape->dragging_control_point_index = cp;
          return;
        }
      }

      if (!data->connection_start) {
        data->connection_start = element;
        data->connection_start_point = cp;
      } else {
        if (element != data->connection_start) {
          // Use helper function to get model elements
          ModelElement *from_model = model_get_by_visual(data->model, data->connection_start);
          ModelElement *to_model = model_get_by_visual(data->model, element);

          if (from_model && to_model) {
            ElementPosition position = {
              .x = 0,
              .y = 0,
              .z = MAX(from_model->position->z, to_model->position->z),
            };
            ElementColor bg_color = {
              .r = 1.0,
              .g = 1.0,
              .b = 1.0,
              .a = 1.0,
            };
            ElementColor text_color = {
              .r = 0.0,
              .g = 0.0,
              .b = 0.0,
              .a = 0.0,
            };
            ElementSize size = {
              .width = 1,
              .height = 1,
            };
            ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
            ElementConnection connection = {
              .from_element = data->connection_start,
              .to_element = element,
              .from_element_uuid = from_model->uuid,
              .to_element_uuid = to_model->uuid,
              .from_point = data->connection_start_point,
              .to_point = cp,
              .connection_type = CONNECTION_TYPE_PARALLEL,
              .arrowhead_type = ARROWHEAD_SINGLE,
            };
            ElementDrawing drawing = {
              .drawing_points = NULL,
              .stroke_width = 0,
            };
            ElementText text = {
              .text = NULL,
              .text_color = text_color,
              .font_description = NULL,
            };
            ElementConfig config = {
              .type = ELEMENT_CONNECTION,
              .bg_color = bg_color,
              .position = position,
              .size = size,
              .media = media,
              .drawing = drawing,
              .connection = connection,
              .text = text,
            };

            ModelElement *model_conn = model_create_element(data->model, config);

            model_conn->visual_element = create_visual_element(model_conn, data);

            // Push undo action for connection creation
            undo_manager_push_create_action(data->undo_manager, model_conn);
          }
        }
        data->connection_start = NULL;
        data->connection_start_point = -1;
      }
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    element_bring_to_front(element, &data->next_z_index);

    if (n_press == 2) {
      element_start_editing(element, data->overlay);
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }


    if (!((element->type == ELEMENT_PAPER_NOTE && ((PaperNote*)element)->editing) ||
          (element->type == ELEMENT_MEDIA_FILE && ((MediaNote*)element)->editing) ||
          (element->type == ELEMENT_NOTE && ((Note*)element)->editing) ||
          (element->type == ELEMENT_SHAPE && ((Shape*)element)->editing) ||
          (element->type == ELEMENT_INLINE_TEXT && ((InlineText*)element)->editing))) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        canvas_clear_selection(data);
      }
      if (!canvas_is_element_selected(data, element)) {
        data->selected_elements = g_list_append(data->selected_elements, element);

        // Store position for the newly selected element
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element && model_element->position) {
          PositionData *pos_data = g_new0(PositionData, 1);
          pos_data->element = model_element;
          pos_data->x = model_element->position->x;
          pos_data->y = model_element->position->y;

          g_hash_table_insert(data->drag_start_positions, model_element, pos_data);
        }
      }
      element->dragging = TRUE;
      element->drag_offset_x = cx - element->x;
      element->drag_offset_y = cy - element->y;
    }
  } else {
    data->connection_start = NULL;
    data->connection_start_point = -1;

    if (!(data->modifier_state & GDK_SHIFT_MASK)) {
      canvas_clear_selection(data);
    }

    data->selecting = TRUE;
    data->start_x = (int)x;
    data->start_y = (int)y;
    data->current_x = (int)x;
    data->current_y = (int)y;
  }

  gtk_widget_queue_draw(data->drawing_area);
}

static void canvas_update_connections_for_selection(CanvasData *data) {
  if (!data || !data->model || !data->selected_elements) {
    return;
  }

  GHashTable *elements = data->model->elements;
  if (!elements) return;

  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_element = (ModelElement*)value;
    if (!model_element || model_element->state == MODEL_STATE_DELETED) {
      continue;
    }

    if (!model_element->type || model_element->type->type != ELEMENT_CONNECTION) {
      continue;
    }

    Connection *connection = (Connection*)model_element->visual_element;
    if (!connection || !connection->from || !connection->to) {
      continue;
    }

    gboolean affects_from = g_list_find(data->selected_elements, connection->from) != NULL;
    gboolean affects_to = g_list_find(data->selected_elements, connection->to) != NULL;

    if (!affects_from && !affects_to) {
      continue;
    }

    int new_from_point = connection->from_point;
    int new_to_point = connection->to_point;

    ConnectionRect from_rect = {
      .x = connection->from->x,
      .y = connection->from->y,
      .width = connection->from->width,
      .height = connection->from->height,
    };
    ConnectionRect to_rect = {
      .x = connection->to->x,
      .y = connection->to->y,
      .width = connection->to->width,
      .height = connection->to->height,
    };

    connection_determine_optimal_points(from_rect, to_rect, &new_from_point, &new_to_point);

    if (new_from_point != connection->from_point || new_to_point != connection->to_point) {
      connection->from_point = new_from_point;
      connection->to_point = new_to_point;

      model_element->from_point = new_from_point;
      model_element->to_point = new_to_point;
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Store last mouse position for zoom-at-cursor functionality
  data->last_mouse_x = x;
  data->last_mouse_y = y;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  int cx, cy;
  canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

  if (data->shape_mode) {
    canvas_set_cursor(data, data->draw_cursor);
  } else if (data->drawing_mode) {
    if (data->modifier_state & GDK_SHIFT_MASK) {
      canvas_set_cursor(data, data->line_cursor);
    } else {
      canvas_set_cursor(data, data->draw_cursor);
    }
  } else {
    // Update cursor for normal mode (hover detection)
    canvas_update_cursor(data, (int)x, (int)y);
  }
  if (data->shape_mode && data->current_shape) {
    int x1 = data->shape_start_x;
    int y1 = data->shape_start_y;

    data->current_shape->base.x = MIN(x1, cx);
    data->current_shape->base.y = MIN(y1, cy);
    data->current_shape->base.width = MAX(ABS(cx - x1), 10);
    data->current_shape->base.height = MAX(ABS(cy - y1), 10);

    Shape *shape = data->current_shape;
    if ((shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW)) {
      double width = MAX(data->current_shape->base.width, 1);
      double height = MAX(data->current_shape->base.height, 1);

      double start_x = (double)data->shape_start_x;
      double start_y = (double)data->shape_start_y;
      double end_x = (double)cx;
      double end_y = (double)cy;

      double base_x = data->current_shape->base.x;
      double base_y = data->current_shape->base.y;

      shape->line_start_u = CLAMP((start_x - base_x) / width, 0.0, 1.0);
      shape->line_start_v = CLAMP((start_y - base_y) / height, 0.0, 1.0);
      shape->line_end_u = CLAMP((end_x - base_x) / width, 0.0, 1.0);
      shape->line_end_v = CLAMP((end_y - base_y) / height, 0.0, 1.0);
      shape->has_line_points = TRUE;
    } else if (shape->shape_type == SHAPE_BEZIER) {
      // Set bezier points with default S-curve shape
      shape->has_bezier_points = TRUE;
      // Keep default values already set in shape_create
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && data->current_drawing) {
    gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;

    if (is_straight_line) {
      // For straight lines, update the second point
      if (data->current_drawing->points->len >= 2) {
        DrawingPoint *points = (DrawingPoint*)data->current_drawing->points->data;

        // Calculate relative coordinates (relative to the drawing's position)
        float rel_x = (float)(cx - data->current_drawing->base.x);
        float rel_y = (float)(cy - data->current_drawing->base.y);

        points[1].x = rel_x;
        points[1].y = rel_y;

        // Update bounding box
        float min_x = MIN(points[0].x, rel_x);
        float min_y = MIN(points[0].y, rel_y);
        float max_x = MAX(points[0].x, rel_x);
        float max_y = MAX(points[0].y, rel_y);

        // Add padding for stroke width
        float padding = data->current_drawing->stroke_width / 2.0f;
        data->current_drawing->base.width = (int)(max_x - min_x + padding * 2);
        data->current_drawing->base.height = (int)(max_y - min_y + padding * 2);

        // Adjust position if needed to maintain the relative points
        if (min_x < 0) {
          data->current_drawing->base.x += (int)min_x;
          // Adjust all points to be relative to new position
          for (guint i = 0; i < data->current_drawing->points->len; i++) {
            points[i].x -= min_x;
          }
          data->current_drawing->base.width += (int)(-min_x);
        }

        if (min_y < 0) {
          data->current_drawing->base.y += (int)min_y;
          for (guint i = 0; i < data->current_drawing->points->len; i++) {
            points[i].y -= min_y;
          }
          data->current_drawing->base.height += (int)(-min_y);
        }
      }
    } else {
      freehand_drawing_add_point(data->current_drawing, cx, cy);
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  // Redraw to update guide lines when in drawing mode
  if (data->drawing_mode && !data->shape_mode && !data->current_drawing) {
    gtk_widget_queue_draw(data->drawing_area);
  }

  if (data->panning) {
    int dx = (int)x - data->pan_start_x;
    int dy = (int)y - data->pan_start_y;

    data->offset_x += dx;
    data->offset_y += dy;

    data->pan_start_x = (int)x;
    data->pan_start_y = (int)y;

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }
  GList *visual_elements = canvas_get_visual_elements(data);

  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;

    if (element->rotating) {
      // Calculate element center
      double center_x = element->x + element->width / 2.0;
      double center_y = element->y + element->height / 2.0;

      // Calculate angle from center to mouse position
      // atan2 gives angle where 0 is pointing right, we need 0 to point up
      // So we use atan2(dx, -dy) to rotate the coordinate system 90 degrees
      double angle = atan2(cx - center_x, -(cy - center_y)) * 180.0 / M_PI;

      // Normalize angle to 0-360 range
      while (angle < 0) angle += 360.0;
      while (angle >= 360.0) angle -= 360.0;

      // Update rotation
      element->rotation_degrees = angle;

      gtk_widget_queue_draw(data->drawing_area);
      continue;
    }

    if (element->resizing) {
      int dx = cx - element->resize_start_x;
      int dy = cy - element->resize_start_y;

      double angle_rad = -element->rotation_degrees * M_PI / 180.0;
      double cos_a = cos(angle_rad);
      double sin_a = sin(angle_rad);

      double rotated_dx = dx * cos_a - dy * sin_a;
      double rotated_dy = dx * sin_a + dy * cos_a;

      int new_x = element->orig_x;
      int new_y = element->orig_y;
      int new_width = element->orig_width;
      int new_height = element->orig_height;

      switch (element->resize_edge) {
      case 0: // Top-left
        new_width -= rotated_dx;
        new_height -= rotated_dy;
        new_x += rotated_dx * cos(-angle_rad) - rotated_dy * sin(-angle_rad);
        new_y += rotated_dx * sin(-angle_rad) + rotated_dy * cos(-angle_rad);
        break;
      case 1: // Top-right
        new_width += rotated_dx;
        new_height -= rotated_dy;
        new_y += rotated_dx * sin(-angle_rad);
        break;
      case 2: // Bottom-right
        new_width += rotated_dx;
        new_height += rotated_dy;
        break;
      case 3: // Bottom-left
        new_width -= rotated_dx;
        new_height += rotated_dy;
        new_x += rotated_dx * cos(-angle_rad);
        break;
      }

      if (new_width < 50) new_width = 50;
      if (new_height < 30) new_height = 30;

      // Only update visual position/size during resize, not model
      element->x = new_x;
      element->y = new_y;
      element->width = new_width;
      element->height = new_height;

      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    // Handle bezier control point dragging
    if (element->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)element;
      if (shape->dragging_control_point && shape->shape_type == SHAPE_BEZIER) {
        double width = MAX(element->width, 1);
        double height = MAX(element->height, 1);

        // Apply inverse rotation to mouse coordinates if element is rotated
        double local_cx = cx;
        double local_cy = cy;

        if (element->rotation_degrees != 0.0) {
          double center_x = element->x + element->width / 2.0;
          double center_y = element->y + element->height / 2.0;
          double dx = cx - center_x;
          double dy = cy - center_y;
          double angle_rad = -element->rotation_degrees * M_PI / 180.0;
          local_cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
          local_cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
        }

        // Calculate normalized coordinates within the bounding box
        double u = CLAMP((double)(local_cx - element->x) / width, 0.0, 1.0);
        double v = CLAMP((double)(local_cy - element->y) / height, 0.0, 1.0);

        // Update the appropriate control point
        switch (shape->dragging_control_point_index) {
          case 0:
            shape->bezier_p0_u = u;
            shape->bezier_p0_v = v;
            break;
          case 1:
            shape->bezier_p1_u = u;
            shape->bezier_p1_v = v;
            break;
          case 2:
            shape->bezier_p2_u = u;
            shape->bezier_p2_v = v;
            break;
          case 3:
            shape->bezier_p3_u = u;
            shape->bezier_p3_v = v;
            break;
        }

        gtk_widget_queue_draw(data->drawing_area);
        return;
      }
    }

    if (element->dragging) {
      int dx = cx - element->drag_offset_x - element->x;
      int dy = cy - element->drag_offset_y - element->y;

      for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
        Element *selected_element = (Element*)sel->data;
        int new_x = selected_element->x + dx;
        int new_y = selected_element->y + dy;

        // Only update visual position during drag, not model
        selected_element->x = new_x;
        selected_element->y = new_y;
      }

      canvas_update_connections_for_selection(data);

      gtk_widget_queue_draw(data->drawing_area);
      return;
    }
  }

  if (data->selecting) {
    data->current_x = (int)x;
    data->current_y = (int)y;
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void canvas_on_right_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (data->panning) {
    data->panning = FALSE;
    canvas_set_cursor(data, data->default_cursor);
    return;
  }
}

void canvas_on_left_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (data->shape_mode && data->current_shape) {
    ElementConfig config = {0};
    Element* element = (Element*) data->current_shape;
    config.type = element->type;
    config.position.x = element->x;
    config.position.y = element->y;
    config.position.z = element->z;
    config.size.width = element->width;
    config.size.height = element->height;
    config.bg_color.r = element->bg_r;
    config.bg_color.g = element->bg_g;
    config.bg_color.b = element->bg_b;
    config.bg_color.a = element->bg_a;
    Shape *shape = (Shape*)element;
    config.text.text = shape->text;
    config.text.text_color.r = shape->text_r;
    config.text.text_color.g = shape->text_g;
    config.text.text_color.b = shape->text_b;
    config.text.text_color.a = shape->text_a;
    config.text.font_description = shape->font_description;
    config.shape.shape_type = shape->shape_type;
    config.shape.stroke_width = shape->stroke_width;
    config.shape.filled = shape->filled;
    config.shape.stroke_style = shape->stroke_style;
    config.shape.fill_style = shape->fill_style;
    config.shape.stroke_color.r = shape->stroke_r;
    config.shape.stroke_color.g = shape->stroke_g;
    config.shape.stroke_color.b = shape->stroke_b;
    config.shape.stroke_color.a = shape->stroke_a;
    ElementDrawing drawing = { .drawing_points = NULL, .stroke_width = shape->stroke_width };

    GArray *line_points = NULL;
    if ((shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW) && shape->has_line_points) {
      line_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 2);

      DrawingPoint start_point;
      graphene_point_init(&start_point, (float)shape->line_start_u, (float)shape->line_start_v);
      g_array_append_val(line_points, start_point);

      DrawingPoint end_point;
      graphene_point_init(&end_point, (float)shape->line_end_u, (float)shape->line_end_v);
      g_array_append_val(line_points, end_point);

      drawing.drawing_points = line_points;
    } else if (shape->shape_type == SHAPE_BEZIER && shape->has_bezier_points) {
      line_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 4);

      DrawingPoint p0;
      graphene_point_init(&p0, (float)shape->bezier_p0_u, (float)shape->bezier_p0_v);
      g_array_append_val(line_points, p0);

      DrawingPoint p1;
      graphene_point_init(&p1, (float)shape->bezier_p1_u, (float)shape->bezier_p1_v);
      g_array_append_val(line_points, p1);

      DrawingPoint p2;
      graphene_point_init(&p2, (float)shape->bezier_p2_u, (float)shape->bezier_p2_v);
      g_array_append_val(line_points, p2);

      DrawingPoint p3;
      graphene_point_init(&p3, (float)shape->bezier_p3_u, (float)shape->bezier_p3_v);
      g_array_append_val(line_points, p3);

      drawing.drawing_points = line_points;
    }

    config.drawing = drawing;

    ModelElement *model_element = model_create_element(data->model, config);

    if (model_element) {
      model_element->visual_element = create_visual_element(model_element, data);
      undo_manager_push_create_action(data->undo_manager, model_element);
    }

    if (line_points) {
      g_array_free(line_points, TRUE);
    }

    // Clear current shape and exit shape mode
    shape_free((Element*)data->current_shape);
    data->current_shape = NULL;
    data->shape_mode = FALSE;
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && data->current_drawing) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

    gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;

    if (is_straight_line) {
      if (data->current_drawing->points->len >= 2) {
        DrawingPoint *points = (DrawingPoint*)data->current_drawing->points->data;

        // Calculate relative coordinates
        float rel_x = (float)(cx - data->current_drawing->base.x);
        float rel_y = (float)(cy - data->current_drawing->base.y);

        points[1].x = rel_x;
        points[1].y = rel_y;
      }
    } else {
      freehand_drawing_add_point(data->current_drawing, cx, cy);
    }
    ElementPosition position = {
      .x = data->current_drawing->base.x,
      .y = data->current_drawing->base.y,
      .z = data->next_z_index++,
    };
    ElementColor bg_color = {
      .r = data->current_drawing->base.bg_r,
      .g = data->current_drawing->base.bg_g,
      .b = data->current_drawing->base.bg_b,
      .a = data->current_drawing->base.bg_a,
    };
    ElementSize size = {
      .width = data->current_drawing->base.width,
      .height = data->current_drawing->base.height
    };
    ElementColor text_color = {
      .r = 0,
      .g = 0,
      .b = 0,
      .a = 0,
    };
    ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
    ElementConnection connection = {
      .from_element_uuid = NULL,
      .to_element_uuid = NULL,
      .from_point = -1,
      .to_point = -1,
    };
    ElementDrawing drawing = {
      .drawing_points = data->current_drawing->points,
      .stroke_width = data->current_drawing->stroke_width,
    };
    ElementText text = {
      .text = NULL,
      .text_color = text_color,
      .font_description = NULL,
    };
    ElementConfig config = {
      .type = ELEMENT_FREEHAND_DRAWING,
      .bg_color = bg_color,
      .position = position,
      .size = size,
      .media = media,
      .drawing = drawing,
      .connection = connection,
      .text = text,
    };


    ModelElement *model_element = model_create_element(data->model, config);

    if (!model_element) {
      g_printerr("Failed to create drawing element\n");
      return;
    }
    model_element->visual_element = create_visual_element(model_element, data);
    undo_manager_push_create_action(data->undo_manager, model_element);
    data->current_drawing = NULL;
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->selecting) {
    data->selecting = FALSE;

    int start_cx, start_cy, current_cx, current_cy;
    canvas_screen_to_canvas(data, data->start_x, data->start_y, &start_cx, &start_cy);
    canvas_screen_to_canvas(data, data->current_x, data->current_y, &current_cx, &current_cy);

    int sel_x = MIN(start_cx, current_cx);
    int sel_y = MIN(start_cy, current_cy);
    int sel_width = ABS(current_cx - start_cx);
    int sel_height = ABS(current_cy - start_cy);

    GList *visual_elements = canvas_get_visual_elements(data);

    for (GList *iter = visual_elements; iter != NULL; iter = iter->next) {
      Element *element = (Element*)iter->data;

      if (element->x + element->width >= sel_x &&
          element->x <= sel_x + sel_width &&
          element->y + element->height >= sel_y &&
          element->y <= sel_y + sel_height) {
        if (!canvas_is_element_selected(data, element)) {
          data->selected_elements = g_list_append(data->selected_elements, element);
        }
      }
    }
  }

  // Handle undo for drag operations
  gboolean was_moved = FALSE;
  if (data->drag_start_positions && g_hash_table_size(data->drag_start_positions) > 0) {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, data->drag_start_positions);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      ModelElement *model_element = (ModelElement*)key;
      PositionData *start_pos = (PositionData*)value;

      // Get the current visual position from the element
      Element *visual_element = model_element->visual_element;

      if (visual_element && model_element->position &&
          (visual_element->x != start_pos->x ||
           visual_element->y != start_pos->y)) {

        was_moved = TRUE;

        // Update the model with the new position
        model_update_position(data->model, model_element,
                              visual_element->x, visual_element->y,
                              model_element->position->z);

        // Push undo action for the move
        undo_manager_push_move_action(data->undo_manager, model_element,
                                      start_pos->x, start_pos->y,
                                      visual_element->x, visual_element->y);
      }

      g_free(start_pos);
    }

    g_hash_table_remove_all(data->drag_start_positions);
  }

  gboolean was_resized = FALSE;
  gboolean was_rotated = FALSE;
  GList *visual_elements = canvas_get_visual_elements(data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    if (element->resizing) {
      was_resized = TRUE;

      // For resize operations, update model and push undo action
      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && model_element->size) {
        // Push resize undo action
        undo_manager_push_resize_action(data->undo_manager, model_element,
                                        element->orig_width, element->orig_height,
                                        element->width, element->height);

        // Update model size
        model_update_size(data->model, model_element, element->width, element->height);
      }
    }
    if (element->rotating) {
      was_rotated = TRUE;

      // For rotate operations, update model and push undo action
      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element) {
        // Push rotate undo action
        undo_manager_push_rotate_action(data->undo_manager, model_element,
                                        element->orig_rotation,
                                        element->rotation_degrees);

        // Update model rotation
        model_update_rotation(data->model, model_element, element->rotation_degrees);
      }
    }
    // Handle bezier control point dragging completion
    if (element->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)element;
      if (shape->dragging_control_point && shape->shape_type == SHAPE_BEZIER) {
        // Update the model with the new control points
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element && shape->has_bezier_points) {
          // Create a new drawing points array with 4 bezier control points
          GArray *bezier_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 4);

          DrawingPoint p0;
          graphene_point_init(&p0, (float)shape->bezier_p0_u, (float)shape->bezier_p0_v);
          g_array_append_val(bezier_points, p0);

          DrawingPoint p1;
          graphene_point_init(&p1, (float)shape->bezier_p1_u, (float)shape->bezier_p1_v);
          g_array_append_val(bezier_points, p1);

          DrawingPoint p2;
          graphene_point_init(&p2, (float)shape->bezier_p2_u, (float)shape->bezier_p2_v);
          g_array_append_val(bezier_points, p2);

          DrawingPoint p3;
          graphene_point_init(&p3, (float)shape->bezier_p3_u, (float)shape->bezier_p3_v);
          g_array_append_val(bezier_points, p3);

          // Update the model's drawing points
          if (model_element->drawing_points) {
            g_array_free(model_element->drawing_points, TRUE);
          }
          model_element->drawing_points = bezier_points;
        }

        shape->dragging_control_point = FALSE;
        shape->dragging_control_point_index = -1;
      }
    }

    element->dragging = FALSE;
    element->resizing = FALSE;
    element->rotating = FALSE;
  }

  // Re-create visual elements since some properties may be changed due to moving, resizing, or rotation
  // This is needed to sync cloned elements (e.g., elements that share position, size, or color)
  if (was_moved || was_resized || was_rotated) canvas_sync_with_model(data);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  canvas_set_cursor(data, data->default_cursor);
}

Element* canvas_pick_element(CanvasData *data, int x, int y) {
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

    // Skip hidden elements from picking
    ModelElement *model_element = model_get_by_visual(data->model, element);
    if (model_element && canvas_is_element_hidden(data, model_element->uuid)) {
      continue;
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

    if (rotated_x >= element->x && rotated_x <= element->x + element->width &&
        rotated_y >= element->y && rotated_y <= element->y + element->height) {
      if (element->z > highest_z_index) {
        selected_element = element;
        highest_z_index = element->z;
      }
    }
  }

  // Free the candidate list from quadtree query
  if (data->quadtree && candidates) {
    g_list_free(candidates);
  }

  return selected_element;
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

static void on_clone_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    model_save_elements(data->model);
    ModelElement *original = g_hash_table_lookup(data->model->elements, element_uuid);
    if (original) {
      clone_dialog_open(data, original);
    }
  }
}

static gboolean destroy_popover_callback(gpointer user_data) {
  GtkWidget *popover = user_data;
  gtk_widget_unparent(popover);
  return G_SOURCE_REMOVE;
}

static void on_popover_closed(GtkPopover *popover, gpointer user_data) {
  g_idle_add(destroy_popover_callback, popover);
}

static void on_description_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    ModelElement *model_element = g_object_get_data(G_OBJECT(dialog), "model_element");
    GtkTextBuffer *buffer = g_object_get_data(G_OBJECT(dialog), "text_buffer");
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");

    if (model_element && buffer && data) {
      GtkTextIter start, end;
      gtk_text_buffer_get_bounds(buffer, &start, &end);
      gchar *new_description = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

      // Update the description
      g_free(model_element->description);
      model_element->description = g_strdup(new_description);

      // Mark element as updated (but keep NEW elements as NEW)
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }

      g_free(new_description);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_description_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      // Create description dialog
      GtkWidget *dialog = gtk_dialog_new_with_buttons("Element Description",
                                                      GTK_WINDOW(gtk_widget_get_root(data->drawing_area)),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "Cancel", GTK_RESPONSE_CANCEL,
                                                      "Save", GTK_RESPONSE_OK,
                                                      NULL);

      GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
      gtk_widget_set_size_request(dialog, 400, 300);

      // Add creation date label
      gchar *created_text;
      if (model_element->created_at) {
        created_text = g_strdup_printf("Created: %s", model_element->created_at);
      } else if (model_element->state == MODEL_STATE_NEW) {
        created_text = g_strdup("Created: Just now (not saved yet)");
      } else {
        created_text = g_strdup("Created: Unknown");
      }
      GtkWidget *created_label = gtk_label_new(created_text);
      gtk_label_set_xalign(GTK_LABEL(created_label), 0.0);
      gtk_box_append(GTK_BOX(content_area), created_label);
      g_free(created_text);

      // Add description text view
      GtkWidget *scrolled = gtk_scrolled_window_new();
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand(scrolled, TRUE);

      GtkWidget *text_view = gtk_text_view_new();
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
      if (model_element->description) {
        gtk_text_buffer_set_text(buffer, model_element->description, -1);
      }

      gtk_box_append(GTK_BOX(content_area), scrolled);

      // Store references for the response handler
      g_object_set_data(G_OBJECT(dialog), "model_element", model_element);
      g_object_set_data(G_OBJECT(dialog), "text_buffer", buffer);
      g_object_set_data(G_OBJECT(dialog), "canvas_data", data);

      g_signal_connect(dialog, "response", G_CALLBACK(on_description_dialog_response), NULL);

      gtk_widget_show(dialog);
    }
  }
}

static void on_delete_element_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      // Check if it's a space with elements
      if (model_element->type->type == ELEMENT_SPACE) {
        if (model_element->state != MODEL_STATE_NEW) {
          int element_count = model_get_amount_of_elements(data->model, model_element->target_space_uuid);
          if (element_count > 0) {
            GtkWidget *dialog = gtk_message_dialog_new(
              GTK_WINDOW(gtk_widget_get_ancestor(data->drawing_area, GTK_TYPE_WINDOW)),
              GTK_DIALOG_MODAL,
              GTK_MESSAGE_WARNING,
              GTK_BUTTONS_OK,
              "Space contains %d elements. Please remove them first before deleting the space.",
              element_count
            );
            gtk_window_present(GTK_WINDOW(dialog));
            g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
            return;
          }
        }
      }

      // Push undo action BEFORE deletion
      undo_manager_push_delete_action(data->undo_manager, model_element);

      model_delete_element(data->model, model_element);
      canvas_sync_with_model(data);
      gtk_widget_queue_draw(data->drawing_area);
    }
  }
}

static void on_color_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");
    const gchar *element_uuid = g_object_get_data(G_OBJECT(dialog), "element_uuid");

    GtkColorChooser *chooser = GTK_COLOR_CHOOSER(dialog);
    GdkRGBA color;
    gtk_color_chooser_get_rgba(chooser, &color);

    if (data && data->model && element_uuid) {
      ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
      if (model_element && model_element->bg_color) {
        // Save old color for undo
        double old_r = model_element->bg_color->r;
        double old_g = model_element->bg_color->g;
        double old_b = model_element->bg_color->b;
        double old_a = model_element->bg_color->a;

        // Push undo action
        undo_manager_push_color_action(data->undo_manager, model_element,
                                       old_r, old_g, old_b, old_a,
                                       color.red, color.green, color.blue, color.alpha);

        model_update_color(data->model, model_element, color.red, color.green, color.blue, color.alpha);
        canvas_sync_with_model(data);
        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_change_color_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      GtkWidget *dialog = gtk_color_chooser_dialog_new("Choose Element Color",
                                                       GTK_WINDOW(gtk_widget_get_root(data->drawing_area)));

      gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dialog), TRUE);

      if (model_element->bg_color) {
        GdkRGBA initial_color = {
          .red = model_element->bg_color->r / 255.0,
          .green = model_element->bg_color->g / 255.0,
          .blue = model_element->bg_color->b / 255.0,
          .alpha = model_element->bg_color->a / 255.0
        };
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &initial_color);
      }

      g_object_set_data(G_OBJECT(dialog), "canvas_data", data);
      g_object_set_data_full(G_OBJECT(dialog), "element_uuid", g_strdup(element_uuid), g_free);

      g_signal_connect(dialog, "response", G_CALLBACK(on_color_dialog_response), NULL);
      gtk_window_present(GTK_WINDOW(dialog));
    }
  }
}

static void on_change_space_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_show_space_select_dialog(data, element_uuid);
  }
}

typedef struct {
  CanvasData *canvas_data;
  ModelElement *model_element;
  GtkWidget *stroke_combo;
  GtkWidget *fill_combo;
} ShapeStyleDialogData;

static void on_shape_style_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  ShapeStyleDialogData *style_data = (ShapeStyleDialogData*)user_data;
  if (!style_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
    return;
  }

  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = style_data->canvas_data;
    ModelElement *model_element = style_data->model_element;
    if (data && model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)model_element->visual_element;

      int stroke_index = gtk_combo_box_get_active(GTK_COMBO_BOX(style_data->stroke_combo));
      StrokeStyle new_stroke_style = STROKE_STYLE_SOLID;
      switch (stroke_index) {
        case 1:
          new_stroke_style = STROKE_STYLE_DASHED;
          break;
        case 2:
          new_stroke_style = STROKE_STYLE_DOTTED;
          break;
        default:
          new_stroke_style = STROKE_STYLE_SOLID;
          break;
      }

      int fill_index = gtk_combo_box_get_active(GTK_COMBO_BOX(style_data->fill_combo));
      gboolean new_filled = FALSE;
      FillStyle new_fill_style = FILL_STYLE_SOLID;
      switch (fill_index) {
        case 0:
          new_filled = FALSE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
        case 1:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
        case 2:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_HACHURE;
          break;
        case 3:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_CROSS_HATCH;
          break;
        default:
          new_filled = FALSE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
      }

      gboolean style_changed = (shape->stroke_style != new_stroke_style) ||
                               (shape->filled != new_filled) ||
                               (shape->fill_style != new_fill_style);

      if (style_changed) {
        shape->stroke_style = new_stroke_style;
        shape->filled = new_filled;
        shape->fill_style = new_fill_style;

        model_element->stroke_style = new_stroke_style;
        model_element->filled = new_filled;
        model_element->fill_style = new_fill_style;
        if (model_element->state != MODEL_STATE_NEW) {
          model_element->state = MODEL_STATE_UPDATED;
        }

        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
  g_free(style_data);
}

static void on_change_shape_style_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (!data || !data->model || !element_uuid) {
    return;
  }

  ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
  if (!model_element || !model_element->visual_element || model_element->type->type != ELEMENT_SHAPE) {
    return;
  }

  Shape *shape = (Shape*)model_element->visual_element;
  GtkRoot *root = gtk_widget_get_root(data->drawing_area);
  GtkWidget *window = GTK_WIDGET(root);
  if (!GTK_IS_WINDOW(window)) {
    return;
  }
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Change Shape Style",
    GTK_WINDOW(window),
    GTK_DIALOG_MODAL,
    "_Cancel", GTK_RESPONSE_CANCEL,
    "_OK", GTK_RESPONSE_OK,
    NULL);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_set_margin_top(content, 10);
  gtk_widget_set_margin_bottom(content, 10);
  gtk_widget_set_margin_start(content, 10);
  gtk_widget_set_margin_end(content, 10);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
  gtk_box_append(GTK_BOX(content), grid);

  GtkWidget *fill_label = gtk_label_new("Fill Style");
  gtk_widget_set_halign(fill_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), fill_label, 0, 0, 1, 1);

  GtkWidget *fill_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Outline");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Solid");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Hachure");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Cross Hatch");
  gtk_widget_set_hexpand(fill_combo, TRUE);
  int fill_index = 0;
  if (shape->filled) {
    switch (shape->fill_style) {
      case FILL_STYLE_HACHURE:
        fill_index = 2;
        break;
      case FILL_STYLE_CROSS_HATCH:
        fill_index = 3;
        break;
      default:
        fill_index = 1;
        break;
    }
  } else {
    fill_index = 0;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(fill_combo), fill_index);
  gtk_grid_attach(GTK_GRID(grid), fill_combo, 1, 0, 1, 1);

  GtkWidget *stroke_label = gtk_label_new("Stroke Style");
  gtk_widget_set_halign(stroke_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), stroke_label, 0, 1, 1, 1);

  GtkWidget *stroke_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Solid");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Dashed");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Dotted");
  gtk_widget_set_hexpand(stroke_combo, TRUE);
  int stroke_index = 0;
  switch (shape->stroke_style) {
    case STROKE_STYLE_DASHED:
      stroke_index = 1;
      break;
    case STROKE_STYLE_DOTTED:
      stroke_index = 2;
      break;
    default:
      stroke_index = 0;
      break;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(stroke_combo), stroke_index);
  gtk_grid_attach(GTK_GRID(grid), stroke_combo, 1, 1, 1, 1);

  ShapeStyleDialogData *style_data = g_new0(ShapeStyleDialogData, 1);
  style_data->canvas_data = data;
  style_data->model_element = model_element;
  style_data->stroke_combo = stroke_combo;
  style_data->fill_combo = fill_combo;

  g_signal_connect(dialog, "response", G_CALLBACK(on_shape_style_dialog_response), style_data);
  gtk_window_present(GTK_WINDOW(dialog));
}

static void on_stroke_color_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");
    const gchar *element_uuid = g_object_get_data(G_OBJECT(dialog), "element_uuid");
    if (data && data->model && element_uuid) {
      ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
      if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
        Shape *shape = (Shape*)model_element->visual_element;
        GtkColorChooser *chooser = GTK_COLOR_CHOOSER(dialog);
        GdkRGBA color;
        gtk_color_chooser_get_rgba(chooser, &color);

        shape->stroke_r = color.red;
        shape->stroke_g = color.green;
        shape->stroke_b = color.blue;
        shape->stroke_a = color.alpha;

        if (model_element->stroke_color) {
          g_free(model_element->stroke_color);
        }
        model_element->stroke_color = g_strdup_printf("#%02X%02X%02X%02X",
          (int)CLAMP(color.red * 255.0, 0, 255),
          (int)CLAMP(color.green * 255.0, 0, 255),
          (int)CLAMP(color.blue * 255.0, 0, 255),
          (int)CLAMP(color.alpha * 255.0, 0, 255));

        if (model_element->state != MODEL_STATE_NEW) {
          model_element->state = MODEL_STATE_UPDATED;
        }

        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_change_shape_stroke_color_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (!data || !data->model || !element_uuid) {
    return;
  }

  ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
  if (!model_element || !model_element->visual_element || model_element->type->type != ELEMENT_SHAPE) {
    return;
  }

  Shape *shape = (Shape*)model_element->visual_element;
  GtkRoot *root = gtk_widget_get_root(data->drawing_area);
  GtkWidget *window = GTK_WIDGET(root);
  if (!GTK_IS_WINDOW(window)) {
    return;
  }
  GtkWidget *dialog = gtk_color_chooser_dialog_new("Choose Stroke Color",
                                                   GTK_WINDOW(window));
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dialog), TRUE);

  GdkRGBA initial = { .red = shape->stroke_r, .green = shape->stroke_g, .blue = shape->stroke_b, .alpha = shape->stroke_a };
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &initial);

  g_object_set_data(G_OBJECT(dialog), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(dialog), "element_uuid", g_strdup(element_uuid), g_free);

  g_signal_connect(dialog, "response", G_CALLBACK(on_stroke_color_dialog_response), NULL);
  gtk_window_present(GTK_WINDOW(dialog));
}

static void on_change_arrow_type_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");
  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->visual_element->type == ELEMENT_CONNECTION) {
      Connection *conn = (Connection*)model_element->visual_element;

      // Cycle through arrow types
      conn->connection_type = (conn->connection_type + 1) % 2;

      // Update the model element too
      model_element->connection_type = conn->connection_type;

      // Redraw the canvas
      gtk_widget_queue_draw(data->drawing_area);

      // Mark as modified for saving
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

static void on_change_arrowhead_type_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");
  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->visual_element->type == ELEMENT_CONNECTION) {
      Connection *conn = (Connection*)model_element->visual_element;

      // Cycle through arrowhead types
      conn->arrowhead_type = (conn->arrowhead_type + 1) % 3;

      // Update the model element too
      model_element->arrowhead_type = conn->arrowhead_type;

      // Redraw the canvas
      gtk_widget_queue_draw(data->drawing_area);

      // Mark as modified for saving
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

static void on_change_text_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *canvas_data = g_object_get_data(G_OBJECT(action), "canvas_data");
  char *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");
  Element *element = NULL;
  ModelElement *model_element = g_hash_table_lookup(canvas_data->model->elements, element_uuid);
  element = model_element->visual_element;
  font_dialog_open(canvas_data, element);
}

static void on_hide_children_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_hide_children(data, element_uuid);
    gtk_widget_queue_draw(data->drawing_area);
  }
}

static void on_show_children_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_show_children(data, element_uuid);
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void canvas_on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (n_press == 1) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);
    Element *element = canvas_pick_element(data, cx, cy);

    if (element) {
      ModelElement *model_element = model_get_by_visual(data->model, element);

      if (model_element) {
        // Create an action group for the menu
        GSimpleActionGroup *action_group = g_simple_action_group_new();

        // Create delete action
        GSimpleAction *delete_action = g_simple_action_new("delete", NULL);
        g_object_set_data(G_OBJECT(delete_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(delete_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_signal_connect(delete_action, "activate", G_CALLBACK(on_delete_element_action), NULL);

        // Create description action
        GSimpleAction *description_action = g_simple_action_new("description", NULL);
        g_object_set_data(G_OBJECT(description_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(description_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_signal_connect(description_action, "activate", G_CALLBACK(on_description_action), NULL);

        GSimpleAction *change_color_action = g_simple_action_new("change-color", NULL);
        g_object_set_data(G_OBJECT(change_color_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        GSimpleAction *clone_action = g_simple_action_new("clone", NULL);
        GSimpleAction *change_space_action = g_simple_action_new("change-space", NULL);
        GSimpleAction *change_text_action = g_simple_action_new("change-text", NULL);

        // Hide/show children actions
        GSimpleAction *hide_children_action = g_simple_action_new("hide-children", NULL);
        GSimpleAction *show_children_action = g_simple_action_new("show-children", NULL);

        // Store data for existing actions
        g_object_set_data(G_OBJECT(change_space_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_space_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(clone_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(clone_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(change_text_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_text_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(hide_children_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(hide_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(show_children_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(show_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        // Connect existing actions
        g_signal_connect(clone_action, "activate", G_CALLBACK(on_clone_action), NULL);
        g_signal_connect(change_color_action, "activate", G_CALLBACK(on_change_color_action), NULL);
        g_signal_connect(change_space_action, "activate", G_CALLBACK(on_change_space_action), NULL);
        g_signal_connect(change_text_action, "activate", G_CALLBACK(on_change_text_action), NULL);
        g_signal_connect(hide_children_action, "activate", G_CALLBACK(on_hide_children_action), NULL);
        g_signal_connect(show_children_action, "activate", G_CALLBACK(on_show_children_action), NULL);

        // Add all actions to the action group
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(delete_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(description_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_color_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_space_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_text_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(hide_children_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(show_children_action));

        GSimpleAction *change_shape_style_action = NULL;
        GSimpleAction *change_shape_stroke_color_action = NULL;

        if (element->type == ELEMENT_SHAPE) {
          change_shape_style_action = g_simple_action_new("change-shape-style", NULL);
          g_object_set_data(G_OBJECT(change_shape_style_action), "canvas_data", data);
          g_object_set_data_full(G_OBJECT(change_shape_style_action), "element_uuid", g_strdup(model_element->uuid), g_free);
          g_signal_connect(change_shape_style_action, "activate", G_CALLBACK(on_change_shape_style_action), NULL);
          g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_shape_style_action));

          change_shape_stroke_color_action = g_simple_action_new("change-shape-stroke-color", NULL);
          g_object_set_data(G_OBJECT(change_shape_stroke_color_action), "canvas_data", data);
          g_object_set_data_full(G_OBJECT(change_shape_stroke_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);
          g_signal_connect(change_shape_stroke_color_action, "activate", G_CALLBACK(on_change_shape_stroke_color_action), NULL);
          g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_shape_stroke_color_action));
        }

        // Connection-specific actions
        GSimpleAction *change_arrow_type_action = NULL;
        GSimpleAction *change_arrowhead_type_action = NULL;
        if (element->type == ELEMENT_CONNECTION) {
          change_arrow_type_action = g_simple_action_new("change-arrow-type", NULL);
          g_object_set_data(G_OBJECT(change_arrow_type_action), "canvas_data", data);
          g_object_set_data_full(G_OBJECT(change_arrow_type_action), "element_uuid", g_strdup(model_element->uuid), g_free);
          g_signal_connect(change_arrow_type_action, "activate", G_CALLBACK(on_change_arrow_type_action), NULL);
          g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_arrow_type_action));

          change_arrowhead_type_action = g_simple_action_new("change-arrowhead-type", NULL);
          g_object_set_data(G_OBJECT(change_arrowhead_type_action), "canvas_data", data);
          g_object_set_data_full(G_OBJECT(change_arrowhead_type_action), "element_uuid", g_strdup(model_element->uuid), g_free);
          g_signal_connect(change_arrowhead_type_action, "activate", G_CALLBACK(on_change_arrowhead_type_action), NULL);
          g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_arrowhead_type_action));
        }

        // Build the context menu with visual separators between sections
        GMenu *menu_model = g_menu_new();
        GMenu *modify_section = g_menu_new();
        GMenu *structure_section = g_menu_new();
        GMenu *clone_section = g_menu_new();
        GMenu *info_section = g_menu_new();
        GMenu *danger_section = g_menu_new();

        g_menu_append(modify_section, "Change Space", "menu.change-space");

        if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE  ||
            element->type == ELEMENT_SPACE || element->type == ELEMENT_MEDIA_FILE ||
            element->type == ELEMENT_SHAPE || element->type == ELEMENT_INLINE_TEXT) {
          g_menu_append(modify_section, "Change Text", "menu.change-text");
        }

        // Only show "Change Color" if:
        // - Not a shape, OR
        // - A shape that is filled AND not LINE/ARROW/BEZIER
        gboolean show_bg_color = TRUE;
        if (element->type == ELEMENT_SHAPE) {
          Shape *shape = (Shape *)element;
          if (!shape->filled ||
              shape->shape_type == SHAPE_LINE ||
              shape->shape_type == SHAPE_ARROW ||
              shape->shape_type == SHAPE_BEZIER) {
            show_bg_color = FALSE;
          }
        }
        if (show_bg_color) {
          g_menu_append(modify_section, "Change Color", "menu.change-color");
        }

        if (element->type == ELEMENT_SHAPE) {
          g_menu_append(modify_section, "Change Shape Style", "menu.change-shape-style");
          g_menu_append(modify_section, "Change Stroke Color", "menu.change-shape-stroke-color");
        }

        if (element->type == ELEMENT_CONNECTION) {
          g_menu_append(modify_section, "Change Arrow Type", "menu.change-arrow-type");
          g_menu_append(modify_section, "Change Arrowhead", "menu.change-arrowhead-type");
        }

        // Add hide/show children options if element has children (outgoing arrows)
        GList *children = find_children_bfs(data->model, model_element->uuid);
        if (children) { // Has children via outgoing arrows
          gboolean has_hidden_children = canvas_has_hidden_children(data, model_element->uuid);

          if (has_hidden_children) {
            g_menu_append(structure_section, "Show Children", "menu.show-children");
          } else {
            g_menu_append(structure_section, "Hide Children", "menu.hide-children");
          }
          g_list_free(children);
        }

        g_menu_append(clone_section, "Clone", "menu.clone");

        g_menu_append(info_section, "Description", "menu.description");
        g_menu_append(danger_section, "Delete", "menu.delete");

        append_section_if_not_empty(menu_model, modify_section);
        append_section_if_not_empty(menu_model, structure_section);
        append_section_if_not_empty(menu_model, clone_section);
        append_section_if_not_empty(menu_model, info_section);
        append_section_if_not_empty(menu_model, danger_section);

        // Create the popover menu
        GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu_model));

        // Add the action group to the popover
        gtk_widget_insert_action_group(popover, "menu", G_ACTION_GROUP(action_group));

        // Set the parent and position
        gtk_widget_set_parent(popover, data->drawing_area);
        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &(GdkRectangle){x, y, 1, 1});
        gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);

        // Store references for cleanup
        g_object_set_data_full(G_OBJECT(popover), "action_group", action_group, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "menu_model", menu_model, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "delete_action", delete_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "clone_action", clone_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_space_action", change_space_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_color_action", change_color_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_text_action", change_text_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "hide_children_action", hide_children_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "show_children_action", show_children_action, g_object_unref);
        if (change_shape_style_action) {
          g_object_set_data_full(G_OBJECT(popover), "change_shape_style_action", change_shape_style_action, g_object_unref);
        }
        if (change_shape_stroke_color_action) {
          g_object_set_data_full(G_OBJECT(popover), "change_shape_stroke_color_action", change_shape_stroke_color_action, g_object_unref);
        }
        if (change_arrow_type_action) {
          g_object_set_data_full(G_OBJECT(popover), "change_arrow_type_action", change_arrow_type_action, g_object_unref);
        }
        if (change_arrowhead_type_action) {
          g_object_set_data_full(G_OBJECT(popover), "change_arrowhead_type_action", change_arrowhead_type_action, g_object_unref);
        }

        // Connect the closed signal with deferred cleanup
        g_signal_connect(popover, "closed", G_CALLBACK(on_popover_closed), NULL);

        // Show the popover
        gtk_popover_popup(GTK_POPOVER(popover));
      }
    } else {
      data->panning = TRUE;
      data->pan_start_x = (int)x;
      data->pan_start_y = (int)y;
      canvas_set_cursor(data, gdk_cursor_new_from_name("grabbing", NULL));
    }
  }
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

  GList *visual_elements = canvas_get_visual_elements(data);

  // Check if any inline text is editing - if so, let TextView handle all input
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    if (element->type == ELEMENT_INLINE_TEXT && ((InlineText*)element)->editing) {
      // Let the TextView handle all input - don't consume keys here
      return FALSE;
    }
  }

  if (keyval == GDK_KEY_F1 && (state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) == 0) {
    canvas_show_shortcuts_dialog(data);
    return TRUE;
  }

  // Add Ctrl+C for copying selected elements
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_c) {
    if (data->selected_elements) {
      // Clear previous copied elements (don't use g_list_free_full since we don't own the ModelElements)
      if (data->copied_elements) {
        g_list_free(data->copied_elements);
        data->copied_elements = NULL;
      }

      // Copy all selected elements
      for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element) {
          data->copied_elements = g_list_append(data->copied_elements, model_element);
        }
      }

      int count = g_list_length(data->copied_elements);
      char message[64];
      snprintf(message, sizeof(message), "%d element%s copied", count, count == 1 ? "" : "s");
      canvas_show_notification(data, message);
    }
    return TRUE;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_v) {
    canvas_on_paste(data->drawing_area, data);
    return TRUE;
  }

  // Add Ctrl+S for search
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_s) {
    canvas_show_search_dialog(NULL, data);
    return TRUE;
  }

  // Add Ctrl+Shift+N for new note
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_N) {
    canvas_on_add_note(NULL, data);
    return TRUE;
  }

  // Add Ctrl+N for new note
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_n) {
    canvas_on_add_text(NULL, data);
    return TRUE;
  }

  // Add Ctrl+E for DSL executor window
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_e) {
    canvas_show_script_dialog(NULL, data);
    return TRUE;
  }

  // Add Ctrl+D for toggling drawing mode
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_d) {
    canvas_toggle_drawing_mode(NULL, data);
    return TRUE;
  }

  // Add Ctrl+L for shape library
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_l) {
    canvas_show_shape_selection_dialog(NULL, data);
    return TRUE;
  }

  // Add Ctrl+Shift+P for new paper note
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_P) {
    canvas_on_add_paper_note(NULL, data);
    return TRUE;
  }

  // Add Ctrl+Shift+S for new space
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_S) {
    canvas_on_add_space(NULL, data);
    return TRUE;
  }

  // Add Ctrl+T for toolbar toggle
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_t) {
    extern void toggle_toolbar_visibility(CanvasData *data);
    toggle_toolbar_visibility(data);
    return TRUE;
  }

  // Add Ctrl+Shift+T for toolbar auto-hide toggle
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_T) {
    extern void toggle_toolbar_auto_hide(CanvasData *data);
    toggle_toolbar_auto_hide(data);
    return TRUE;
  }

  // Add Ctrl+J for tree view toggle
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_j) {
    if (data->tree_scrolled) {
      gboolean is_visible = gtk_widget_get_visible(data->tree_scrolled);
      gtk_widget_set_visible(data->tree_scrolled, !is_visible);
      data->tree_view_visible = !is_visible;
    }
    return TRUE;
  }

  // Add undo/redo keyboard shortcuts
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_z) on_undo_clicked(NULL, data);
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_y) on_redo_clicked(NULL, data);

  // Add Ctrl+A for select all elements (when not editing)
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_a) {
    // Clear existing selection first
    canvas_clear_selection(data);

    // Get all visual elements in current space
    GList *visual_elements = canvas_get_visual_elements(data);

    // Add all elements to selection
    for (GList *l = visual_elements; l != NULL; l = l->next) {
      Element *element = (Element*)l->data;
      // Skip hidden elements from selection
      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && !canvas_is_element_hidden(data, model_element->uuid)) {
        data->selected_elements = g_list_append(data->selected_elements, element);
      }
    }

    // Free the list (elements are still valid, we just copied the pointers)
    g_list_free(visual_elements);

    // Redraw to show selection
    gtk_widget_queue_draw(data->drawing_area);
    return TRUE;
  }

  // Add Delete key for deleting selected elements (when not editing)
  if (keyval == GDK_KEY_Delete) {
    if (data->selected_elements) {
      // Create a copy of the list since we'll be modifying the original during deletion
      GList *elements_to_delete = g_list_copy(data->selected_elements);

      for (GList *l = elements_to_delete; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        ModelElement *model_element = model_get_by_visual(data->model, element);

        if (model_element) {
          // Check if it's a space with elements (same logic as single delete)
          if (model_element->type->type == ELEMENT_SPACE) {
            if (model_element->state != MODEL_STATE_NEW) {
              int element_count = model_get_amount_of_elements(data->model, model_element->target_space_uuid);
              if (element_count > 0) {
                // Skip deletion of non-empty spaces, but continue with other elements
                continue;
              }
            }
          }

          // Push undo action BEFORE deletion
          undo_manager_push_delete_action(data->undo_manager, model_element);

          // Delete the element
          model_delete_element(data->model, model_element);
        }
      }

      g_list_free(elements_to_delete);

      // Clear selection and sync with model
      canvas_clear_selection(data);
      canvas_sync_with_model(data);
      gtk_widget_queue_draw(data->drawing_area);
    }
    return TRUE;
  }

  // Add backspace navigation to parent space (when not editing)
  if (keyval == GDK_KEY_BackSpace) {
    go_back_to_parent_space(data);
    return TRUE;
  }

  return FALSE;
}

gboolean canvas_on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Calculate zoom factor (positive dy = zoom out, negative dy = zoom in)
  const double zoom_speed = 0.1;
  double zoom_factor = 1.0;

  if (dy > 0) {
    // Zoom out
    zoom_factor = 1.0 - zoom_speed;
  } else if (dy < 0) {
    // Zoom in
    zoom_factor = 1.0 + zoom_speed;
  }

  // Limit zoom range (0.1x to 10x)
  double new_zoom = data->zoom_scale * zoom_factor;
  if (new_zoom < 0.1) new_zoom = 0.1;
  if (new_zoom > 10.0) new_zoom = 10.0;

  if (new_zoom != data->zoom_scale) {
    double cursor_x = data->last_mouse_x;
    double cursor_y = data->last_mouse_y;

    int canvas_point_x, canvas_point_y;
    canvas_screen_to_canvas(data, (int)cursor_x, (int)cursor_y, &canvas_point_x, &canvas_point_y);

    data->zoom_scale = new_zoom;

    data->offset_x = (cursor_x / new_zoom) - canvas_point_x;
    data->offset_y = (cursor_y / new_zoom) - canvas_point_y;

    canvas_update_zoom_entry(data);
    gtk_widget_queue_draw(data->drawing_area);
  }

  return TRUE; // Event handled
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
