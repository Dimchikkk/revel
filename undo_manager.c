// undo_manager.c
#include "undo_manager.h"
#include "element.h"
#include "model.h"
#include <string.h>
#include "canvas.h"
#include "canvas_core.h"

UndoManager* undo_manager_new(Model *model) {
  UndoManager *manager = g_new0(UndoManager, 1);
  manager->undo_stack = NULL;
  manager->redo_stack = NULL;
  manager->action_log = NULL;
  manager->log_window = NULL;
  manager->log_store = NULL;
  manager->model = model;
  return manager;
}

static void update_log_window(UndoManager *manager) {
  if (!manager || !manager->log_store) return;

  // Clear existing entries
  gtk_list_store_clear(manager->log_store);

  // Add actions from undo stack (current timeline)
  for (GList *l = manager->undo_stack; l != NULL; l = l->next) {
    Action *action = (Action*)l->data;

    GtkTreeIter iter;
    gtk_list_store_append(manager->log_store, &iter);

    gchar *timestamp_str = g_date_time_format(action->timestamp, "%H:%M:%S");
    gtk_list_store_set(manager->log_store, &iter,
                       0, action->description,
                       1, timestamp_str,
                       -1);
    g_free(timestamp_str);
  }

  // Optionally, add actions from redo stack (alternative timeline)
  // This would show the full history including undone actions
  for (GList *l = manager->redo_stack; l != NULL; l = l->next) {
    Action *action = (Action*)l->data;

    GtkTreeIter iter;
    gtk_list_store_append(manager->log_store, &iter);

    gchar *timestamp_str = g_date_time_format(action->timestamp, "%H:%M:%S");
    gtk_list_store_set(manager->log_store, &iter,
                       0, action->description,
                       1, timestamp_str,
                       -1);
    g_free(timestamp_str);
  }
}

static const char* element_get_type_name(ModelElement *element) {
  if (!element || !element->type) return "Unknown";

  switch (element->type->type) {
  case ELEMENT_NOTE: return "Note";
  case ELEMENT_PAPER_NOTE: return "Paper Note";
  case ELEMENT_CONNECTION: return "Connection";
  case ELEMENT_SPACE: return "Space";
  case ELEMENT_MEDIA_FILE: return "Media File";
  case ELEMENT_FREEHAND_DRAWING: return "Freehand Drawing";
  case ELEMENT_SHAPE: return "Shape";
  default: return "Unknown";
  }
}

static Action* action_new(ActionType type, gpointer data, const char *description) {
  Action *action = g_new0(Action, 1);
  action->type = type;
  action->data = data;
  action->description = g_strdup(description);
  action->timestamp = g_date_time_new_now_local();
  return action;
}

static void free_action_data(Action *action) {
  if (!action || !action->data) return;

  switch (action->type) {
  case ACTION_MOVE_ELEMENT:
  case ACTION_RESIZE_ELEMENT:
  case ACTION_CHANGE_COLOR:
    // These are simple structs that can be freed directly
    g_free(action->data);
    break;
  case ACTION_DELETE_ELEMENT: {
    DeleteData *delete_data = (DeleteData*)action->data;
    if (delete_data) {
      g_free(delete_data);
    }
    break;
  }
  case ACTION_CREATE_ELEMENT: {
    CreateData *create_data = (CreateData*)action->data;
    if (create_data) {
      g_free(create_data);
    }
    break;
  }
  case ACTION_EDIT_TEXT: {
    TextData *text_data = (TextData*)action->data;
    if (text_data) {
      g_free(text_data->old_text);
      g_free(text_data->new_text);
      g_free(text_data);
    }
    break;
  }
  }
}

static void free_action(Action *action) {
  if (!action) return;
  free_action_data(action);
  g_free(action->description);
  if (action->timestamp) g_date_time_unref(action->timestamp);
  g_free(action);
}

void undo_manager_free(UndoManager *manager) {
  if (!manager) return;

  g_list_free_full(manager->undo_stack, (GDestroyNotify)free_action);
  g_list_free_full(manager->redo_stack, (GDestroyNotify)free_action);
  g_list_free_full(manager->action_log, (GDestroyNotify)free_action);

  if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
    gtk_window_destroy(GTK_WINDOW(manager->log_window));
  }

  if (manager->log_store) {
    g_object_unref(manager->log_store);
  }

  g_free(manager);
}

