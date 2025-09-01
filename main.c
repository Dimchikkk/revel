#include <gtk/gtk.h>
#include <pango/pangocairo.h>

typedef struct {
    int x, y, width, height;
    char *text;
    GtkWidget *text_view;   // in-place editor
    gboolean editing;
    int z_index; // New: z-index for rendering order

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
    GList *selected_notes;  // Multi-selection support
    GtkWidget *drawing_area;
    GtkWidget *overlay;
    int next_z_index; // Counter for assigning z-index

    // Selection rectangle
    gboolean selecting;
    int start_x, start_y;
    int current_x, current_y;
    GdkModifierType modifier_state; // Store modifier state
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
static void draw_note(cairo_t *cr, Note *note, gboolean is_selected) {
    // Draw background
    if (is_selected) {
        cairo_set_source_rgb(cr, 0.8, 0.8, 1.0); // Light blue for selected notes
    } else {
        cairo_set_source_rgb(cr, 1, 1, 0.8); // Light yellow for unselected notes
    }
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

/* Check if note is selected */
static gboolean is_note_selected(CanvasData *data, Note *note) {
    for (GList *l = data->selected_notes; l != NULL; l = l->next) {
        if (l->data == note) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Compare function for sorting notes by z-index */
static gint compare_notes_by_z_index(gconstpointer a, gconstpointer b) {
    const Note *note_a = (const Note*)a;
    const Note *note_b = (const Note*)b;
    return note_a->z_index - note_b->z_index;
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

    // Sort notes by z-index (lowest first) for rendering
    GList *sorted_notes = g_list_copy(data->notes);
    sorted_notes = g_list_sort(sorted_notes, compare_notes_by_z_index);

    // Draw notes in z-index order
    for (GList *l = sorted_notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        draw_note(cr, note, is_note_selected(data, note));
    }

    g_list_free(sorted_notes);

    // Draw selection rectangle if selecting
    if (data->selecting) {
        cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.3); // Semi-transparent blue
        cairo_rectangle(cr,
                       MIN(data->start_x, data->current_x),
                       MIN(data->start_y, data->current_y),
                       ABS(data->current_x - data->start_x),
                       ABS(data->current_y - data->start_y));
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.2, 0.2, 1.0); // Blue border
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
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

/* Pick note at position (considering z-index - highest z-index on top wins) */
static Note* pick_note(CanvasData *data, int x, int y) {
    Note *selected_note = NULL;
    int highest_z_index = -1;

    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        if (x >= note->x && x <= note->x + note->width &&
            y >= note->y && y <= note->y + note->height) {
            // Select the note with the highest z-index at this position
            if (note->z_index > highest_z_index) {
                selected_note = note;
                highest_z_index = note->z_index;
            }
        }
    }
    return selected_note;
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

/* Bring note to front (highest z-index) */
static void bring_note_to_front(CanvasData *data, Note *note) {
    note->z_index = data->next_z_index++;
    gtk_widget_queue_draw(data->drawing_area);
}

/* Clear selection */
static void clear_selection(CanvasData *data) {
    if (data->selected_notes) {
        g_list_free(data->selected_notes);
        data->selected_notes = NULL;
    }
}

/* Mouse press: drag, edit, or create connection */
static void on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    static Note *connection_start = NULL;
    static int connection_start_point = -1;

    // Get the event to check modifier state
    GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
    if (event) {
        data->modifier_state = gdk_event_get_modifier_state(event);
    }

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

        // Bring note to front when clicked
        bring_note_to_front(data, note);

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

        // Single click: handle selection and start dragging
        if (!note->editing) {
            // Clear selection if not holding shift
            if (!(data->modifier_state & GDK_SHIFT_MASK)) {
                clear_selection(data);
            }

            // Add note to selection if not already selected
            if (!is_note_selected(data, note)) {
                data->selected_notes = g_list_append(data->selected_notes, note);
            }

            // Start dragging
            note->dragging = TRUE;
            note->drag_offset_x = (int)x - note->x;
            note->drag_offset_y = (int)y - note->y;
        }
    } else {
        // Clicked on empty space - start selection rectangle
        connection_start = NULL;
        connection_start_point = -1;

        // Clear selection if not holding shift
        if (!(data->modifier_state & GDK_SHIFT_MASK)) {
            clear_selection(data);
        }

        data->selecting = TRUE;
        data->start_x = (int)x;
        data->start_y = (int)y;
        data->current_x = (int)x;
        data->current_y = (int)y;
    }

    gtk_widget_queue_draw(data->drawing_area);
}

/* Mouse motion: drag note or update selection rectangle */
static void on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    // Handle dragging of selected notes
    gboolean any_note_dragging = FALSE;
    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        if (note->dragging) {
            any_note_dragging = TRUE;
            int dx = (int)x - note->x - note->drag_offset_x;
            int dy = (int)y - note->y - note->drag_offset_y;

            // Move all selected notes
            for (GList *sel = data->selected_notes; sel != NULL; sel = sel->next) {
                Note *selected_note = (Note*)sel->data;
                selected_note->x += dx;
                selected_note->y += dy;

                // Move text_view if exists
                if (selected_note->text_view) {
                    gtk_widget_set_margin_start(selected_note->text_view, selected_note->x);
                    gtk_widget_set_margin_top(selected_note->text_view, selected_note->y);
                }
            }

            gtk_widget_queue_draw(data->drawing_area);
            break;
        }
    }

    // Handle selection rectangle
    if (data->selecting && !any_note_dragging) {
        data->current_x = (int)x;
        data->current_y = (int)y;
        gtk_widget_queue_draw(data->drawing_area);
    }
}

/* Mouse release */
static void on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    // Finalize selection rectangle
    if (data->selecting) {
        data->selecting = FALSE;

        // Calculate selection rectangle
        int sel_x = MIN(data->start_x, data->current_x);
        int sel_y = MIN(data->start_y, data->current_y);
        int sel_width = ABS(data->current_x - data->start_x);
        int sel_height = ABS(data->current_y - data->start_y);

        // Select notes that intersect with the selection rectangle
        for (GList *iter = data->notes; iter != NULL; iter = iter->next) {
            Note *note = (Note*)iter->data;

            // Check if note intersects with selection rectangle
            if (note->x + note->width >= sel_x &&
                note->x <= sel_x + sel_width &&
                note->y + note->height >= sel_y &&
                note->y <= sel_y + sel_height) {

                // Add to selection if not already selected
                if (!is_note_selected(data, note)) {
                    data->selected_notes = g_list_append(data->selected_notes, note);
                }
            }
        }
    }

    // Stop dragging all notes
    for (GList *l = data->notes; l != NULL; l = l->next) {
        Note *note = (Note*)l->data;
        note->dragging = FALSE;
    }

    gtk_widget_queue_draw(data->drawing_area);
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
    note->z_index = data->next_z_index++; // Assign and increment z-index

    data->notes = g_list_append(data->notes, note);
    gtk_widget_queue_draw(data->drawing_area);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Note Canvas with Connections and Multi-Select");

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
    data->selected_notes = NULL;
    data->drawing_area = drawing_area;
    data->overlay = overlay;
    data->next_z_index = 1; // Start z-index from 1
    data->selecting = FALSE;
    data->modifier_state = 0;

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

        // Cleanup selected notes list
        g_list_free(data->selected_notes);

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
