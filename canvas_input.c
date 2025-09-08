#include "canvas_input.h"
#include "canvas_core.h"
#include "canvas_spaces.h"
#include "model.h"
#include "paper_note.h"
#include "note.h"
#include "connection.h"
#include "space.h"
#include <pango/pangocairo.h>

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

void canvas_on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    static Element *connection_start = NULL;
    static int connection_start_point = -1;

    GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
    if (event) {
        data->modifier_state = gdk_event_get_modifier_state(event);
    }

    Element *element = canvas_pick_element(data, (int)x, (int)y);

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
            element->orig_width = element->width;
            element->orig_height = element->height;

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
                        // Create connection in the model
                        int z = MAX(from_model->position->z, to_model->position->z);
                        ModelElement *model_conn = model_create_connection(data->model, from_model->uuid, to_model->uuid, connection_start_point, cp, z);
                        model_conn->visual_element = create_visual_element(model_conn, data);
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
              (element->type == ELEMENT_NOTE && ((Note*)element)->editing))) {
            if (!(data->modifier_state & GDK_SHIFT_MASK)) {
                canvas_clear_selection(data);
            }
            if (!canvas_is_element_selected(data, element)) {
                data->selected_elements = g_list_append(data->selected_elements, element);
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
    canvas_update_cursor(data, (int)x, (int)y);
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

            element_update_position(element, new_x, new_y, element->z);
            element_update_size(element, new_width, new_height);

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
                element_update_position(selected_element, new_x, new_y, element->z);
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

void canvas_on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    if (data->selecting) {
        data->selecting = FALSE;
        int sel_x = MIN(data->start_x, data->current_x);
        int sel_y = MIN(data->start_y, data->current_y);
        int sel_width = ABS(data->current_x - data->start_x);
        int sel_height = ABS(data->current_y - data->start_y);

        GList *visual_elements = canvas_get_visual_elements(data);

        for (GList *iter = visual_elements; iter != NULL; iter = iter->next) {
            Element *element = (Element*)iter->data;

            // Skip hidden elements
            if (element->hidden) {
                continue;
            }

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

    gboolean was_resized = FALSE;
    GList *visual_elements = canvas_get_visual_elements(data);
    for (GList *l = visual_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        if (element->resizing) {
          was_resized = TRUE;
        }
        element->dragging = FALSE;
        element->resizing = FALSE;
    }

    if (was_resized) {
      // Re-create visual elements since some sizes may be changed due to resizing of cloned element
      GList *sorted_elements = sort_model_elements_for_serialization(data->model->elements);
      create_visual_elements_from_sorted_list(sorted_elements, data);
      g_list_free(sorted_elements);
    }

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    canvas_set_cursor(data, data->default_cursor);
}

Element* canvas_pick_element(CanvasData *data, int x, int y) {
    Element *selected_element = NULL;
    int highest_z_index = -1;

    GList *visual_elements = canvas_get_visual_elements(data);
    for (GList *l = visual_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;

        // Skip hidden elements
        if (element->hidden) {
            continue;
        }

        if (x >= element->x && x <= element->x + element->width &&
            y >= element->y && y <= element->y + element->height) {
            if (element->z > highest_z_index) {
                selected_element = element;
                highest_z_index = element->z;
            }
        }
    }
    return selected_element;
}

void canvas_update_cursor(CanvasData *data, int x, int y) {
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
                gtk_widget_queue_draw(data->drawing_area);
            }
        }
    }
}

// Add this helper function
static gboolean destroy_popover_callback(gpointer user_data) {
    GtkWidget *popover = user_data;
    gtk_widget_unparent(popover);
    return G_SOURCE_REMOVE;
}

static void on_popover_closed(GtkPopover *popover, gpointer user_data) {
    // Defer the destruction until after the action handlers have finished
    g_idle_add(destroy_popover_callback, popover);
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

                // Create actions - use g_strdup for the UUID
                GSimpleAction *fork_action = g_simple_action_new("fork", NULL);
                GSimpleAction *clone_text_action = g_simple_action_new("clone-text", NULL);
                GSimpleAction *clone_size_action = g_simple_action_new("clone-size", NULL);

                // Store the necessary data in the actions with proper string duplication
                g_object_set_data(G_OBJECT(fork_action), "canvas_data", data);
                g_object_set_data_full(G_OBJECT(fork_action), "element_uuid", g_strdup(model_element->uuid), g_free);

                g_object_set_data(G_OBJECT(clone_text_action), "canvas_data", data);
                g_object_set_data_full(G_OBJECT(clone_text_action), "element_uuid", g_strdup(model_element->uuid), g_free);

                g_object_set_data(G_OBJECT(clone_size_action), "canvas_data", data);
                g_object_set_data_full(G_OBJECT(clone_size_action), "element_uuid", g_strdup(model_element->uuid), g_free);

                // Connect the actions to their handlers
                g_signal_connect(fork_action, "activate", G_CALLBACK(on_fork_element_action), NULL);
                g_signal_connect(clone_text_action, "activate", G_CALLBACK(on_clone_by_text_action), NULL);
                g_signal_connect(clone_size_action, "activate", G_CALLBACK(on_clone_by_size_action), NULL);

                // Add actions to the action group
                g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(fork_action));
                g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_text_action));
                g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_size_action));

                // Create the menu model
                GMenu *menu_model = g_menu_new();
                g_menu_append(menu_model, "Fork Element", "menu.fork");
                g_menu_append(menu_model, "Clone by Text", "menu.clone-text");
                g_menu_append(menu_model, "Clone by Size", "menu.clone-size");

                // Create the popover menu
                GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu_model));

                // Add the action group to the popover
                gtk_widget_insert_action_group(popover, "menu", G_ACTION_GROUP(action_group));

                // Set the parent and position
                gtk_widget_set_parent(popover, data->drawing_area);
                gtk_popover_set_pointing_to(GTK_POPOVER(popover), &(GdkRectangle){x, y, 1, 1});
                gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);

                // Store references for cleanup when the popover is destroyed
                g_object_set_data_full(G_OBJECT(popover), "action_group", action_group, g_object_unref);
                g_object_set_data_full(G_OBJECT(popover), "menu_model", menu_model, g_object_unref);
                g_object_set_data_full(G_OBJECT(popover), "fork_action", fork_action, g_object_unref);
                g_object_set_data_full(G_OBJECT(popover), "clone_text_action", clone_text_action, g_object_unref);
                g_object_set_data_full(G_OBJECT(popover), "clone_size_action", clone_size_action, g_object_unref);

                // Connect the closed signal with deferred cleanup
                g_signal_connect(popover, "closed", G_CALLBACK(on_popover_closed), NULL);

                // Show the popover
                gtk_popover_popup(GTK_POPOVER(popover));
            }
        }
    }
}
