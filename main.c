#include <gtk/gtk.h>
#include <pango/pangocairo.h>

typedef struct {
    int x, y, width, height;
    char *text;
    GtkWidget *text_view;   // in-place editor
    gboolean editing;

    // Dragging
    gboolean dragging;
    int drag_offset_x;
    int drag_offset_y;
} Note;

typedef struct {
    Note *from;
    int from_point; // 0=top,1=right,2=bottom,3=left
    Note *to;
    int to_point;
} Connection;

typedef struct {
    GList *notes;
    GList *connections;
    GtkWidget *drawing_area;
    GtkWidget *overlay; // Changed from fixed to overlay
} CanvasData;

/* Utility to get connection point coordinates */
static void get_connection_point(Note *note, int point, int *cx, int *cy) {
    switch(point) {
        case 0: *cx = note->x + note->width/2; *cy = note->y; break;
        case 1: *cx = note->x + note->width; *cy = note->y + note->height/2; break;
        case 2: *cx = note->x + note->width/2; *cy = note->y + note->height; break;
        case 3: *cx = note->x; *cy = note->y + note->height/2; break;
    }
}

/* Draw the note and text */
static void draw_note(cairo_t *cr, Note *note) {
    // Draw background
    cairo_set_source_rgb(cr, 1, 1, 0.8);
    cairo_rectangle(cr, note->x, note->y, note->width, note->height);
    cairo_fill_preserve(cr);

    // Draw border
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.3);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    // Draw connection points
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        get_connection_point(note, i, &cx, &cy);
        cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.8);
        cairo_fill(cr);
    }

    if (!note->editing) {
        // Draw text with Pango
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font_desc = pango_font_description_from_string("Sans 12");
        pango_layout_set_font_description(layout, font_desc);
        pango_font_description_free(font_desc);

        pango_layout_set_text(layout, note->text, -1);
        pango_layout_set_width(layout, (note->width - 10) * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        cairo_move_to(cr, note->x + 5, note->y + 5);
        cairo_set_source_rgb(cr, 0, 0, 0);
        pango_cairo_show_layout(cr, layout);

        g_object_unref(layout);
    }
}

/* Draw canvas */
static void on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    // Clear background
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    // Draw connections
    for (GList *l = data->connections; l != NULL; l = l->next) {
        Connection *conn = (Connection*)l->data;
        int x1, y1, x2, y2;
        get_connection_point(conn->from, conn->from_point, &x1, &y1);
        get_connection_point(conn->to, conn->to_point, &x2, &y2);

        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_set_line_width(cr, 2);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }

    // Draw notes
    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        draw_note(cr, note);
    }
}

/* Finish editing */
static void finish_edit(GtkWidget *widget, gpointer user_data) {
    Note *note = (Note*)user_data;
    if (!note->text_view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
    if (!buffer) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    g_free(note->text);
    note->text = new_text;

    note->editing = FALSE;
    gtk_widget_hide(note->text_view);

    // Get the drawing area from the note's text view data
    CanvasData *data = g_object_get_data(G_OBJECT(note->text_view), "canvas_data");
    if (data && data->drawing_area) {
        gtk_widget_queue_draw(data->drawing_area);
    }
}

/* Handle Enter key in TextView */
static gboolean on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    Note *note = (Note*)user_data;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state & GDK_CONTROL_MASK) return FALSE; // allow Ctrl+Enter
        GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
        finish_edit(widget, note);
        return TRUE;
    }
    return FALSE;
}

/* Pick note at position */
static Note* pick_note(CanvasData *data, int x, int y) {
    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        if (x >= note->x && x <= note->x + note->width &&
            y >= note->y && y <= note->y + note->height) {
            return note;
        }
    }
    return NULL;
}

/* Pick connection point */
static int pick_connection_point(Note *note, int x, int y) {
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        get_connection_point(note, i, &cx, &cy);
        int dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < 36) return i;
    }
    return -1;
}

