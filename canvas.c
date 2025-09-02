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
    data->elements = NULL;
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

    return data;
}

void canvas_data_free(CanvasData *data) {
    if (data->default_cursor) g_object_unref(data->default_cursor);
    if (data->move_cursor) g_object_unref(data->move_cursor);
    if (data->resize_cursor) g_object_unref(data->resize_cursor);
    if (data->connect_cursor) g_object_unref(data->connect_cursor);

    for (GList *l = data->elements; l != NULL; l = l->next) {
        element_free((Element*)l->data);
    }
    g_list_free(data->elements);

    g_list_free(data->selected_elements);
    if (data->undo_manager) undo_manager_free(data->undo_manager);
    g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    GList *sorted_elements = g_list_copy(data->elements);
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
                    Connection *conn = connection_create(connection_start, connection_start_point,
                                                         element, cp, data->next_z_index++);
                    data->elements = g_list_append(data->elements, (Element*)conn);

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

    for (GList *l = data->elements; l != NULL; l = l->next) {
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

        for (GList *iter = data->elements; iter != NULL; iter = iter->next) {
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

    for (GList *l = data->elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
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
    data->elements = g_list_append(data->elements, (Element*)paper_note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_PAPER_NOTE, paper_note, "Create Paper Note");

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    Note *note = note_create(100, 100, 200, 150, "Note", data->next_z_index++, data);
    data->elements = g_list_append(data->elements, (Element*)note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_NOTE, note, "Create Note");

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

    for (GList *l = data->elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
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
    }

    canvas_set_cursor(data, gdk_cursor_new_from_name("default", NULL));
}

void canvas_set_cursor(CanvasData *data, GdkCursor *cursor) {
    if (data->current_cursor != cursor) {
        gtk_widget_set_cursor(data->drawing_area, cursor);
        data->current_cursor = cursor;
    }
}
