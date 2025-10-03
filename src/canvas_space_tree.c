#include "canvas_space_tree.h"
#include "canvas_spaces.h"
#include "model.h"
#include "element.h"
#include <string.h>
#include <gtk/gtk.h>

static gboolean is_space_on_active_path(SpaceTreeView *tree_view, const char *space_uuid) {
  if (!tree_view || !space_uuid || !tree_view->canvas_data || !tree_view->canvas_data->model) {
    return FALSE;
  }

  const char *current_uuid = tree_view->canvas_data->model->current_space_uuid;
  if (!current_uuid) {
    return FALSE;
  }

  char *iter_uuid = g_strdup(current_uuid);
  gboolean result = FALSE;

  // Track visited spaces to detect cycles
  GHashTable *visited = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  while (iter_uuid) {
    if (g_strcmp0(space_uuid, iter_uuid) == 0) {
      result = TRUE;
      break;
    }

    // Cycle detection: if we've seen this UUID before, break
    if (g_hash_table_contains(visited, iter_uuid)) {
      printf("WARNING: Cycle detected in space hierarchy at space %s\n", iter_uuid);
      break;
    }

    g_hash_table_add(visited, g_strdup(iter_uuid));

    char *next_uuid = NULL;
    if (!model_get_space_parent_uuid(tree_view->canvas_data->model, iter_uuid, &next_uuid)) {
      break;
    }

    g_free(iter_uuid);
    iter_uuid = next_uuid;
  }

  g_free(iter_uuid);
  g_hash_table_destroy(visited);
  return result;
}

static void load_space_elements(SpaceTreeView *tree_view, GtkTreeIter *parent_iter,
                               const char *space_uuid) {
  if (!tree_view->canvas_data->model) {
    return;
  }

  // Only load elements for spaces that are on the active path (current space or its ancestors)
  if (!is_space_on_active_path(tree_view, space_uuid)) {
    return;
  }

  // Get all elements in the current space
  GHashTableIter iter;
  gpointer key, value;
  GList *elements_in_space = NULL;

  g_hash_table_iter_init(&iter, tree_view->canvas_data->model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement*)value;

    if (element->space_uuid && g_strcmp0(element->space_uuid, space_uuid) == 0) {
      elements_in_space = g_list_append(elements_in_space, element);
    }
  }

  // Sort elements by type then by name/UUID for consistent ordering
  elements_in_space = g_list_sort(elements_in_space, (GCompareFunc)model_compare_for_saving_loading);

  // Add all elements (no pagination)
  for (GList *l = elements_in_space; l != NULL; l = l->next) {
    ModelElement *element = (ModelElement*)l->data;

    // Skip space elements (they're shown in the hierarchy)
    if (element->type && element->type->type == ELEMENT_SPACE) {
      continue;
    }

    GtkTreeIter child_iter;
    gtk_tree_store_append(tree_view->tree_store, &child_iter, parent_iter);

    // Get element name or create one
    const char *element_name = "Unnamed";
    if (element->text && element->text->text) {
      element_name = element->text->text;
    }

    // Truncate long names
    char display_name[100];
    if (strlen(element_name) > 80) {
      strncpy(display_name, element_name, 80);
      display_name[80] = '\0';
      strcat(display_name, "...");
    } else {
      strcpy(display_name, element_name);
    }

    const char *type_name = "Unknown";
    if (element->type) {
      type_name = element_get_type_name(element->type->type);
    }

    char formatted_name[150];
    snprintf(formatted_name, sizeof(formatted_name), "%s (%s)", display_name, type_name);

    gtk_tree_store_set(tree_view->tree_store, &child_iter,
                      SPACE_TREE_COL_NAME, formatted_name,
                      SPACE_TREE_COL_UUID, element->uuid,
                      SPACE_TREE_COL_TYPE, "element",
                      SPACE_TREE_COL_ELEMENT_TYPE, type_name,
                      SPACE_TREE_COL_IS_CURRENT, FALSE,
                      SPACE_TREE_COL_IS_LOADED, TRUE,
                      SPACE_TREE_COL_SPACE_UUID, space_uuid,
                      -1);
  }

  g_list_free(elements_in_space);
}