// Action creation functions
void undo_manager_push_move_action(UndoManager *manager, ModelElement *element,
                                   int old_x, int old_y,
                                   int new_x, int new_y) {
  if (!manager || !element) return;

  MoveData *move_data = g_new0(MoveData, 1);
  move_data->element = element;
  move_data->old_x = old_x;
  move_data->old_y = old_y;
  move_data->new_x = new_x;
  move_data->new_y = new_y;

  char *description = g_strdup_printf("Moved %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_MOVE_ELEMENT, move_data, description);
  g_free(description);
}

void undo_manager_push_resize_action(UndoManager *manager, ModelElement *element,
                                     int old_width, int old_height,
                                     int new_width, int new_height) {
  if (!manager || !element) return;

  ResizeData *resize_data = g_new0(ResizeData, 1);
  resize_data->element = element;
  resize_data->old_width = old_width;
  resize_data->old_height = old_height;
  resize_data->new_width = new_width;
  resize_data->new_height = new_height;

  char *description = g_strdup_printf("Resized %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_RESIZE_ELEMENT, resize_data, description);
  g_free(description);
}

void undo_manager_push_text_action(UndoManager *manager, ModelElement *element,
                                   const char *old_text, const char *new_text) {
  if (!manager || !element) return;

  TextData *text_data = g_new0(TextData, 1);
  text_data->element = element;
  text_data->old_text = g_strdup(old_text);
  text_data->new_text = g_strdup(new_text);

  char *description = g_strdup_printf("Edited text in %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_EDIT_TEXT, text_data, description);
  g_free(description);
}

void undo_manager_push_color_action(UndoManager *manager, ModelElement *element,
                                    double old_r, double old_g, double old_b, double old_a,
                                    double new_r, double new_g, double new_b, double new_a) {
  if (!manager || !element || !element->bg_color) return;

  ColorData *color_data = g_new0(ColorData, 1);
  color_data->element = element;
  color_data->old_r = old_r;
  color_data->old_g = old_g;
  color_data->old_b = old_b;
  color_data->old_a = old_a;
  color_data->new_r = new_r;
  color_data->new_g = new_g;
  color_data->new_b = new_b;
  color_data->new_a = new_a;

  char *description = g_strdup_printf("Changed color of %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_CHANGE_COLOR, color_data, description);
  g_free(description);
}

void undo_manager_push_delete_action(UndoManager *manager, ModelElement *element) {
  if (!manager || !element) return;

  DeleteData *delete_data = g_new0(DeleteData, 1);
  delete_data->element = element;
  delete_data->previous_state = element->state;  // Store the previous state

  char *description = g_strdup_printf("Deleted %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_DELETE_ELEMENT, delete_data, description);
  g_free(description);
}

void undo_manager_push_action(UndoManager *manager, ActionType type, gpointer data, const char *description) {
  if (!manager) return;

  Action *action = action_new(type, data, description);
  manager->undo_stack = g_list_append(manager->undo_stack, action);

  // Create a proper copy for the log
  gpointer log_data = NULL;
  switch (type) {
  case ACTION_EDIT_TEXT: {
    TextData *original = (TextData*)data;
    TextData *copy = g_new0(TextData, 1);
    copy->element = original->element;
    copy->old_text = g_strdup(original->old_text);
    copy->new_text = g_strdup(original->new_text);
    log_data = copy;
    break;
  }
  case ACTION_CHANGE_COLOR: {
    ColorData *original = (ColorData*)data;
    ColorData *copy = g_new0(ColorData, 1);
    copy->element = original->element;
    copy->old_r = original->old_r;
    copy->old_g = original->old_g;
    copy->old_b = original->old_b;
    copy->old_a = original->old_a;
    copy->new_r = original->new_r;
    copy->new_g = original->new_g;
    copy->new_b = original->new_b;
    copy->new_a = original->new_a;
    log_data = copy;
    break;
  }
  case ACTION_MOVE_ELEMENT: {
    MoveData *original = (MoveData*)data;
    MoveData *copy = g_new0(MoveData, 1);
    *copy = *original;
    log_data = copy;
    break;
  }
  case ACTION_RESIZE_ELEMENT: {
    ResizeData *original = (ResizeData*)data;
    ResizeData *copy = g_new0(ResizeData, 1);
    *copy = *original;
    log_data = copy;
    break;
  }
  case ACTION_DELETE_ELEMENT: {
    DeleteData *original = (DeleteData*)data;
    DeleteData *copy = g_new0(DeleteData, 1);
    copy->element = original->element;
    copy->previous_state = original->previous_state;
    log_data = copy;
    break;
  }
  case ACTION_CREATE_ELEMENT: {
    CreateData *original = (CreateData*)data;
    CreateData *copy = g_new0(CreateData, 1);
    copy->element = original->element;
    copy->initial_state = original->initial_state;
    log_data = copy;
    break;
  }
  }

  Action *log_action = action_new(type, log_data, description);
  manager->action_log = g_list_append(manager->action_log, log_action);

  // Clear redo stack completely when new action is pushed
  g_list_free_full(manager->redo_stack, (GDestroyNotify)free_action);
  manager->redo_stack = NULL;

  // Update log window if it's open
  if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
    update_log_window(manager);
  }
}

// Enhanced undo/redo implementation
void undo_manager_undo(UndoManager *manager) {
  if (!manager || !manager->undo_stack) return;

  GList *last = g_list_last(manager->undo_stack);
  Action *action = (Action*)last->data;

  switch (action->type) {
  case ACTION_MOVE_ELEMENT: {
    MoveData *move_data = (MoveData*)action->data;
    if (move_data->element->position) {
      // Restore old position in model
      model_update_position(manager->model, move_data->element,
                            move_data->old_x, move_data->old_y,
                            move_data->element->position->z);
    }
    break;
  }
  case ACTION_RESIZE_ELEMENT: {
    ResizeData *resize_data = (ResizeData*)action->data;
    if (resize_data->element->size) {
      // Restore old size in model
      model_update_size(manager->model, resize_data->element,
                        resize_data->old_width, resize_data->old_height);
    }
    break;
  }
  case ACTION_EDIT_TEXT: {
    TextData *text_data = (TextData*)action->data;
    if (text_data->element->text) {
      // Restore old text in model
      model_update_text(manager->model, text_data->element, text_data->old_text);
    }
    break;
  }
  case ACTION_CHANGE_COLOR: {
    ColorData *color_data = (ColorData*)action->data;
    if (color_data->element->bg_color) {
      // Restore old color in model
      model_update_color(manager->model, color_data->element,
                         color_data->old_r, color_data->old_g,
                         color_data->old_b, color_data->old_a);
    }
    break;
  }
  case ACTION_DELETE_ELEMENT: {
    DeleteData *delete_data = (DeleteData*)action->data;
    // Restore the previous state
    delete_data->element->state = delete_data->previous_state;

    // Make sure it's in the elements hash table if it should be
    if (delete_data->previous_state == MODEL_STATE_SAVED &&
        !g_hash_table_contains(manager->model->elements, delete_data->element->uuid)) {
      g_hash_table_insert(manager->model->elements,
                          g_strdup(delete_data->element->uuid),
                          delete_data->element);
    }
    break;
  }
  case ACTION_CREATE_ELEMENT: {
    CreateData *create_data = (CreateData*)action->data;
    // For creation undo, mark as deleted
    create_data->element->state = MODEL_STATE_DELETED;
    break;
  }
  }

  // Move from undo to redo stack
  manager->undo_stack = g_list_delete_link(manager->undo_stack, last);
  manager->redo_stack = g_list_append(manager->redo_stack, action);
}

void undo_manager_redo(UndoManager *manager) {
  if (!manager || !manager->redo_stack) return;

  GList *last = g_list_last(manager->redo_stack);
  Action *action = (Action*)last->data;

  switch (action->type) {
  case ACTION_MOVE_ELEMENT: {
    MoveData *move_data = (MoveData*)action->data;
    if (move_data->element->position) {
      // Apply new position in model
      model_update_position(manager->model, move_data->element,
                            move_data->new_x, move_data->new_y,
                            move_data->element->position->z);
    }
    break;
  }
  case ACTION_RESIZE_ELEMENT: {
    ResizeData *resize_data = (ResizeData*)action->data;
    if (resize_data->element->size) {
      // Apply new size in model
      model_update_size(manager->model, resize_data->element,
                        resize_data->new_width, resize_data->new_height);
    }
    break;
  }
  case ACTION_EDIT_TEXT: {
    TextData *text_data = (TextData*)action->data;
    if (text_data->element->text) {
      // Apply new text in model
      model_update_text(manager->model, text_data->element, text_data->new_text);
    }
    break;
  }
  case ACTION_CHANGE_COLOR: {
    ColorData *color_data = (ColorData*)action->data;
    if (color_data->element->bg_color) {
      // Apply new color in model
      model_update_color(manager->model, color_data->element,
                         color_data->new_r, color_data->new_g,
                         color_data->new_b, color_data->new_a);
    }
    break;
  }
  case ACTION_DELETE_ELEMENT: {
    DeleteData *delete_data = (DeleteData*)action->data;
    // Delete element again
    delete_data->element->state = MODEL_STATE_DELETED;
    break;
  }
  case ACTION_CREATE_ELEMENT: {
    CreateData *create_data = (CreateData*)action->data;
    // For creation redo, restore the initial state
    create_data->element->state = create_data->initial_state;
    break;
  }
  }

  // Move from redo to undo stack
  manager->redo_stack = g_list_delete_link(manager->redo_stack, last);
  manager->undo_stack = g_list_append(manager->undo_stack, action);
}

// Callback for when log window is destroyed
static void on_log_window_destroy(GtkWidget *widget, gpointer user_data) {
  UndoManager *manager = (UndoManager*)user_data;
  manager->log_window = NULL;
  if (manager->log_store) {
    g_object_unref(manager->log_store);
    manager->log_store = NULL;
  }
}

void show_action_log(CanvasData *data) {
  UndoManager *manager = data->undo_manager;

  // If log window already exists, just show it and update
  if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
    update_log_window(manager);
    gtk_window_present(GTK_WINDOW(manager->log_window));
    return;
  }

  // Get the main window from the drawing area
  GtkWidget *main_window = gtk_widget_get_ancestor(data->drawing_area, GTK_TYPE_WINDOW);

  // Create new dialog
  GtkWidget *dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Available undo/redo actions");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 500);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));
  gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);

  // Store reference to the dialog
  manager->log_window = dialog;

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Create list store for the action log
  manager->log_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeView *tree_view = GTK_TREE_VIEW(gtk_tree_view_new());

  // Configure the tree view for expanded row height
  gtk_tree_view_set_fixed_height_mode(tree_view, FALSE); // Allow variable row heights

  // Create columns with expanded width
  GtkTreeViewColumn *action_col = gtk_tree_view_column_new();
  GtkCellRenderer *action_renderer = gtk_cell_renderer_text_new();

  // Set cell renderer properties for expanded height
  g_object_set(action_renderer,
               "ypad", 8,           // Vertical padding
               "height", 30,        // Minimum row height
               "wrap-mode", PANGO_WRAP_WORD, // Enable text wrapping
               "wrap-width", 350,   // Wrap width for action text
               NULL);

  gtk_tree_view_column_pack_start(action_col, action_renderer, TRUE);
  gtk_tree_view_column_set_title(action_col, "Action");
  gtk_tree_view_column_add_attribute(action_col, action_renderer, "text", 0);
  gtk_tree_view_column_set_expand(action_col, TRUE); // Make this column expand
  gtk_tree_view_column_set_min_width(action_col, 400); // Minimum width

  GtkTreeViewColumn *time_col = gtk_tree_view_column_new();
  GtkCellRenderer *time_renderer = gtk_cell_renderer_text_new();

  // Set cell renderer properties for expanded height
  g_object_set(time_renderer,
               "ypad", 8,           // Vertical padding
               "height", 30,        // Minimum row height
               NULL);

  gtk_tree_view_column_pack_start(time_col, time_renderer, TRUE);
  gtk_tree_view_column_set_title(time_col, "Time");
  gtk_tree_view_column_add_attribute(time_col, time_renderer, "text", 1);
  gtk_tree_view_column_set_min_width(time_col, 100); // Minimum width

  gtk_tree_view_append_column(tree_view, action_col);
  gtk_tree_view_append_column(tree_view, time_col);

  // Set the model
  gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(manager->log_store));

  // Populate with actions
  update_log_window(manager);

  GtkWidget *scrolled = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(tree_view));

  // Configure the scrolled window to expand and fill available space
  gtk_widget_set_hexpand(scrolled, TRUE);
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_widget_set_size_request(scrolled, -1, 400); // Minimum height of 400 pixels

  // Configure the content area to allow expansion
  gtk_widget_set_hexpand(content_area, TRUE);
  gtk_widget_set_vexpand(content_area, TRUE);

  gtk_box_append(GTK_BOX(content_area), scrolled);

  // Connect destroy signal to clear the reference
  g_signal_connect(dialog, "destroy", G_CALLBACK(on_log_window_destroy), manager);

  gtk_widget_show(dialog);
}

