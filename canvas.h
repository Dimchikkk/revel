#ifndef CANVAS_H
#define CANVAS_H

#include <gtk/gtk.h>
#include "note.h"
#include "connection.h"

typedef struct _CanvasData CanvasData;

struct _UndoManager;
typedef struct _UndoManager UndoManager;

struct _CanvasData {
    GList *elements;
    GList *connections;
    GList *selected_elements;
    GtkWidget *drawing_area;
    GtkWidget *overlay;
    int next_z_index;

    gboolean selecting;
    int start_x, start_y;
    int current_x, current_y;
    GdkModifierType modifier_state;

    GdkCursor *default_cursor;
    GdkCursor *move_cursor;
    GdkCursor *resize_cursor;
    GdkCursor *connect_cursor;
    GdkCursor *current_cursor;

    UndoManager *undo_manager;
};


CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay);
void canvas_data_free(CanvasData *data);
void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);
void canvas_on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void canvas_on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data);
void canvas_on_add_note(GtkButton *button, gpointer user_data);
void canvas_on_app_shutdown(GApplication *app, gpointer user_data);
void canvas_clear_selection(CanvasData *data);
void canvas_update_cursor(CanvasData *data, int x, int y);
void canvas_set_cursor(CanvasData *data, GdkCursor *cursor);
Element* canvas_pick_element(CanvasData *data, int x, int y);
gboolean canvas_is_element_selected(CanvasData *data, Element *element);
void canvas_clear_selection(CanvasData *data);
void canvas_on_add_paper_note(GtkButton *button, gpointer user_data);
void canvas_on_add_note(GtkButton *button, gpointer user_data);

#endif
