#ifndef CANVAS_SPACE_TREE_H
#define CANVAS_SPACE_TREE_H

#include "canvas.h"
#include <gtk/gtk.h>

// Tree view columns
typedef enum {
  SPACE_TREE_COL_NAME,         // Display name (string)
  SPACE_TREE_COL_UUID,         // Space/Element UUID (string)
  SPACE_TREE_COL_TYPE,         // Node type (string: "space", "element", "load_more")
  SPACE_TREE_COL_ELEMENT_TYPE, // Element type for elements (string)
  SPACE_TREE_COL_IS_CURRENT,   // Is current space (boolean)
  SPACE_TREE_COL_IS_LOADED,    // Has children been loaded (boolean)
  SPACE_TREE_COL_SPACE_UUID,   // Parent space UUID (string)
  SPACE_TREE_N_COLUMNS
} SpaceTreeColumns;


// Tree view widget and data
struct _SpaceTreeView {
  GtkWidget *tree_view;
  GtkTreeStore *tree_store;
  GtkTreeSelection *selection;
  CanvasData *canvas_data;
  gboolean suppress_selection_signal;
  gboolean is_rebuilding;
  guint idle_refresh_handle;
};

// Create and initialize the space tree view
SpaceTreeView *space_tree_view_new(CanvasData *canvas_data);

// Free the space tree view
void space_tree_view_free(SpaceTreeView *tree_view);

// Refresh the tree view with current space data
void space_tree_view_refresh(SpaceTreeView *tree_view);
void space_tree_view_schedule_refresh(SpaceTreeView *tree_view);

// Navigate to selected space
void space_tree_view_navigate_to_selected(SpaceTreeView *tree_view);

// Get the tree view widget for UI integration
GtkWidget *space_tree_view_get_widget(SpaceTreeView *tree_view);

// Update current space highlighting
void space_tree_view_update_current_space(SpaceTreeView *tree_view, const char *space_uuid);

// Load children for a tree node (spaces and elements)
void space_tree_view_load_children(SpaceTreeView *tree_view, GtkTreeIter *parent_iter, const char *space_uuid, int depth);

#endif