static gboolean find_current_iter_recursive(SpaceTreeView *tree_view, GtkTreeIter *parent_iter, GtkTreeIter *found_iter) {
  GtkTreeModel *model = GTK_TREE_MODEL(tree_view->tree_store);
  GtkTreeIter iter;
  gboolean valid = parent_iter ? gtk_tree_model_iter_children(model, &iter, parent_iter)
                               : gtk_tree_model_get_iter_first(model, &iter);

  while (valid) {
    gboolean is_current = FALSE;
    gtk_tree_model_get(model, &iter, SPACE_TREE_COL_IS_CURRENT, &is_current, -1);
    if (is_current) {
      *found_iter = iter;
      return TRUE;
    }

    if (find_current_iter_recursive(tree_view, &iter, found_iter)) {
      return TRUE;
    }

    valid = gtk_tree_model_iter_next(model, &iter);
  }

  return FALSE;
}

static gboolean select_current_space_node(SpaceTreeView *tree_view) {
  if (!tree_view || !tree_view->tree_store || !tree_view->selection) {
    return FALSE;
  }

  GtkTreeIter current_iter;
  if (!find_current_iter_recursive(tree_view, NULL, &current_iter)) {
    return FALSE;
  }

  GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_view->tree_store), &current_iter);
  if (!path) {
    return FALSE;
  }

  gboolean previous_flag = tree_view->suppress_selection_signal;
  tree_view->suppress_selection_signal = TRUE;

  gtk_tree_selection_unselect_all(tree_view->selection);
  gtk_tree_selection_select_iter(tree_view->selection, &current_iter);
  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view->tree_view), path);
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view->tree_view), path, NULL, FALSE, 0.0, 0.0);

  tree_view->suppress_selection_signal = previous_flag;
  gtk_tree_path_free(path);
  return TRUE;
}

static void build_initial_tree(SpaceTreeView *tree_view);

static gboolean refresh_tree_view_idle(gpointer user_data) {
  SpaceTreeView *tree_view = (SpaceTreeView*)user_data;
  if (!tree_view) {
    return G_SOURCE_REMOVE;
  }

  if (tree_view->is_rebuilding) {
    // Try again on next iteration until rebuild finishes
    return G_SOURCE_CONTINUE;
  }

  tree_view->suppress_selection_signal = TRUE;
  build_initial_tree(tree_view);
  tree_view->suppress_selection_signal = FALSE;
  select_current_space_node(tree_view);

  tree_view->is_built = TRUE;
  tree_view->idle_refresh_handle = 0;

  return G_SOURCE_REMOVE;
}

void space_tree_view_schedule_refresh(SpaceTreeView *tree_view) {
  if (!tree_view) return;
  if (!tree_view->idle_refresh_handle) {
    tree_view->idle_refresh_handle = g_idle_add(refresh_tree_view_idle, tree_view);
  }
}

// Build path from root to a specific space
static GList* build_path_to_space(SpaceTreeView *tree_view, const char *target_space_uuid) {
  if (!target_space_uuid) return NULL;

  GList *path = NULL;
  char *current_uuid = g_strdup(target_space_uuid);

  // Track visited spaces to detect cycles
  GHashTable *visited = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  // Build path from target back to root
  while (current_uuid) {
    // Cycle detection: if we've seen this UUID before, break
    if (g_hash_table_contains(visited, current_uuid)) {
      printf("WARNING: Cycle detected while building path to space %s\n", target_space_uuid);
      g_free(current_uuid);
      break;
    }

    path = g_list_prepend(path, g_strdup(current_uuid));
    g_hash_table_add(visited, g_strdup(current_uuid));

    char *parent_uuid = NULL;
    if (model_get_space_parent_uuid(tree_view->canvas_data->model, current_uuid, &parent_uuid) != 0) {
      g_free(current_uuid);
      current_uuid = parent_uuid;
    } else {
      g_free(current_uuid);
      current_uuid = NULL;
    }
  }

  g_hash_table_destroy(visited);
  return path;
}

// Simple expansion function - just expand all loaded nodes
static void expand_to_current_space(SpaceTreeView *tree_view, const char *current_space_uuid) {
  if (!current_space_uuid || !tree_view) return;

  // Simply expand all rows that have IS_LOADED = TRUE
  gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_view->tree_view));
}

