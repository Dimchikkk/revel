#include "canvas.h"
#include "paper_note.h"
#include "note.h"
#include <pango/pangocairo.h>
#include "undo_manager.h"

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static gint compare_elements_by_z_index(gconstpointer a, gconstpointer b) {
    const Element *element_a = (const Element*)a;
    const Element *element_b = (const Element*)b;
    return element_a->z_index - element_b->z_index;
}

CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay) {
    CanvasData *data = g_new0(CanvasData, 1);
    data->selected_elements = NULL;
    data->drawing_area = drawing_area;
    data->overlay = overlay;
    data->next_z_index = 1;
    data->selecting = FALSE;
    data->modifier_state = 0;

    data->default_cursor = gdk_cursor_new_from_name("default", NULL);
    data->move_cursor = gdk_cursor_new_from_name("move", NULL);
    data->resize_cursor = gdk_cursor_new_from_name("nwse-resize", NULL);
    data->connect_cursor = gdk_cursor_new_from_name("crosshair", NULL);
    data->current_cursor = NULL;
    data->undo_manager = undo_manager_new();

    // Initialize undo tracking fields
    data->undo_original_width = 0;
    data->undo_original_height = 0;
    data->undo_original_x = 0;
    data->undo_original_y = 0;
    data->undo_original_positions = NULL;

    data->current_space = space_new("root", NULL);
    data->space_history = NULL;

    return data;
}

void canvas_data_free(CanvasData *data) {
    if (data->default_cursor) g_object_unref(data->default_cursor);
    if (data->move_cursor) g_object_unref(data->move_cursor);
    if (data->resize_cursor) g_object_unref(data->resize_cursor);
    if (data->connect_cursor) g_object_unref(data->connect_cursor);

    if (data->current_space && data->current_space->parent == NULL) {
        space_free(data->current_space);
    }

    g_list_free(data->selected_elements);

    // Free undo position data
    if (data->undo_original_positions) {
        g_list_free_full(data->undo_original_positions, g_free);
    }

    if (data->undo_manager) undo_manager_free(data->undo_manager);
    g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

        // Draw current space name in the top-left corner
    if (data->current_space && data->current_space->name) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text

        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 10");
        pango_layout_set_font_description(layout, font_desc);
        pango_font_description_free(font_desc);

        char space_info[100];
        if (data->current_space->parent) {
            snprintf(space_info, sizeof(space_info), "Space: %s (in %s)",
                     data->current_space->name, data->current_space->parent->name);
        } else {
            snprintf(space_info, sizeof(space_info), "Space: %s", data->current_space->name);
        }

        pango_layout_set_text(layout, space_info, -1);

        int text_width, text_height;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);

        // Draw semi-transparent background for better readability
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.7);  // White with transparency
        cairo_rectangle(cr, 10, 10, text_width + 10, text_height + 6);
        cairo_fill(cr);

        // Draw the text
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text
        cairo_move_to(cr, 15, 13);
        pango_cairo_show_layout(cr, layout);

        g_object_unref(layout);
    }

    GList *sorted_elements = g_list_copy(data->current_space->elements);
    sorted_elements = g_list_sort(sorted_elements, compare_elements_by_z_index);

    for (GList *l = sorted_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        if (!element->hidden) {
            element_draw(element, cr, canvas_is_element_selected(data, element));
        }
    }

    g_list_free(sorted_elements);

    if (data->selecting) {
        cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.3);
        cairo_rectangle(cr,
                        MIN(data->start_x, data->current_x),
                        MIN(data->start_y, data->current_y),
                        ABS(data->current_x - data->start_x),
                        ABS(data->current_y - data->start_y));
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    }
}

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
        // Switch to the space represented by this element
        SpaceElement *space_elem = (SpaceElement*)element;
        switch_to_space(data, space_elem->target_space);
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

            // STORE ORIGINAL STATE FOR UNDO
            data->undo_original_width = element->width;
            data->undo_original_height = element->height;
            data->undo_original_x = element->x;
            data->undo_original_y = element->y;

            return;
        }

        int cp = element_pick_connection_point(element, (int)x, (int)y);
        if (cp >= 0) {
            if (!connection_start) {
                connection_start = element;
                connection_start_point = cp;
            } else {
              if (element != connection_start) {
                    Connection *conn = connection_create(connection_start, connection_start_point,
                                                         element, cp, data->next_z_index++, data);
                    data->current_space->elements = g_list_append(data->current_space->elements, (Element*)conn);

                    // Log the action
                    undo_manager_push_action(data->undo_manager, ACTION_CREATE_CONNECTION, conn, "Create Connection");
                }
                connection_start = NULL;
                connection_start_point = -1;
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

            // STORE ORIGINAL POSITIONS FOR ALL SELECTED ELEMENTS
            if (data->undo_original_positions) {
                g_list_free_full(data->undo_original_positions, g_free);
                data->undo_original_positions = NULL;
            }

            if (data->selected_elements) {
                data->undo_original_positions = NULL;
                for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
                    Element *selected_element = (Element*)sel->data;
                    PositionData *pos_data = g_new0(PositionData, 1);
                    pos_data->element = selected_element;
                    pos_data->x = selected_element->x;
                    pos_data->y = selected_element->y;
                    data->undo_original_positions = g_list_append(data->undo_original_positions, pos_data);
                }
            }
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

    for (GList *l = data->current_space->elements; l != NULL; l = l->next) {
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

            element_update_position(element, new_x, new_y);
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
                element_update_position(selected_element, new_x, new_y);
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

        for (GList *iter = data->current_space->elements; iter != NULL; iter = iter->next) {
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

    for (GList *l = data->current_space->elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;

        if (element->resizing) {
            // PUSH RESIZE UNDO ACTION
            if (element->width != data->undo_original_width || element->height != data->undo_original_height) {
                undo_manager_push_resize_action(data->undo_manager, element,
                                              data->undo_original_width, data->undo_original_height,
                                              element->width, element->height);
            }
        }

        if (element->dragging && data->undo_original_positions) {
            // PUSH MOVE UNDO ACTIONS FOR ALL SELECTED ELEMENTS
            for (GList *pos_list = data->undo_original_positions; pos_list != NULL; pos_list = pos_list->next) {
                PositionData *pos_data = (PositionData*)pos_list->data;
                if (pos_data->element->x != pos_data->x || pos_data->element->y != pos_data->y) {
                    undo_manager_push_move_action(data->undo_manager, pos_data->element,
                                                pos_data->x, pos_data->y,
                                                pos_data->element->x, pos_data->element->y);
                }
            }

            // Clean up stored positions
            g_list_free_full(data->undo_original_positions, g_free);
            data->undo_original_positions = NULL;
        }

        element->dragging = FALSE;
        element->resizing = FALSE;
    }

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    canvas_set_cursor(data, data->default_cursor);
}

void canvas_on_add_paper_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    PaperNote *paper_note = paper_note_create(50, 50, 200, 150, "Paper Note", data->next_z_index++, data);
    data->current_space->elements = g_list_append(data->current_space->elements, (Element*)paper_note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_PAPER_NOTE, paper_note, "Create Paper Note");

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    Note *note = note_create(100, 100, 200, 150, "Note", data->next_z_index++, data);
    data->current_space->elements = g_list_append(data->current_space->elements, (Element*)note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_NOTE, note, "Create Note");

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_delete_selected(CanvasData *data) {
    if (!data->selected_elements) return;

    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;

        // Push delete action before actually hiding
        undo_manager_push_delete_action(data->undo_manager, element);

        element->hidden = TRUE;
    }

    canvas_clear_selection(data);
    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_app_shutdown(GApplication *app, gpointer user_data) {
    CanvasData *data = g_object_get_data(G_OBJECT(app), "canvas_data");
    if (data) {
        canvas_data_free(data);
        g_object_set_data(G_OBJECT(app), "canvas_data", NULL);
    }
}

Element* canvas_pick_element(CanvasData *data, int x, int y) {
    Element *selected_element = NULL;
    int highest_z_index = -1;

    for (GList *l = data->current_space->elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;

        // Skip hidden elements
        if (element->hidden) {
            continue;
        }

        if (x >= element->x && x <= element->x + element->width &&
            y >= element->y && y <= element->y + element->height) {
            if (element->z_index > highest_z_index) {
                selected_element = element;
                highest_z_index = element->z_index;
            }
        }
    }
    return selected_element;
}

gboolean canvas_is_element_selected(CanvasData *data, Element *element) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        if (l->data == element) {
            return TRUE;
        }
    }
    return FALSE;
}

