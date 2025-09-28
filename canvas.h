#ifndef CANVAS_H
#define CANVAS_H

#include <gtk/gtk.h>
#include "note.h"
#include "space.h"
#include "model.h"
#include "connection.h"
#include "freehand_drawing.h"
#include "shape.h"

typedef struct _CanvasData CanvasData;
typedef struct _UndoManager UndoManager;

typedef struct {
  ModelElement *element;
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

  gboolean panning;
  int pan_start_x;
  int pan_start_y;
  double offset_x;
  double offset_y;
  double zoom_scale;

  double last_mouse_x;
  double last_mouse_y;

  UndoManager *undo_manager;
  GHashTable *drag_start_positions;
  GHashTable *drag_start_sizes;

  gboolean drawing_mode;
  FreehandDrawing *current_drawing;
  ElementColor drawing_color;
  int drawing_stroke_width;
  GdkCursor *draw_cursor;
  GdkCursor *line_cursor;

  gboolean shape_mode;
  int selected_shape_type;
  gboolean shape_filled;
  Shape *current_shape;
  int shape_start_x;
  int shape_start_y;

  Element *connection_start;
  int connection_start_point;

  GtkWidget *zoom_entry;

  // Grid settings
  gboolean show_grid;
  GdkRGBA grid_color;

  Model *model;
};

#endif
