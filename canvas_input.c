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
#include "connection.h"
#include "space.h"
#include <pango/pangocairo.h>
#include <gtk/gtkdialog.h>
#include "undo_manager.h"
#include "dsl_executor.h"
#include "freehand_drawing.h"
#include "font_dialog.h"

void canvas_on_left_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  static Element *connection_start = NULL;
  static int connection_start_point = -1;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  if (data->drawing_mode) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

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

  Element *element = canvas_pick_element(data, (int)x, (int)y);

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
    int rh = element_pick_resize_handle(element, (int)x, (int)y);
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
      element->resize_start_x = (int)x;
      element->resize_start_y = (int)y;
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

    int cp = element_pick_connection_point(element, (int)x, (int)y);
    if (cp >= 0) {
      if (!connection_start) {
        connection_start = element;
        connection_start_point = cp;
      } else {
        if (element != connection_start) {
          // Use helper function to get model elements
          ModelElement *from_model = model_get_by_visual(data->model, connection_start);
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
              .from_element_uuid = from_model->uuid,
              .to_element_uuid = to_model->uuid,
              .from_point = connection_start_point,
              .to_point = cp,
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
        connection_start = NULL;
        connection_start_point = -1;
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
          (element->type == ELEMENT_NOTE && ((Note*)element)->editing))) {
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
      element->drag_offset_x = (int)x - element->x;
      element->drag_offset_y = (int)y - element->y;
    }
  } else {
    connection_start = NULL;
    connection_start_point = -1;

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

void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  if (data->drawing_mode) {
    if (data->modifier_state & GDK_SHIFT_MASK) {
      canvas_set_cursor(data, data->line_cursor);
    } else {
      canvas_set_cursor(data, data->draw_cursor);
    }
  }

  if (data->drawing_mode && data->current_drawing) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

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

  canvas_update_cursor(data, (int)x, (int)y);

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

    if (element->resizing) {
      int dx = (int)x - element->resize_start_x;
      int dy = (int)y - element->resize_start_y;
      int new_x = element->x;
      int new_y = element->y;
      int new_width = element->width;
      int new_height = element->height;

      switch (element->resize_edge) {
      case 0:
        new_x = element->orig_x + dx;
        new_y = element->orig_y + dy;
        new_width = element->orig_width - dx;
        new_height = element->orig_height - dy;
        break;
      case 1:
        new_y = element->orig_y + dy;
        new_width = element->orig_width + dx;
        new_height = element->orig_height - dy;
        break;
      case 2:
        new_width = element->orig_width + dx;
        new_height = element->orig_height + dy;
        break;
      case 3:
        new_x = element->orig_x + dx;
        new_width = element->orig_width - dx;
        new_height = element->orig_height + dy;
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

    if (element->dragging) {
      int dx = (int)x - element->x - element->drag_offset_x;
      int dy = (int)y - element->y - element->drag_offset_y;

      for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
        Element *selected_element = (Element*)sel->data;
        int new_x = selected_element->x + dx;
        int new_y = selected_element->y + dy;

        // Only update visual position during drag, not model
        selected_element->x = new_x;
        selected_element->y = new_y;
      }

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

  if (data->drawing_mode && data->current_drawing) {
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
    element->dragging = FALSE;
    element->resizing = FALSE;
  }

  // Re-create visual elements since some sizes may be changed due to resizing
  if (was_resized) canvas_sync_with_model(data);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  canvas_set_cursor(data, data->default_cursor);
}

Element* canvas_pick_element(CanvasData *data, int x, int y) {
  int cx, cy;
  canvas_screen_to_canvas(data, x, y, &cx, &cy);

  Element *selected_element = NULL;
  int highest_z_index = -1;

  GList *visual_elements = canvas_get_visual_elements(data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;

    if (cx >= element->x && cx <= element->x + element->width &&
        cy >= element->y && cy <= element->y + element->height) {
      if (element->z > highest_z_index) {
        selected_element = element;
        highest_z_index = element->z;
      }
    }
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

  Element *element = canvas_pick_element(data, x, y);

  if (element) {
    int rh = element_pick_resize_handle(element, x, y);
    if (rh >= 0) {
      switch (rh) {
      case 0: case 2:
        canvas_set_cursor(data, gdk_cursor_new_from_name("nwse-resize", NULL));
        break;
      case 1: case 3:
        canvas_set_cursor(data, gdk_cursor_new_from_name("nesw-resize", NULL));
        break;
      }
      return;
    }

    int cp = element_pick_connection_point(element, x, y);
    if (cp >= 0) {
      canvas_set_cursor(data, gdk_cursor_new_from_name("crosshair", NULL));
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

static void on_fork_element_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    model_save_elements(data->model);
    ModelElement *original = g_hash_table_lookup(data->model->elements, element_uuid);
    if (original) {
      ModelElement *forked = model_element_fork(data->model, original);
      if (forked) {
        forked->visual_element = create_visual_element(forked, data);
        // Push undo action for fork
        undo_manager_push_create_action(data->undo_manager, forked);
        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }
}

static void on_clone_by_text_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    model_save_elements(data->model);
    ModelElement *original = g_hash_table_lookup(data->model->elements, element_uuid);
    if (original) {
      ModelElement *clone = model_element_clone_by_text(data->model, original);
      if (clone) {
        clone->visual_element = create_visual_element(clone, data);
        // Push undo action for clone
        undo_manager_push_create_action(data->undo_manager, clone);
        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }
}

static void on_clone_by_size_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    model_save_elements(data->model);
    ModelElement *original = g_hash_table_lookup(data->model->elements, element_uuid);
    if (original) {
      ModelElement *clone = model_element_clone_by_size(data->model, original);
      if (clone) {
        clone->visual_element = create_visual_element(clone, data);
        // Push undo action for clone
        undo_manager_push_create_action(data->undo_manager, clone);
        gtk_widget_queue_draw(data->drawing_area);
      }
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
            g_print("Only empty space is allowed for deletion for now\n");
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

static void on_change_text_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *canvas_data = g_object_get_data(G_OBJECT(action), "canvas_data");
  char *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");
  Element *element = NULL;
  ModelElement *model_element = g_hash_table_lookup(canvas_data->model->elements, element_uuid);
  element = model_element->visual_element;
  font_dialog_open(canvas_data, element);
}

void canvas_on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (n_press == 1) {
    Element *element = canvas_pick_element(data, (int)x, (int)y);

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

        GSimpleAction *change_color_action = g_simple_action_new("change-color", NULL);
        g_object_set_data(G_OBJECT(change_color_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        GSimpleAction *fork_action = g_simple_action_new("fork", NULL);
        GSimpleAction *clone_text_action = g_simple_action_new("clone-text", NULL);
        GSimpleAction *clone_size_action = g_simple_action_new("clone-size", NULL);
        GSimpleAction *change_space_action = g_simple_action_new("change-space", NULL);
        GSimpleAction *change_text_action = g_simple_action_new("change-text", NULL);

        // Store data for existing actions
        g_object_set_data(G_OBJECT(change_space_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_space_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(fork_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(fork_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(clone_text_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(clone_text_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(clone_size_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(clone_size_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(change_text_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_text_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        // Connect existing actions
        g_signal_connect(fork_action, "activate", G_CALLBACK(on_fork_element_action), NULL);
        g_signal_connect(clone_text_action, "activate", G_CALLBACK(on_clone_by_text_action), NULL);
        g_signal_connect(clone_size_action, "activate", G_CALLBACK(on_clone_by_size_action), NULL);
        g_signal_connect(change_color_action, "activate", G_CALLBACK(on_change_color_action), NULL);
        g_signal_connect(change_space_action, "activate", G_CALLBACK(on_change_space_action), NULL);
        g_signal_connect(change_text_action, "activate", G_CALLBACK(on_change_text_action), NULL);

        // Add all actions to the action group
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(delete_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(fork_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_text_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_size_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_color_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_space_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_text_action));

        // Create the menu model with delete option first
        GMenu *menu_model = g_menu_new();
        g_menu_append(menu_model, "Change Space", "menu.change-space");
        g_menu_append(menu_model, "Change Color", "menu.change-color");

        if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE  ||
            element->type == ELEMENT_SPACE || element->type == ELEMENT_MEDIA_FILE) {
          g_menu_append(menu_model, "Change Text", "menu.change-text");
        }

        if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE) {
          g_menu_append(menu_model, "Fork Element", "menu.fork");
          g_menu_append(menu_model, "Clone by Text", "menu.clone-text");
          g_menu_append(menu_model, "Clone by Size", "menu.clone-size");
        }

        g_menu_append(menu_model, "Delete", "menu.delete");

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
        g_object_set_data_full(G_OBJECT(popover), "fork_action", fork_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "clone_text_action", clone_text_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "clone_size_action", clone_size_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_space_action", change_space_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_color_action", change_color_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "change_text_action", change_text_action, g_object_unref);

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
    g_print("No image in clipboard or failed: %s\n",
            error ? error->message : "unknown");
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
      .font_description = g_strdup("Sans 10"),
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

void canvas_on_paste(GtkWidget *widget, CanvasData *data) {
  GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());

  if (!clipboard) {
    g_print("Failed to get clipboard\n");
    return;
  }

  // Async read texture from clipboard
  gdk_clipboard_read_texture_async(
                                   clipboard,
                                   NULL,                     // GCancellable
                                   on_clipboard_texture_ready, // Callback function
                                   data                       // user_data
                                   );
}

gboolean canvas_on_key_pressed(GtkEventControllerKey *controller, guint keyval,
                               guint keycode, GdkModifierType state, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Check if any element is currently being edited
  gboolean is_editing = FALSE;
  GList *visual_elements = canvas_get_visual_elements(data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    if ((element->type == ELEMENT_PAPER_NOTE && ((PaperNote*)element)->editing) ||
        (element->type == ELEMENT_MEDIA_FILE && ((MediaNote*)element)->editing) ||
        (element->type == ELEMENT_NOTE && ((Note*)element)->editing)) {
      is_editing = TRUE;
      break;
    }
  }

  // If editing, let the text widget handle Enter and other keys
  if (is_editing) {
    return FALSE;
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

  // Add Ctrl+N for new note
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_n) {
    canvas_on_add_note(NULL, data);
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

  // Add undo/redo keyboard shortcuts
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_z) on_undo_clicked(NULL, data);
  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_y) on_redo_clicked(NULL, data);

  return FALSE;
}
