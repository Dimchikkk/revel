#ifndef CANVAS_H
#define CANVAS_H

#include <gtk/gtk.h>
#include "note.h"
#include "space.h"
#include "connection.h"

typedef struct _CanvasData CanvasData;

struct _UndoManager;
typedef struct _UndoManager UndoManager;

typedef struct {
    Element *element;
    double x;
    double y;
} PositionData;

struct _CanvasData {
    GList *selected_elements;
    GtkWidget *drawing_area;
    GtkWidget *overlay;
    int next_z_index;
    gboolean selecting;
    int start_x, start_y;
    int current_x, current_y;
    guint modifier_state;

    GdkCursor *default_cursor;
    GdkCursor *move_cursor;
    GdkCursor *resize_cursor;
    GdkCursor *connect_cursor;
    GdkCursor *current_cursor;

    UndoManager *undo_manager;

    double undo_original_width;
    double undo_original_height;
    double undo_original_x;
    double undo_original_y;
    GList *undo_original_positions;

    Space *current_space;
    GList *space_history;
};

#endif