/* Mouse press: drag, edit, or create connection */
static void on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    static Note *connection_start = NULL;
    static int connection_start_point = -1;

    Note *note = pick_note(data, (int)x, (int)y);

    if (note) {
        int cp = pick_connection_point(note, (int)x, (int)y);

        // Handle connection point click
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

        // Double-click: start editing
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

                // Store canvas data reference
                g_object_set_data(G_OBJECT(note->text_view), "canvas_data", data);

                // Use event controller for focus handling (GTK4)
                GtkEventController *focus_controller = gtk_event_controller_focus_new();
                g_signal_connect(focus_controller, "leave", G_CALLBACK(finish_edit), note);
                gtk_widget_add_controller(note->text_view, focus_controller);

                GtkEventController *key_controller = gtk_event_controller_key_new();
                g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_textview_key_press), note);
                gtk_widget_add_controller(note->text_view, key_controller);
            }

            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
            gtk_text_buffer_set_text(buffer, note->text, -1);

            gtk_widget_show(note->text_view);
            gtk_widget_grab_focus(note->text_view);
            gtk_widget_queue_draw(data->drawing_area);

            return;
        }

        // Single click: start dragging
        if (!note->editing) {
            note->dragging = TRUE;
            note->drag_offset_x = (int)x - note->x;
            note->drag_offset_y = (int)y - note->y;
        }
    } else {
        connection_start = NULL;
        connection_start_point = -1;
    }
}

/* Mouse motion: drag note */
static void on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        if (note->dragging) {
            note->x = (int)x - note->drag_offset_x;
            note->y = (int)y - note->drag_offset_y;

            // Move text_view if exists (editing or not)
            if (note->text_view) {
                gtk_widget_set_margin_start(note->text_view, note->x);
                gtk_widget_set_margin_top(note->text_view, note->y);
            }

            gtk_widget_queue_draw(data->drawing_area);
            break;
        }
    }
}

/* Mouse release */
static void on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        note->dragging = FALSE;
    }
}

/* Add a new note */
static void on_add_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    Note *note = g_new0(Note, 1);
    note->x = 50;
    note->y = 50;
    note->width = 200;
    note->height = 150;
    note->text = g_strdup("Double-click to edit this note.\nDrag to move.");
    note->text_view = NULL;
    note->editing = FALSE;
    note->dragging = FALSE;

    data->notes = g_list_append(data->notes, note);
    gtk_widget_queue_draw(data->drawing_area);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Note Canvas with Connections");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(vbox), toolbar);

    GtkWidget *add_btn = gtk_button_new_with_label("New Note");
    gtk_box_append(GTK_BOX(toolbar), add_btn);

    // Create overlay container for drawing area and text views
    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(overlay, TRUE);
    gtk_widget_set_vexpand(overlay, TRUE);
    gtk_box_append(GTK_BOX(vbox), overlay);

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing_area, TRUE);
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);

    // Create and initialize canvas data
    CanvasData *data = g_new0(CanvasData, 1);
    data->notes = NULL;
    data->connections = NULL;
    data->drawing_area = drawing_area;
    data->overlay = overlay; // Store overlay instead of fixed

    // Set draw function with data
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), on_draw, data, NULL);

    // Set up event controllers with data
    GtkGesture *click_controller = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_controller), GDK_BUTTON_PRIMARY);
    g_signal_connect(click_controller, "pressed", G_CALLBACK(on_button_press), data);
    g_signal_connect(click_controller, "released", G_CALLBACK(on_release), data);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(click_controller));

    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_motion), data);
    gtk_widget_add_controller(drawing_area, motion_controller);

    // Connect add button with data
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_note), data);

    // Store data in window for cleanup
    g_object_set_data(G_OBJECT(window), "canvas_data", data);

    gtk_widget_show(window);
}

static void on_window_destroy(GtkWidget *window, gpointer user_data) {
    CanvasData *data = g_object_get_data(G_OBJECT(window), "canvas_data");
    if (data) {
        // Cleanup notes
        for (GList *l = data->notes; l != NULL; l = l->next) {
            Note *note = (Note*)l->data;
            g_free(note->text);
            g_free(note);
        }
        g_list_free(data->notes);

        // Cleanup connections
        for (GList *l = data->connections; l != NULL; l = l->next) {
            g_free(l->data);
        }
        g_list_free(data->connections);

        g_free(data);
    }
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.notecanvas", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_window_destroy), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