// Load children for a space with lazy loading - only expand if on path to current space
static void space_tree_view_load_children_lazy_internal(SpaceTreeView *tree_view, GtkTreeIter *parent_iter,
                                                       const char *space_uuid, GList *expansion_path, GHashTable *processed_spaces) {
  // Load child spaces first
  GList *all_spaces = NULL;
  if (model_get_all_spaces(tree_view->canvas_data->model, &all_spaces) != 0) {
    for (GList *l = all_spaces; l != NULL; l = l->next) {
      ModelSpaceInfo *space = (ModelSpaceInfo*)l->data;

      // Skip if we've already processed this space
      if (g_hash_table_contains(processed_spaces, space->uuid)) {
        continue;
      }

      char *parent_uuid = NULL;
      if (model_get_space_parent_uuid(tree_view->canvas_data->model,
                                     space->uuid, &parent_uuid) != 0) {
        if ((!parent_uuid && !space_uuid) || (parent_uuid && space_uuid && g_strcmp0(parent_uuid, space_uuid) == 0)) {
          // This is a direct child space - mark as processed
          g_hash_table_add(processed_spaces, g_strdup(space->uuid));

          GtkTreeIter child_iter;
          gtk_tree_store_append(tree_view->tree_store, &child_iter, parent_iter);

          gboolean is_current = (tree_view->canvas_data->model->current_space_uuid &&
                                g_strcmp0(space->uuid, tree_view->canvas_data->model->current_space_uuid) == 0);
          gboolean should_expand = (expansion_path && g_list_find_custom(expansion_path, space->uuid, (GCompareFunc)g_strcmp0) != NULL);

          gtk_tree_store_set(tree_view->tree_store, &child_iter,
                            SPACE_TREE_COL_NAME, space->name,
                            SPACE_TREE_COL_UUID, space->uuid,
                            SPACE_TREE_COL_TYPE, "space",
                            SPACE_TREE_COL_ELEMENT_TYPE, "",
                            SPACE_TREE_COL_IS_CURRENT, is_current,
                            SPACE_TREE_COL_IS_LOADED, should_expand, // Mark as loaded if we're expanding it
                            SPACE_TREE_COL_SPACE_UUID, space_uuid,
                            -1);

          // Load elements for this child space
          load_space_elements(tree_view, &child_iter, space->uuid);

          // If this space is on the path to current space, expand it recursively
          // Only if it actually has children (to avoid unnecessary recursion)
          if (should_expand) {
            // Check if this space actually has children before recursing
            gboolean has_children = FALSE;
            GList *check_spaces = NULL;
            if (model_get_all_spaces(tree_view->canvas_data->model, &check_spaces) != 0) {
              for (GList *check_l = check_spaces; check_l != NULL; check_l = check_l->next) {
                ModelSpaceInfo *check_space = (ModelSpaceInfo*)check_l->data;
                char *check_parent_uuid = NULL;
                if (model_get_space_parent_uuid(tree_view->canvas_data->model, check_space->uuid, &check_parent_uuid) != 0) {
                  if (check_parent_uuid && g_strcmp0(check_parent_uuid, space->uuid) == 0) {
                    has_children = TRUE;
                    g_free(check_parent_uuid);
                    break;
                  }
                  g_free(check_parent_uuid);
                }
              }
              g_list_free_full(check_spaces, (GDestroyNotify)model_free_space_info);
            }

            if (has_children) {
              space_tree_view_load_children_lazy_internal(tree_view, &child_iter, space->uuid, expansion_path, processed_spaces);
            }
          }
        }
        g_free(parent_uuid);
      }
    }
    g_list_free_full(all_spaces, (GDestroyNotify)model_free_space_info);
  }
}

// Wrapper function to initialize the processed spaces hash table
static void space_tree_view_load_children_lazy(SpaceTreeView *tree_view, GtkTreeIter *parent_iter,
                                              const char *space_uuid, GList *expansion_path, GHashTable *processed_spaces) {
  // Use the passed hash table to avoid duplicates across all calls
  space_tree_view_load_children_lazy_internal(tree_view, parent_iter, space_uuid, expansion_path, processed_spaces);
}

