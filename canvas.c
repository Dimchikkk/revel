#include "canvas.h"
#include "note.h"
#include "connection.h"
#include <pango/pangocairo.h>

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

static gint compare_notes_by_z_index(gconstpointer a, gconstpointer b) {
    const Note *note_a = (const Note*)a;
    const Note *note_b = (const Note*)b;
    return note_a->z_index - note_b->z_index;
}

CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay) {
    CanvasData *data = g_new0(CanvasData, 1);
    data->notes = NULL;
    data->connections = NULL;
    data->selected_notes = NULL;
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

    return data;
}

void canvas_data_free(CanvasData *data) {
    if (data->default_cursor) g_object_unref(data->default_cursor);
    if (data->move_cursor) g_object_unref(data->move_cursor);
    if (data->resize_cursor) g_object_unref(data->resize_cursor);
    if (data->connect_cursor) g_object_unref(data->connect_cursor);

    for (GList *l = data->notes; l != NULL; l = l->next) {
        note_free((Note*)l->data);
    }
    g_list_free(data->notes);

    for (GList *l = data->connections; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(data->connections);

    g_list_free(data->selected_notes);
    g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    for (GList *l = data->connections; l != NULL; l = l->next) {
        connection_draw((Connection*)l->data, cr);
    }

    GList *sorted_notes = g_list_copy(data->notes);
    sorted_notes = g_list_sort(sorted_notes, compare_notes_by_z_index);

    for (GList *l = sorted_notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        note_draw(note, cr, canvas_is_note_selected(data, note));
    }

    g_list_free(sorted_notes);

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
    static Note *connection_start = NULL;
    static int connection_start_point = -1;

    GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
    if (event) {
        data->modifier_state = gdk_event_get_modifier_state(event);
    }

    Note *note = canvas_pick_note(data, (int)x, (int)y);

    if (note) {
        int rh = note_pick_resize_handle(note, (int)x, (int)y);
        if (rh >= 0) {
            if (!(data->modifier_state & GDK_SHIFT_MASK)) {
                canvas_clear_selection(data);
            }
            if (!canvas_is_note_selected(data, note)) {
                data->selected_notes = g_list_append(data->selected_notes, note);
            }

            note_bring_to_front(note, &data->next_z_index);
            note->resizing = TRUE;
            note->resize_edge = rh;
            note->resize_start_x = (int)x;
            note->resize_start_y = (int)y;
            note->orig_x = note->x;
            note->orig_y = note->y;
            note->orig_width = note->width;
            note->orig_height = note->height;
            return;
        }

        int cp = note_pick_connection_point(note, (int)x, (int)y);
        if (cp >= 0) {
            if (!connection_start) {
                connection_start = note;
                connection_start_point = cp;
            } else {
                if (note != connection_start) {
                    Connection *conn = g_new(Connection, 1);
                    conn->from = connection_start;
                    conn->from_point = connection_start_point;
                    conn->to = note;
                    conn->to_point = cp;
                    data->connections = g_list_append(data->connections, conn);
                }
                connection_start = NULL;
                connection_start_point = -1;
            }
            gtk_widget_queue_draw(data->drawing_area);
            return;
        }

        note_bring_to_front(note, &data->next_z_index);

        if (n_press == 2) {
            note->editing = TRUE;

            if (!note->text_view) {
                note->text_view = gtk_text_view_new();
                gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(note->text_view), GTK_WRAP_WORD);
                gtk_widget_set_size_request(note->text_view, note->width, note->height);

                gtk_overlay_add_overlay(GTK_OVERLAY(data->overlay), note->text_view);
                gtk_widget_set_halign(note->text_view, GTK_ALIGN_START);
                gtk_widget_set_valign(note->text_view, GTK_ALIGN_START);
                gtk_widget_set_margin_start(note->text_view, note->x);
                gtk_widget_set_margin_top(note->text_view, note->y);

                g_object_set_data(G_OBJECT(note->text_view), "canvas_data", data);

                GtkEventController *focus_controller = gtk_event_controller_focus_new();
                g_signal_connect(focus_controller, "leave", G_CALLBACK(note_on_text_view_focus_leave), note);
                gtk_widget_add_controller(note->text_view, focus_controller);

                GtkEventController *key_controller = gtk_event_controller_key_new();
                g_signal_connect(key_controller, "key-pressed", G_CALLBACK(note_on_textview_key_press), note);
                gtk_widget_add_controller(note->text_view, key_controller);
            }

            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
            gtk_text_buffer_set_text(buffer, note->text, -1);

            gtk_widget_show(note->text_view);
            gtk_widget_grab_focus(note->text_view);
            gtk_widget_queue_draw(data->drawing_area);
            return;
        }

        if (!note->editing) {
            if (!(data->modifier_state & GDK_SHIFT_MASK)) {
                canvas_clear_selection(data);
            }
            if (!canvas_is_note_selected(data, note)) {
                data->selected_notes = g_list_append(data->selected_notes, note);
            }
            note->dragging = TRUE;
            note->drag_offset_x = (int)x - note->x;
            note->drag_offset_y = (int)y - note->y;
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

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;

        if (note->resizing) {
            int dx = (int)x - note->resize_start_x;
            int dy = (int)y - note->resize_start_y;
            switch (note->resize_edge) {
            case 0:
                note->x = note->orig_x + dx;
                note->y = note->orig_y + dy;
                note->width = note->orig_width - dx;
                note->height = note->orig_height - dy;
                break;
            case 1:
                note->y = note->orig_y + dy;
                note->width = note->orig_width + dx;
                note->height = note->orig_height - dy;
                break;
            case 2:
                note->width = note->orig_width + dx;
                note->height = note->orig_height + dy;
                break;
            case 3:
                note->x = note->orig_x + dx;
                note->width = note->orig_width - dx;
                note->height = note->orig_height + dy;
                break;
            }
            if (note->width < 50) note->width = 50;
            if (note->height < 30) note->height = 30;

            if (note->text_view) {
                gtk_widget_set_margin_start(note->text_view, note->x);
                gtk_widget_set_margin_top(note->text_view, note->y);
                gtk_widget_set_size_request(note->text_view, note->width, note->height);
            }
            gtk_widget_queue_draw(data->drawing_area);
            return;
        }

        if (note->dragging) {
            int dx = (int)x - note->x - note->drag_offset_x;
            int dy = (int)y - note->y - note->drag_offset_y;

            for (GList *sel = data->selected_notes; sel != NULL; sel = sel->next) {
                Note *selected_note = (Note*)sel->data;
                selected_note->x += dx;
                selected_note->y += dy;

                if (selected_note->text_view) {
                    gtk_widget_set_margin_start(selected_note->text_view, selected_note->x);
                    gtk_widget_set_margin_top(selected_note->text_view, selected_note->y);
                }
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

        for (GList *iter = data->notes; iter != NULL; iter = iter->next) {
            Note *note = (Note*)iter->data;
            if (note->x + note->width >= sel_x &&
                note->x <= sel_x + sel_width &&
                note->y + note->height >= sel_y &&
                note->y <= sel_y + sel_height) {
                if (!canvas_is_note_selected(data, note)) {
                    data->selected_notes = g_list_append(data->selected_notes, note);
                }
            }
        }
    }

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        note->dragging = FALSE;
        note->resizing = FALSE;
    }

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    canvas_set_cursor(data, data->default_cursor);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    Note *note = note_create(50, 50, 200, 150, "Note text", data->next_z_index++);
    data->notes = g_list_append(data->notes, note);
    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_app_shutdown(GApplication *app, gpointer user_data) {
    CanvasData *data = g_object_get_data(G_OBJECT(app), "canvas_data");
    if (data) {
        canvas_data_free(data);
        g_object_set_data(G_OBJECT(app), "canvas_data", NULL);
    }
}

Note* canvas_pick_note(CanvasData *data, int x, int y) {
    Note *selected_note = NULL;
    int highest_z_index = -1;

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        if (x >= note->x && x <= note->x + note->width &&
            y >= note->y && y <= note->y + note->height) {
            if (note->z_index > highest_z_index) {
                selected_note = note;
                highest_z_index = note->z_index;
            }
        }
    }
    return selected_note;
}

gboolean canvas_is_note_selected(CanvasData *data, Note *note) {
    for (GList *l = data->selected_notes; l != NULL; l = l->next) {
        if (l->data == note) {
            return TRUE;
        }
    }
    return FALSE;
}

void canvas_clear_selection(CanvasData *data) {
    if (data->selected_notes) {
        g_list_free(data->selected_notes);
        data->selected_notes = NULL;
    }
}

void canvas_update_cursor(CanvasData *data, int x, int y) {
    Note *note = canvas_pick_note(data, x, y);

    if (note) {
        int rh = note_pick_resize_handle(note, x, y);
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

        int cp = note_pick_connection_point(note, x, y);
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