void canvas_clear_selection(CanvasData *data) {
    if (data->selected_elements) {
        g_list_free(data->selected_elements);
        data->selected_elements = NULL;
    }
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

    canvas_set_cursor(data, gdk_cursor_new_from_name("default", NULL));
}

void canvas_set_cursor(CanvasData *data, GdkCursor *cursor) {
    if (data->current_cursor != cursor) {
        gtk_widget_set_cursor(data->drawing_area, cursor);
        data->current_cursor = cursor;
    }
}

void switch_to_space(CanvasData *data, Space *space) {
    if (!space || space == data->current_space) return;

    // Add current space to history
    data->space_history = g_list_append(data->space_history, data->current_space);

    // Switch to new space
    data->current_space = space;

    // NO NEED to copy elements - we'll use space->elements directly
    gtk_widget_queue_draw(data->drawing_area);
}

void go_back_to_parent_space(CanvasData *data) {
    if (!data->current_space || !data->current_space->parent) {
        // Already at root or no parent
        return;
    }

    // Switch to parent space
    Space *parent = data->current_space->parent;
    switch_to_space(data, parent);

    // Remove the last history entry
    if (data->space_history) {
        GList *last = g_list_last(data->space_history);
        data->space_history = g_list_delete_link(data->space_history, last);
    }
}

void canvas_on_go_back(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    go_back_to_parent_space(data);
}

void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    if (response_id == GTK_RESPONSE_OK) {
        // Get the entry widget from dialog data
        GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "space_name_entry");
        const char *space_name = gtk_editable_get_text(GTK_EDITABLE(entry));

        if (space_name && strlen(space_name) > 0) {
            // Create a new space
            Space *new_space = space_new(space_name, data->current_space);

            // Create a visual representation of the space
            SpaceElement *space_elem = space_element_create(100, 100, 200, 150,
                                                           new_space, data->next_z_index++, data);

            // Add to current space
            data->current_space->elements = g_list_append(data->current_space->elements, (Element*)space_elem);

            gtk_widget_queue_draw(data->drawing_area);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void canvas_on_add_space(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    GtkRoot *root = gtk_widget_get_root(data->drawing_area);
    GtkWindow *window = GTK_WINDOW(root);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create New Space",
        window,
        GTK_DIALOG_MODAL,
        "Create", GTK_RESPONSE_OK,
        NULL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // Create a grid for better layout
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    // Label
    GtkWidget *label = gtk_label_new("Enter space name:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    // Entry field
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Space name");
    gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 1, 1);

    gtk_box_append(GTK_BOX(content_area), grid);

    // Store the entry widget in dialog data for easy access
    g_object_set_data(G_OBJECT(dialog), "space_name_entry", entry);

    // Set focus on the entry field
    gtk_widget_grab_focus(entry);

    g_signal_connect(dialog, "response", G_CALLBACK(space_creation_dialog_response), data);

    gtk_window_present(GTK_WINDOW(dialog));
}