// Build initial tree with lazy loading - expand only to current space
static void build_initial_tree(SpaceTreeView *tree_view) {
  if (!tree_view || !tree_view->canvas_data || !tree_view->canvas_data->model || !tree_view->tree_store) {
    return;
  }

  tree_view->is_rebuilding = TRUE;
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view->tree_view), NULL);
  gtk_tree_store_clear(tree_view->tree_store);
  gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view->tree_view), GTK_TREE_MODEL(tree_view->tree_store));
  tree_view->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view->tree_view));
  gtk_tree_selection_set_mode(tree_view->selection, GTK_SELECTION_SINGLE);

  GList *all_spaces = NULL;
  if (model_get_all_spaces(tree_view->canvas_data->model, &all_spaces) == 0) {
    tree_view->is_rebuilding = FALSE;
    return;
  }

  // Build path to current space for expansion
  const char *current_space_uuid = tree_view->canvas_data->model->current_space_uuid;
  GList *expansion_path = build_path_to_space(tree_view, current_space_uuid);

  // Create a global processed spaces hash table to prevent duplicates
  GHashTable *processed_spaces = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  // Find and add all root spaces (spaces with no parent)
  for (GList *l = all_spaces; l != NULL; l = l->next) {
    ModelSpaceInfo *space = (ModelSpaceInfo*)l->data;
    char *parent_uuid = NULL;
    model_get_space_parent_uuid(tree_view->canvas_data->model, space->uuid, &parent_uuid);

    if (g_hash_table_contains(processed_spaces, space->uuid)) {
      g_free(parent_uuid);
      continue;
    }

    if (!parent_uuid) { // This is a root space
      GtkTreeIter root_iter;
      gtk_tree_store_append(tree_view->tree_store, &root_iter, NULL);

      gboolean is_current = (current_space_uuid && g_strcmp0(space->uuid, current_space_uuid) == 0);

      gtk_tree_store_set(tree_view->tree_store, &root_iter,
                        SPACE_TREE_COL_NAME, space->name,
                        SPACE_TREE_COL_UUID, space->uuid,
                        SPACE_TREE_COL_TYPE, "space",
                        SPACE_TREE_COL_ELEMENT_TYPE, "",
                        SPACE_TREE_COL_IS_CURRENT, is_current,
                        SPACE_TREE_COL_IS_LOADED, TRUE, // Mark root as loaded
                        SPACE_TREE_COL_SPACE_UUID, "",
                        -1);

      // Track processed root to avoid duplicates if re-encountered
      g_hash_table_add(processed_spaces, g_strdup(space->uuid));

      // Load root-level elements so the active root space shows its content
      load_space_elements(tree_view, &root_iter, space->uuid);

      // Load children for the root and expand the path to current
      space_tree_view_load_children_lazy(tree_view, &root_iter, space->uuid, expansion_path, processed_spaces);
    }

    g_free(parent_uuid);
  }

  // Clean up
  g_list_free_full(all_spaces, (GDestroyNotify)model_free_space_info);
  g_list_free_full(expansion_path, (GDestroyNotify)g_free);
  g_hash_table_destroy(processed_spaces);

  // Expand the tree view to show the current space
  expand_to_current_space(tree_view, current_space_uuid);
  select_current_space_node(tree_view);
  tree_view->is_rebuilding = FALSE;
}

// Tree view row selection callback
static void on_tree_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
  SpaceTreeView *tree_view = (SpaceTreeView*)user_data;

  // Safety checks
  if (!tree_view) {
    return;
  }
  if (tree_view->suppress_selection_signal) {
    return;
  }
  if (!selection) {
    return;
  }
  if (!tree_view->canvas_data) {
    return;
  }

  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    gchar *uuid = NULL, *node_type = NULL;
    gtk_tree_model_get(model, &iter,
                      SPACE_TREE_COL_UUID, &uuid,
                      SPACE_TREE_COL_TYPE, &node_type,
                      -1);

    if (uuid && node_type && g_strcmp0(node_type, "space") == 0) {
      // Safety check for model
      if (!tree_view->canvas_data->model) {
        if (uuid) g_free(uuid);
        if (node_type) g_free(node_type);
        return;
      }

      // Save current space before switching
      model_save_elements(tree_view->canvas_data->model);

      // Navigate to the selected space
      tree_view->suppress_selection_signal = TRUE;
      switch_to_space(tree_view->canvas_data, uuid);
      build_initial_tree(tree_view);
      select_current_space_node(tree_view);
      tree_view->suppress_selection_signal = FALSE;
    }

    if (uuid) g_free(uuid);
    if (node_type) g_free(node_type);
  }
}

