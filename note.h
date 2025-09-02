#ifndef NOTE_H
#define NOTE_H

#include <gtk/gtk.h>

typedef struct {
    int x, y, width, height;
    char *text;
    GtkWidget *text_view;
    gboolean editing;
    int z_index;

    gboolean dragging;
    int drag_offset_x;
    int drag_offset_y;

    gboolean resizing;
    int resize_edge;
    int resize_start_x, resize_start_y;
    int orig_x, orig_y, orig_width, orig_height;
} Note;

Note* note_create(int x, int y, int width, int height, const char *text, int z_index);
void note_free(Note *note);
void note_draw(Note *note, cairo_t *cr, gboolean is_selected);
void note_get_connection_point(Note *note, int point, int *cx, int *cy);
int note_pick_resize_handle(Note *note, int x, int y);
int note_pick_connection_point(Note *note, int x, int y);
void note_bring_to_front(Note *note, int *next_z_index);
void note_finish_edit(Note *note);
void note_on_text_view_focus_leave(GtkEventController *controller, gpointer user_data);
gboolean note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

#endif
