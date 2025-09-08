#ifndef CANVAS_H
#define CANVAS_H

#include <gtk/gtk.h>
#include "note.h"
#include "space.h"
#include "model.h"
#include "connection.h"

typedef struct _CanvasData CanvasData;

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

    Model *model;
};

#endif