// Row expansion callback for lazy loading
static void on_tree_row_expanded(GtkTreeView *tree_view_widget, GtkTreeIter *iter,
                                GtkTreePath *path, gpointer user_data) {
  SpaceTreeView *tree_view = (SpaceTreeView*)user_data;

  // Safety checks
  if (!tree_view || !iter || !tree_view->tree_store) return;
  if (tree_view->is_rebuilding) {
    return;
  }

  if (tree_view->is_rebuilding) {
    return;
  }

  GtkTreeModel *model = GTK_TREE_MODEL(tree_view->tree_store);

  gchar *uuid = NULL, *node_type = NULL, *parent_uuid = NULL;
  gboolean is_loaded = FALSE;

  gtk_tree_model_get(model, iter,
                    SPACE_TREE_COL_UUID, &uuid,
                    SPACE_TREE_COL_TYPE, &node_type,
                    SPACE_TREE_COL_IS_LOADED, &is_loaded,
                    SPACE_TREE_COL_SPACE_UUID, &parent_uuid,
                    -1);

  // Only load if it's a space and hasn't been loaded yet
  if (uuid && node_type && g_strcmp0(node_type, "space") == 0 && !is_loaded) {
    // Validate that the iterator is still valid after getting the path
    GtkTreeIter temp_iter;
    if (gtk_tree_model_get_iter(model, &temp_iter, path)) {
      // Load children for this space on expansion
      GHashTable *processed_spaces = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
      space_tree_view_load_children_lazy(tree_view, &temp_iter, uuid, NULL, processed_spaces);
      g_hash_table_destroy(processed_spaces);

      // Mark as loaded using the validated iterator
      gtk_tree_store_set(tree_view->tree_store, &temp_iter,
                        SPACE_TREE_COL_IS_LOADED, TRUE,
                        -1);
    }
  }

  if (uuid) g_free(uuid);
  if (node_type) g_free(node_type);
  if (parent_uuid) g_free(parent_uuid);
}

// Row collapse callback for memory deallocation
static void on_tree_row_collapsed(GtkTreeView *tree_view_widget, GtkTreeIter *iter,
                                 GtkTreePath *path, gpointer user_data) {
  SpaceTreeView *tree_view = (SpaceTreeView*)user_data;

  // Safety checks
  if (!tree_view || !iter || !tree_view->tree_store) return;
  if (tree_view->is_rebuilding) return;

  (void)tree_view_widget; // Unused
  (void)iter;

  GtkTreeModel *model = GTK_TREE_MODEL(tree_view->tree_store);

  GtkTreeIter safe_iter;
  if (!gtk_tree_model_get_iter(model, &safe_iter, path)) {
    return;
  }

  gchar *uuid = NULL, *node_type = NULL;

  gtk_tree_model_get(model, &safe_iter,
                    SPACE_TREE_COL_UUID, &uuid,
                    SPACE_TREE_COL_TYPE, &node_type,
                    -1);

  if (tree_view->is_rebuilding) {
    if (uuid) g_free(uuid);
    if (node_type) g_free(node_type);
    return;
  }

  // If it's a space, deallocate its children from memory
  if (uuid && node_type && g_strcmp0(node_type, "space") == 0) {
    gboolean is_current_space = FALSE;
    const char *current_uuid = tree_view->canvas_data->model ? tree_view->canvas_data->model->current_space_uuid : NULL;
    if (uuid && current_uuid && g_strcmp0(uuid, current_uuid) == 0) {
      is_current_space = TRUE;
    }

    if (is_current_space) {
      char *parent_uuid = NULL;
      if (model_get_space_parent_uuid(tree_view->canvas_data->model, uuid, &parent_uuid) && parent_uuid) {
        // Check for self-reference cycle before switching
        if (g_strcmp0(uuid, parent_uuid) != 0) {
          tree_view->suppress_selection_signal = TRUE;
          switch_to_space(tree_view->canvas_data, parent_uuid);
          tree_view->suppress_selection_signal = FALSE;
          if (!tree_view->is_rebuilding) {
            g_idle_add(refresh_tree_view_idle, tree_view);
          }
        } else {
          printf("WARNING: Space %s has itself as parent, ignoring collapse\n", uuid);
        }
      }
      g_free(parent_uuid);
      if (uuid) g_free(uuid);
      if (node_type) g_free(node_type);
      return;
    }

    // Use the path to get a fresh iterator for safety
    // Remove all child nodes to free memory - safe way
    GtkTreeIter child_iter;
    while (gtk_tree_model_iter_children(model, &child_iter, &safe_iter)) {
      // Always get the first child and remove it
      // This is safe because we get a fresh iterator each time
      if (!gtk_tree_store_remove(tree_view->tree_store, &child_iter)) {
        break; // No more children
      }
    }

    // Mark as not loaded so it can be expanded again later
    gtk_tree_store_set(tree_view->tree_store, &safe_iter,
                      SPACE_TREE_COL_IS_LOADED, FALSE,
                      -1);

    // If we collapsed the currently active space, switch to its parent
    // handled above for current space
  }

  if (uuid) g_free(uuid);
  if (node_type) g_free(node_type);
}


