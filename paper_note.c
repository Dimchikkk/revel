#include "paper_note.h"
#include "canvas.h"
#include <pango/pangocairo.h>
#include <math.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static ElementVTable paper_note_vtable = {
    .draw = paper_note_draw,
    .get_connection_point = paper_note_get_connection_point,
    .pick_resize_handle = paper_note_pick_resize_handle,
    .pick_connection_point = paper_note_pick_connection_point,
    .start_editing = paper_note_start_editing,
    .finish_editing = paper_note_finish_editing,
    .update_position = paper_note_update_position,
    .update_size = paper_note_update_size,
    .free = paper_note_free
};

PaperNote* paper_note_create(int x, int y, int width, int height, const char *text, int z_index, CanvasData *data) {
    PaperNote *note = g_new0(PaperNote, 1);
    note->base.type = ELEMENT_PAPER_NOTE;
    note->base.vtable = &paper_note_vtable;
    note->base.x = x;
    note->base.y = y;
    note->base.width = width;
    note->base.height = height;
    note->base.z_index = z_index;
    note->text = g_strdup(text);
    note->text_view = NULL;
    note->editing = FALSE;
    note->canvas_data = data;
    return note;
}

void paper_note_on_text_view_focus_leave(GtkEventController *controller, gpointer user_data) {
    PaperNote *note = (PaperNote*)user_data;
    paper_note_finish_editing((Element*)note);
}

gboolean paper_note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    PaperNote *note = (PaperNote*)user_data;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state & GDK_CONTROL_MASK) return FALSE;
        paper_note_finish_editing((Element*)note);
        return TRUE;
    }
    return FALSE;
}

void paper_note_draw(Element *element, cairo_t *cr, gboolean is_selected) {
    PaperNote *note = (PaperNote*)element;

    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_clip(cr);

    if (is_selected) {
        cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
    } else {
        cairo_set_source_rgb(cr, 1, 1, 0.8);
    }
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.3);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    for (int i = 0; i < 4; i++) {
        int cx, cy;
        paper_note_get_connection_point(element, i, &cx, &cy);
        cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
        cairo_fill(cr);
    }

    if (!note->editing) {
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font_desc = pango_font_description_from_string("Sans 12");
        pango_layout_set_font_description(layout, font_desc);
        pango_font_description_free(font_desc);

        pango_layout_set_text(layout, note->text, -1);
        pango_layout_set_width(layout, (element->width - 10) * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        int text_width, text_height;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);

        if (text_height <= element->height - 10) {
            cairo_move_to(cr, element->x + 5, element->y + 5);
            cairo_set_source_rgb(cr, 0, 0, 0);
            pango_cairo_show_layout(cr, layout);
        } else {
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            pango_layout_set_height(layout, (element->height - 10) * PANGO_SCALE);
            cairo_move_to(cr, element->x + 5, element->y + 5);
            cairo_set_source_rgb(cr, 0, 0, 0);
            pango_cairo_show_layout(cr, layout);
        }

        g_object_unref(layout);
    }

    cairo_reset_clip(cr);
}

void paper_note_get_connection_point(Element *element, int point, int *cx, int *cy) {
    switch(point) {
    case 0: *cx = element->x + element->width/2; *cy = element->y; break;
    case 1: *cx = element->x + element->width; *cy = element->y + element->height/2; break;
    case 2: *cx = element->x + element->width/2; *cy = element->y + element->height; break;
    case 3: *cx = element->x; *cy = element->y + element->height/2; break;
    }
}

int paper_note_pick_resize_handle(Element *element, int x, int y) {
    int size = 8;
    struct { int px, py; } handles[4] = {
        {element->x, element->y},
        {element->x + element->width, element->y},
        {element->x + element->width, element->y + element->height},
        {element->x, element->y + element->height}
    };

    for (int i = 0; i < 4; i++) {
        if (abs(x - handles[i].px) <= size && abs(y - handles[i].py) <= size) {
            return i;
        }
    }
    return -1;
}

int paper_note_pick_connection_point(Element *element, int x, int y) {
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        paper_note_get_connection_point(element, i, &cx, &cy);
        int dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < 36) return i;
    }
    return -1;
}

void paper_note_start_editing(Element *element, GtkWidget *overlay) {
    PaperNote *note = (PaperNote*)element;
    note->editing = TRUE;

    if (!note->text_view) {
        note->text_view = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(note->text_view), GTK_WRAP_WORD);
        gtk_widget_set_size_request(note->text_view, element->width, element->height);

        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), note->text_view);
        gtk_widget_set_halign(note->text_view, GTK_ALIGN_START);
        gtk_widget_set_valign(note->text_view, GTK_ALIGN_START);
        gtk_widget_set_margin_start(note->text_view, element->x);
        gtk_widget_set_margin_top(note->text_view, element->y);

        GtkEventController *focus_controller = gtk_event_controller_focus_new();
        g_signal_connect(focus_controller, "leave", G_CALLBACK(paper_note_on_text_view_focus_leave), note);
        gtk_widget_add_controller(note->text_view, focus_controller);

        GtkEventController *key_controller = gtk_event_controller_key_new();
        g_signal_connect(key_controller, "key-pressed", G_CALLBACK(paper_note_on_textview_key_press), note);
        gtk_widget_add_controller(note->text_view, key_controller);
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
    gtk_text_buffer_set_text(buffer, note->text, -1);

    gtk_widget_show(note->text_view);
    gtk_widget_grab_focus(note->text_view);
}

void paper_note_finish_editing(Element *element) {
    PaperNote *note = (PaperNote*)element;
    if (!note->text_view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_free(note->text);
    note->text = new_text;

    note->editing = FALSE;
    gtk_widget_hide(note->text_view);

    // Queue redraw using the stored canvas data
    if (note->canvas_data && note->canvas_data->drawing_area) {
        gtk_widget_queue_draw(note->canvas_data->drawing_area);
    }
}

void paper_note_update_position(Element *element, int x, int y) {
    PaperNote *note = (PaperNote*)element;
    if (note->text_view) {
        gtk_widget_set_margin_start(note->text_view, x);
        gtk_widget_set_margin_top(note->text_view, y);
    }
}

void paper_note_update_size(Element *element, int width, int height) {
    PaperNote *note = (PaperNote*)element;
    if (note->text_view) {
        gtk_widget_set_size_request(note->text_view, width, height);
    }
}

void paper_note_free(Element *element) {
    PaperNote *note = (PaperNote*)element;
    if (note->text) g_free(note->text);
    if (note->text_view && GTK_IS_WIDGET(note->text_view) && gtk_widget_get_parent(note->text_view)) {
        gtk_widget_unparent(note->text_view);
    }
    g_free(note);
}
