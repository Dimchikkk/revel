#include "undo_manager.h"
#include "canvas.h"
#include "note.h"
#include "paper_note.h"
#include "connection.h"
#include <glib.h>

UndoManager* undo_manager_new() {
    UndoManager *manager = g_new0(UndoManager, 1);
    manager->undo_stack = NULL;
    manager->redo_stack = NULL;
    manager->action_log = NULL;
    manager->log_window = NULL;
    manager->log_store = NULL;
    return manager;
}

static const char* element_get_type_name(Element *element) {
    switch (element->type) {
        case ELEMENT_NOTE: return "Note";
        case ELEMENT_PAPER_NOTE: return "Paper Note";
        case ELEMENT_CONNECTION: return "Connection";
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

// Helper function to free action data based on type
static void free_action_data(Action *action) {
    if (!action || !action->data) return;

    switch (action->type) {
        case ACTION_MOVE_ELEMENT:
        case ACTION_RESIZE_ELEMENT:
        case ACTION_DELETE_ELEMENT:
            g_free(action->data);
            break;
        case ACTION_EDIT_TEXT:
            {
                TextData *text_data = (TextData*)action->data;
                g_free(text_data->old_text);
                g_free(text_data->new_text);
                g_free(text_data);
            }
            break;
        case ACTION_CREATE_NOTE:
        case ACTION_CREATE_PAPER_NOTE:
        case ACTION_CREATE_CONNECTION:
            // These are handled by the main element management
            break;
    }
}

void undo_manager_free(UndoManager *manager) {
    if (!manager) return;

    g_list_free_full(manager->undo_stack, (GDestroyNotify)free_action_data);
    g_list_free_full(manager->redo_stack, (GDestroyNotify)free_action_data);
    g_list_free_full(manager->action_log, (GDestroyNotify)free_action_data);

    if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
        gtk_window_destroy(GTK_WINDOW(manager->log_window));
    }

    if (manager->log_store) {
        g_object_unref(manager->log_store);
    }

    g_free(manager);
}

static void update_log_window(UndoManager *manager) {
    if (!manager->log_store) return;

    // Clear the current store
    gtk_list_store_clear(manager->log_store);

    // Only show actions that are still in the undo stack (reachable)
    GtkTreeIter iter;
    for (GList *l = manager->undo_stack; l != NULL; l = l->next) {
        Action *action = (Action*)l->data;
        gchar *time_str = g_date_time_format(action->timestamp, "%H:%M:%S");

        gtk_list_store_append(manager->log_store, &iter);
        gtk_list_store_set(manager->log_store, &iter,
                          0, action->description,
                          1, time_str,
                          -1);

        g_free(time_str);
    }
}

// New action creation functions
void undo_manager_push_move_action(UndoManager *manager, Element *element,
                                  double old_x, double old_y,
                                  double new_x, double new_y) {
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

void undo_manager_push_resize_action(UndoManager *manager, Element *element,
                                    double old_width, double old_height,
                                    double new_width, double new_height) {
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

void undo_manager_push_text_action(UndoManager *manager, Element *element,
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

void undo_manager_push_delete_action(UndoManager *manager, Element *element) {
    if (!manager || !element) return;

    DeleteData *delete_data = g_new0(DeleteData, 1);
    delete_data->element = element;

    char *description = g_strdup_printf("Deleted %s", element_get_type_name(element));
    undo_manager_push_action(manager, ACTION_DELETE_ELEMENT, delete_data, description);
    g_free(description);
}

void undo_manager_push_action(UndoManager *manager, ActionType type, gpointer data, const char *description) {
    if (!manager) return;

    Action *action = action_new(type, data, description);
    manager->undo_stack = g_list_append(manager->undo_stack, action);

    // For the log, we need to create a copy of the data for certain types
    gpointer log_data = data;
    if (type == ACTION_EDIT_TEXT) {
        // Deep copy text data for the log
        TextData *original = (TextData*)data;
        TextData *copy = g_new0(TextData, 1);
        copy->element = original->element;
        copy->old_text = g_strdup(original->old_text);
        copy->new_text = g_strdup(original->new_text);
        log_data = copy;
    } else if (type == ACTION_MOVE_ELEMENT || type == ACTION_RESIZE_ELEMENT || type == ACTION_DELETE_ELEMENT) {
        // Shallow copy for these types
        size_t size = 0;
        if (type == ACTION_MOVE_ELEMENT) size = sizeof(MoveData);
        else if (type == ACTION_RESIZE_ELEMENT) size = sizeof(ResizeData);
        else if (type == ACTION_DELETE_ELEMENT) size = sizeof(DeleteData);

        if (size > 0) {
            log_data = g_memdup2(data, size);
        }
    }

    Action *log_action = action_new(type, log_data, description);
    manager->action_log = g_list_append(manager->action_log, log_action);

    // Update the log window if it's open
    if (manager->log_window && gtk_widget_get_visible(manager->log_window)) {
        update_log_window(manager);
    }

    // Clear redo stack when new action is pushed
    g_list_free_full(manager->redo_stack, (GDestroyNotify)free_action_data);
    manager->redo_stack = NULL;
}

// Enhanced undo/redo implementation
void undo_manager_undo(CanvasData *data) {
    if (!data->undo_manager || !data->undo_manager->undo_stack) return;

    GList *last = g_list_last(data->undo_manager->undo_stack);
    Action *action = (Action*)last->data;

    switch (action->type) {
        case ACTION_CREATE_NOTE:
        case ACTION_CREATE_PAPER_NOTE:
        case ACTION_CREATE_CONNECTION: {
            Element *element = (Element*)action->data;
            element->hidden = TRUE;
            break;
        }
        case ACTION_MOVE_ELEMENT: {
            MoveData *move_data = (MoveData*)action->data;
            // Restore old position
            move_data->element->x = move_data->old_x;
            move_data->element->y = move_data->old_y;
            break;
        }
        case ACTION_RESIZE_ELEMENT: {
            ResizeData *resize_data = (ResizeData*)action->data;
            // Restore old size
            resize_data->element->width = resize_data->old_width;
            resize_data->element->height = resize_data->old_height;
            break;
        }
        case ACTION_EDIT_TEXT: {
            TextData *text_data = (TextData*)action->data;
            // Restore old text
            if (text_data->element->type == ELEMENT_NOTE) {
                Note *note = (Note*)text_data->element;
                g_free(note->text);
                note->text = g_strdup(text_data->old_text);
            } else if (text_data->element->type == ELEMENT_PAPER_NOTE) {
                PaperNote *paper_note = (PaperNote*)text_data->element;
                g_free(paper_note->text);
                paper_note->text = g_strdup(text_data->old_text);
            }
            break;
        }
        case ACTION_DELETE_ELEMENT: {
            DeleteData *delete_data = (DeleteData*)action->data;
            // Restore deleted element
            delete_data->element->hidden = FALSE;
            break;
        }
    }

    // Move from undo to redo stack
    data->undo_manager->undo_stack = g_list_delete_link(data->undo_manager->undo_stack, last);
    data->undo_manager->redo_stack = g_list_append(data->undo_manager->redo_stack, action);

    gtk_widget_queue_draw(data->drawing_area);
}

void undo_manager_redo(CanvasData *data) {
    if (!data->undo_manager || !data->undo_manager->redo_stack) return;

    GList *last = g_list_last(data->undo_manager->redo_stack);
    Action *action = (Action*)last->data;

    switch (action->type) {
        case ACTION_CREATE_NOTE:
        case ACTION_CREATE_PAPER_NOTE:
        case ACTION_CREATE_CONNECTION: {
            Element *element = (Element*)action->data;
            element->hidden = FALSE;
            break;
        }
        case ACTION_MOVE_ELEMENT: {
            MoveData *move_data = (MoveData*)action->data;
            // Apply new position
            move_data->element->x = move_data->new_x;
            move_data->element->y = move_data->new_y;
            break;
        }
        case ACTION_RESIZE_ELEMENT: {
            ResizeData *resize_data = (ResizeData*)action->data;
            // Apply new size
            resize_data->element->width = resize_data->new_width;
            resize_data->element->height = resize_data->new_height;
            break;
        }
        case ACTION_EDIT_TEXT: {
            TextData *text_data = (TextData*)action->data;
            // Apply new text
            if (text_data->element->type == ELEMENT_NOTE) {
                Note *note = (Note*)text_data->element;
                g_free(note->text);
                note->text = g_strdup(text_data->new_text);
            } else if (text_data->element->type == ELEMENT_PAPER_NOTE) {
                PaperNote *paper_note = (PaperNote*)text_data->element;
                g_free(paper_note->text);
                paper_note->text = g_strdup(text_data->new_text);
            }
            break;
        }
        case ACTION_DELETE_ELEMENT: {
            DeleteData *delete_data = (DeleteData*)action->data;
            // Delete element again
            delete_data->element->hidden = TRUE;
            break;
        }
    }

    // Move from redo to undo stack
    data->undo_manager->redo_stack = g_list_delete_link(data->undo_manager->redo_stack, last);
    data->undo_manager->undo_stack = g_list_append(data->undo_manager->undo_stack, action);

    gtk_widget_queue_draw(data->drawing_area);
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
    gtk_window_set_title(GTK_WINDOW(dialog), "Action Log");
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

gboolean undo_manager_can_undo(UndoManager *manager) {
    return manager && manager->undo_stack != NULL;
}

gboolean undo_manager_can_redo(UndoManager *manager) {
    return manager && manager->redo_stack != NULL;
}

void on_undo_clicked(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    undo_manager_undo(data);
}

void on_redo_clicked(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    undo_manager_redo(data);
}

void on_log_clicked(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    show_action_log(data);
}
