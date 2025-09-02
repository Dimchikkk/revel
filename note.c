#include "note.h"
#include "canvas.h"
#include <pango/pangocairo.h>
#include <math.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

Note* note_create(int x, int y, int width, int height, const char *text, int z_index) {
    Note *note = g_new0(Note, 1);
    note->x = x;
    note->y = y;
    note->width = width;
    note->height = height;
    note->text = g_strdup(text);
    note->text_view = NULL;
    note->editing = FALSE;
    note->z_index = z_index;
    note->dragging = FALSE;
    note->resizing = FALSE;
    return note;
}

void note_free(Note *note) {
    if (note->text) g_free(note->text);
    if (note->text_view && GTK_IS_WIDGET(note->text_view) && gtk_widget_get_parent(note->text_view)) {
        gtk_widget_unparent(note->text_view);
    }
    g_free(note);
}

void note_draw(Note *note, cairo_t *cr, gboolean is_selected) {
    cairo_rectangle(cr, note->x, note->y, note->width, note->height);
    cairo_clip(cr);

    if (is_selected) {
        cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
    } else {
        cairo_set_source_rgb(cr, 1, 1, 0.8);
    }
    cairo_rectangle(cr, note->x, note->y, note->width, note->height);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.5, 0.5, 0.3);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    for (int i = 0; i < 4; i++) {
        int cx, cy;
        note_get_connection_point(note, i, &cx, &cy);
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
        pango_layout_set_width(layout, (note->width - 10) * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        int text_width, text_height;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);

        if (text_height <= note->height - 10) {
            cairo_move_to(cr, note->x + 5, note->y + 5);
            cairo_set_source_rgb(cr, 0, 0, 0);
            pango_cairo_show_layout(cr, layout);
        } else {
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            pango_layout_set_height(layout, (note->height - 10) * PANGO_SCALE);
            cairo_move_to(cr, note->x + 5, note->y + 5);
            cairo_set_source_rgb(cr, 0, 0, 0);
            pango_cairo_show_layout(cr, layout);
        }

        g_object_unref(layout);
    }

    cairo_reset_clip(cr);
}

void note_get_connection_point(Note *note, int point, int *cx, int *cy) {
    switch(point) {
    case 0: *cx = note->x + note->width/2; *cy = note->y; break;
    case 1: *cx = note->x + note->width; *cy = note->y + note->height/2; break;
    case 2: *cx = note->x + note->width/2; *cy = note->y + note->height; break;
    case 3: *cx = note->x; *cy = note->y + note->height/2; break;
    }
}

int note_pick_resize_handle(Note *note, int x, int y) {
    int size = 8;
    struct { int px, py; } handles[4] = {
        {note->x, note->y},
        {note->x + note->width, note->y},
        {note->x + note->width, note->y + note->height},
        {note->x, note->y + note->height}
    };

    for (int i = 0; i < 4; i++) {
        if (abs(x - handles[i].px) <= size && abs(y - handles[i].py) <= size) {
            return i;
        }
    }
    return -1;
}

int note_pick_connection_point(Note *note, int x, int y) {
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        note_get_connection_point(note, i, &cx, &cy);
        int dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < 36) return i;
    }
    return -1;
}

void note_bring_to_front(Note *note, int *next_z_index) {
    note->z_index = (*next_z_index)++;
}

void note_finish_edit(Note *note) {
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
}

void note_on_text_view_focus_leave(GtkEventController *controller, gpointer user_data) {
    Note *note = (Note*)user_data;
    note_finish_edit(note);
}

gboolean note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    Note *note = (Note*)user_data;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state & GDK_CONTROL_MASK) return FALSE;
        note_finish_edit(note);
        return TRUE;
    }
    return FALSE;
}