void undo_manager_push_create_action(UndoManager *manager, ModelElement *element) {
  if (!manager || !element) return;

  // For create actions, we need to store the initial state
  CreateData *create_data = g_new0(CreateData, 1);
  create_data->element = element;
  create_data->initial_state = element->state;

  char *description = g_strdup_printf("Created %s", element_get_type_name(element));
  undo_manager_push_action(manager, ACTION_CREATE_ELEMENT, create_data, description);
  g_free(description);
}

gboolean undo_manager_can_undo(UndoManager *manager) {
  return manager && manager->undo_stack != NULL;
}

gboolean undo_manager_can_redo(UndoManager *manager) {
  return manager && manager->redo_stack != NULL;
}

void on_undo_clicked(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  undo_manager_undo(data->undo_manager);
  // undo_manager_print_stacks(data->undo_manager);
  canvas_sync_with_model(data);
  gtk_widget_queue_draw(data->drawing_area);
}

void on_redo_clicked(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  undo_manager_redo(data->undo_manager);  // Pass the undo_manager, not the canvas data
  // undo_manager_print_stacks(data->undo_manager);
  canvas_sync_with_model(data);
  gtk_widget_queue_draw(data->drawing_area);
}

void undo_manager_print_stacks(UndoManager *manager) {
  if (!manager) {
    g_print("UndoManager is NULL\n");
    return;
  }

  g_print("\n=== UNDO STACK (size: %d) ===\n", g_list_length(manager->undo_stack));
  GList *iter = manager->undo_stack;
  int i = 1;
  while (iter != NULL) {
    Action *action = (Action*)iter->data;
    gchar *time_str = g_date_time_format(action->timestamp, "%H:%M:%S");
    g_print("%d. [%s] %s\n", i++, time_str, action->description);
    g_free(time_str);
    iter = iter->next;
  }

  g_print("\n=== REDO STACK (size: %d) ===\n", g_list_length(manager->redo_stack));
  iter = manager->redo_stack;
  i = 1;
  while (iter != NULL) {
    Action *action = (Action*)iter->data;
    gchar *time_str = g_date_time_format(action->timestamp, "%H:%M:%S");
    g_print("%d. [%s] %s\n", i++, time_str, action->description);
    g_free(time_str);
    iter = iter->next;
  }
  g_print("\n");
}