// Load children for a space (child spaces + elements) - on-demand loading
void space_tree_view_load_children(SpaceTreeView *tree_view, GtkTreeIter *parent_iter,
                                  const char *space_uuid, int depth) {
  // Safety checks
  if (!tree_view || !parent_iter || !space_uuid || !tree_view->canvas_data || !tree_view->canvas_data->model) {
    return;
  }

  // This is now simplified - just load direct children without depth limits
  GList *all_spaces = NULL;
  if (model_get_all_spaces(tree_view->canvas_data->model, &all_spaces) != 0) {
    for (GList *l = all_spaces; l != NULL; l = l->next) {
      ModelSpaceInfo *space = (ModelSpaceInfo*)l->data;

      char *parent_uuid = NULL;
      if (model_get_space_parent_uuid(tree_view->canvas_data->model,
                                     space->uuid, &parent_uuid) != 0) {
        if ((!parent_uuid && !space_uuid) || (parent_uuid && space_uuid && g_strcmp0(parent_uuid, space_uuid) == 0)) {
          // This is a child space
          GtkTreeIter child_iter;
          gtk_tree_store_append(tree_view->tree_store, &child_iter, parent_iter);

          gboolean is_current = (tree_view->canvas_data->model->current_space_uuid &&
                                g_strcmp0(space->uuid, tree_view->canvas_data->model->current_space_uuid) == 0);

          gtk_tree_store_set(tree_view->tree_store, &child_iter,
                            SPACE_TREE_COL_NAME, space->name,
                            SPACE_TREE_COL_UUID, space->uuid,
                            SPACE_TREE_COL_TYPE, "space",
                            SPACE_TREE_COL_ELEMENT_TYPE, "",
                            SPACE_TREE_COL_IS_CURRENT, is_current,
                            SPACE_TREE_COL_IS_LOADED, FALSE, // Always mark as not loaded for lazy expansion
                            SPACE_TREE_COL_SPACE_UUID, space_uuid,
                            -1);

          // Load elements for this space (but not child spaces - that's done on demand)
          load_space_elements(tree_view, &child_iter, space->uuid);
        }
        g_free(parent_uuid);
      }
    }
    g_list_free_full(all_spaces, (GDestroyNotify)model_free_space_info);
  }

}


// Custom cell renderer for different node types
static void tree_cell_data_func(GtkTreeViewColumn *column,
                               GtkCellRenderer *renderer,
                               GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gpointer user_data) {
  gboolean is_current;
  gchar *node_type;

  gtk_tree_model_get(model, iter,
                    SPACE_TREE_COL_IS_CURRENT, &is_current,
                    SPACE_TREE_COL_TYPE, &node_type,
                    -1);

  if (is_current) {
    g_object_set(renderer, "weight", PANGO_WEIGHT_BOLD, NULL);
    g_object_set(renderer, "foreground", "#2563eb", NULL);  // Blue for current
  } else if (g_strcmp0(node_type, "space") == 0) {
    g_object_set(renderer, "weight", PANGO_WEIGHT_NORMAL, NULL);
    g_object_set(renderer, "foreground", "#059669", NULL);  // Green for spaces
  } else {
    g_object_set(renderer, "weight", PANGO_WEIGHT_NORMAL, NULL);
    g_object_set(renderer, "foreground", NULL, NULL);       // Default for elements
    g_object_set(renderer, "style", PANGO_STYLE_NORMAL, NULL);
  }

  g_free(node_type);
}

