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
#include "shape.h"
#include "font_dialog.h"
#include "shape.h"

void canvas_on_left_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  // Handle shape mode separately from drawing mode to avoid conflicts
  if (data->shape_mode) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

    // Handle shape creation
    if (!data->current_shape) {
      ElementPosition position = { cx, cy, data->next_z_index++ };
      ElementSize size = { 0, 0 };  // Initial size
      ElementText text = {
        .text = "",
        .text_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 },
        .font_description = "Ubuntu Mono 12"
      };
      ElementShape shape_config = {
        .shape_type = data->selected_shape_type,
        .stroke_width = data->drawing_stroke_width,
        .filled = data->shape_filled
      };
      data->current_shape = shape_create(position, size, data->drawing_color,
                                         data->drawing_stroke_width,
                                         data->selected_shape_type,
                                         data->shape_filled, text, shape_config, data);
      data->shape_start_x = cx;
      data->shape_start_y = cy;
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

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
          (element->type == ELEMENT_SHAPE && ((Shape*)element)->editing))) {
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

void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Store last mouse position for zoom-at-cursor functionality
  data->last_mouse_x = x;
  data->last_mouse_y = y;

  GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

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
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

    int x1 = data->shape_start_x;
    int y1 = data->shape_start_y;

    data->current_shape->base.x = MIN(x1, cx);
    data->current_shape->base.y = MIN(y1, cy);
    data->current_shape->base.width = MAX(ABS(cx - x1), 10);
    data->current_shape->base.height = MAX(ABS(cy - y1), 10);

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && data->current_drawing) {
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

    ModelElement *model_element = model_create_element(data->model, config);

    if (model_element) {
      model_element->visual_element = create_visual_element(model_element, data);
      undo_manager_push_create_action(data->undo_manager, model_element);
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

    // Skip hidden elements from picking
    ModelElement *model_element = model_get_by_visual(data->model, element);
    if (model_element && canvas_is_element_hidden(data, model_element->uuid)) {
      continue;
    }

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
      case 0: case 2: // Top-left, Bottom-right
        canvas_set_cursor(data, gdk_cursor_new_from_name("nwse-resize", NULL));
        break;
      case 1: case 3: // Top-right, Bottom-left
        canvas_set_cursor(data, gdk_cursor_new_from_name("nesw-resize", NULL));
        break;
      }
      return;
    }

    int cp = element_pick_connection_point(element, x, y);
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

        // Create description action
        GSimpleAction *description_action = g_simple_action_new("description", NULL);
        g_object_set_data(G_OBJECT(description_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(description_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_signal_connect(description_action, "activate", G_CALLBACK(on_description_action), NULL);

        GSimpleAction *change_color_action = g_simple_action_new("change-color", NULL);
        g_object_set_data(G_OBJECT(change_color_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(change_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        GSimpleAction *fork_action = g_simple_action_new("fork", NULL);
        GSimpleAction *clone_text_action = g_simple_action_new("clone-text", NULL);
        GSimpleAction *clone_size_action = g_simple_action_new("clone-size", NULL);
        GSimpleAction *change_space_action = g_simple_action_new("change-space", NULL);
        GSimpleAction *change_text_action = g_simple_action_new("change-text", NULL);

        // Hide/show children actions
        GSimpleAction *hide_children_action = g_simple_action_new("hide-children", NULL);
        GSimpleAction *show_children_action = g_simple_action_new("show-children", NULL);

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
        g_object_set_data(G_OBJECT(hide_children_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(hide_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);
        g_object_set_data(G_OBJECT(show_children_action), "canvas_data", data);
        g_object_set_data_full(G_OBJECT(show_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);

        // Connect existing actions
        g_signal_connect(fork_action, "activate", G_CALLBACK(on_fork_element_action), NULL);
        g_signal_connect(clone_text_action, "activate", G_CALLBACK(on_clone_by_text_action), NULL);
        g_signal_connect(clone_size_action, "activate", G_CALLBACK(on_clone_by_size_action), NULL);
        g_signal_connect(change_color_action, "activate", G_CALLBACK(on_change_color_action), NULL);
        g_signal_connect(change_space_action, "activate", G_CALLBACK(on_change_space_action), NULL);
        g_signal_connect(change_text_action, "activate", G_CALLBACK(on_change_text_action), NULL);
        g_signal_connect(hide_children_action, "activate", G_CALLBACK(on_hide_children_action), NULL);
        g_signal_connect(show_children_action, "activate", G_CALLBACK(on_show_children_action), NULL);

        // Add all actions to the action group
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(delete_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(description_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(fork_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_text_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_size_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_color_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_space_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_text_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(hide_children_action));
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(show_children_action));

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

        // Create the menu model with delete option first
        GMenu *menu_model = g_menu_new();
        g_menu_append(menu_model, "Change Space", "menu.change-space");
        g_menu_append(menu_model, "Change Color", "menu.change-color");

        if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE  ||
            element->type == ELEMENT_SPACE || element->type == ELEMENT_MEDIA_FILE ||
            element->type == ELEMENT_SHAPE) {
          g_menu_append(menu_model, "Change Text", "menu.change-text");
          g_menu_append(menu_model, "Clone by Text", "menu.clone-text");
        }
        if (element->type == ELEMENT_CONNECTION) {
          g_menu_append(menu_model, "Change Arrow Type", "menu.change-arrow-type");
          g_menu_append(menu_model, "Change Arrowhead", "menu.change-arrowhead-type");
        }
        // Add hide/show children options if element has children (outgoing arrows)
        GList *children = find_children_bfs(data->model, model_element->uuid);
        if (children) { // Has children via outgoing arrows
          gboolean has_hidden_children = canvas_has_hidden_children(data, model_element->uuid);

          if (has_hidden_children) {
            g_menu_append(menu_model, "Show Children", "menu.show-children");
          } else {
            g_menu_append(menu_model, "Hide Children", "menu.hide-children");
          }
          g_list_free(children);
        }

        g_menu_append(menu_model, "Clone by Size", "menu.clone-size");
        g_menu_append(menu_model, "Fork Element", "menu.fork");
        g_menu_append(menu_model, "Description", "menu.description");
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
        g_object_set_data_full(G_OBJECT(popover), "hide_children_action", hide_children_action, g_object_unref);
        g_object_set_data_full(G_OBJECT(popover), "show_children_action", show_children_action, g_object_unref);
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
        (element->type == ELEMENT_NOTE && ((Note*)element)->editing) ||
        (element->type == ELEMENT_SHAPE && ((Shape*)element)->editing)) {
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