void on_log_clicked(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  show_action_log(data);
}

void undo_manager_reset(UndoManager *manager) {
  if (!manager) return;

  g_list_free_full(manager->undo_stack, (GDestroyNotify)free_action);
  manager->undo_stack = NULL;

  g_list_free_full(manager->redo_stack, (GDestroyNotify)free_action);
  manager->redo_stack = NULL;
}

static gboolean action_involves_element(Action *action, ModelElement *element) {
    if (!action || !action->data) return FALSE;

    switch (action->type) {
    case ACTION_MOVE_ELEMENT:
        return ((MoveData*)action->data)->element == element;
    case ACTION_RESIZE_ELEMENT:
        return ((ResizeData*)action->data)->element == element;
    case ACTION_EDIT_TEXT:
        return ((TextData*)action->data)->element == element;
    case ACTION_CHANGE_COLOR:
        return ((ColorData*)action->data)->element == element;
    case ACTION_DELETE_ELEMENT:
        return ((DeleteData*)action->data)->element == element;
    case ACTION_CREATE_ELEMENT:
        return ((CreateData*)action->data)->element == element;
    default:
        return FALSE;
    }
}

void undo_manager_remove_actions_for_element(UndoManager *manager, ModelElement *element) {
    if (!manager || !element) return;

    // Find all connected elements using BFS
    GList *connected_elements = find_connected_elements_bfs(manager->model, element->uuid);

    // Add the original element to the list
    connected_elements = g_list_prepend(connected_elements, element);

    // Remove actions for all connected elements
    for (GList *iter = connected_elements; iter != NULL; iter = iter->next) {
        ModelElement *current_element = (ModelElement*)iter->data;

        // Remove from undo stack
        GList *l = manager->undo_stack;
        while (l) {
            Action *action = (Action*)l->data;
            GList *next = l->next;
            if (action_involves_element(action, current_element)) {
                manager->undo_stack = g_list_remove(manager->undo_stack, action);
                free_action(action);
            }
            l = next;
        }

        // Remove from redo stack
        l = manager->redo_stack;
        while (l) {
            Action *action = (Action*)l->data;
            GList *next = l->next;
            if (action_involves_element(action, current_element)) {
                manager->redo_stack = g_list_remove(manager->redo_stack, action);
                free_action(action);
            }
            l = next;
        }

        // Remove from action log
        l = manager->action_log;
        while (l) {
            Action *action = (Action*)l->data;
            GList *next = l->next;
            if (action_involves_element(action, current_element)) {
                manager->action_log = g_list_remove(manager->action_log, action);
                free_action(action);
            }
            l = next;
        }
    }

    g_list_free(connected_elements);

    // Update log window if open
    if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
        update_log_window(manager);
    }
}