SpaceTreeView *space_tree_view_new(CanvasData *canvas_data) {
  SpaceTreeView *tree_view = g_new0(SpaceTreeView, 1);
  tree_view->canvas_data = canvas_data;
  tree_view->suppress_selection_signal = FALSE;
  tree_view->is_rebuilding = FALSE;
  tree_view->idle_refresh_handle = 0;
  tree_view->is_built = FALSE;  // Track if tree has been built

  // Create tree store with more columns
  tree_view->tree_store = gtk_tree_store_new(SPACE_TREE_N_COLUMNS,
                                           G_TYPE_STRING,   // Name
                                           G_TYPE_STRING,   // UUID
                                           G_TYPE_STRING,   // Type
                                           G_TYPE_STRING,   // Element type
                                           G_TYPE_BOOLEAN,  // Is current
                                           G_TYPE_BOOLEAN,  // Is loaded
                                           G_TYPE_STRING);  // Space UUID

  // Create tree view
  tree_view->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_view->tree_store));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view->tree_view), TRUE);
  gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tree_view->tree_view), TRUE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_view->tree_view), TRUE);

  // Configure expander appearance for better + and - indicators
  // Set to 0 since the expander arrows already provide visual hierarchy
  gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree_view->tree_view), 0);
  gtk_tree_view_set_expander_column(GTK_TREE_VIEW(tree_view->tree_view), NULL); // Auto-select first column

  // Create column
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
    "Current Space Elements", renderer, "text", SPACE_TREE_COL_NAME, NULL);

  gtk_tree_view_column_set_cell_data_func(column, renderer,
                                         tree_cell_data_func,
                                         tree_view, NULL);

  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view->tree_view), column);

  // Set this column as the expander column to get proper + and - indicators
  gtk_tree_view_set_expander_column(GTK_TREE_VIEW(tree_view->tree_view), column);

  // Get selection
  tree_view->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view->tree_view));
  gtk_tree_selection_set_mode(tree_view->selection, GTK_SELECTION_SINGLE);

  // Connect signals
  g_signal_connect(tree_view->selection, "changed",
                   G_CALLBACK(on_tree_selection_changed), tree_view);
  g_signal_connect(tree_view->tree_view, "row-expanded",
                   G_CALLBACK(on_tree_row_expanded), tree_view);
  g_signal_connect(tree_view->tree_view, "row-collapsed",
                   G_CALLBACK(on_tree_row_collapsed), tree_view);

  // Don't build initial tree - do it lazily when first opened
  // build_initial_tree(tree_view);

  return tree_view;
}

void space_tree_view_free(SpaceTreeView *tree_view) {
  if (!tree_view) return;
  if (tree_view->tree_view && GTK_IS_WIDGET(tree_view->tree_view)) {
    gtk_widget_unparent(tree_view->tree_view);
  }
  g_clear_object(&tree_view->tree_store);
  g_free(tree_view);
}

void space_tree_view_refresh(SpaceTreeView *tree_view) {
  if (!tree_view) return;
  build_initial_tree(tree_view);
}

void space_tree_view_navigate_to_selected(SpaceTreeView *tree_view) {
  if (!tree_view) return;

  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected(tree_view->selection, &model, &iter)) {
    gchar *uuid, *node_type;

    gtk_tree_model_get(model, &iter,
                      SPACE_TREE_COL_UUID, &uuid,
                      SPACE_TREE_COL_TYPE, &node_type,
                      -1);

    if (uuid && node_type && tree_view->canvas_data) {
      if (g_strcmp0(node_type, "space") == 0) {
        switch_to_space(tree_view->canvas_data, uuid);
        // Don't refresh tree - just switch the canvas view (consistent with on_tree_selection_changed)
      }
    }

    g_free(uuid);
    g_free(node_type);
  }
}

GtkWidget *space_tree_view_get_widget(SpaceTreeView *tree_view) {
  return tree_view ? tree_view->tree_view : NULL;
}

void space_tree_view_update_current_space(SpaceTreeView *tree_view, const char *space_uuid) {
  if (!tree_view) return;
  // Instead of refreshing entire tree, just update the current highlighting
  // This prevents the double-building issue
}
