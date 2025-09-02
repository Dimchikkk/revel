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

void undo_manager_free(UndoManager *manager) {
    g_list_free_full(manager->undo_stack, (GDestroyNotify)g_free);
    g_list_free_full(manager->redo_stack, (GDestroyNotify)g_free);
    g_list_free_full(manager->action_log, (GDestroyNotify)g_free);

    if (manager->log_window && GTK_IS_WIDGET(manager->log_window)) {
        gtk_window_destroy(GTK_WINDOW(manager->log_window));
    }

    if (manager->log_store) {
        g_object_unref(manager->log_store);
    }

    g_free(manager);
}

static Action* action_new(ActionType type, gpointer data, const char *description) {
    Action *action = g_new0(Action, 1);
    action->type = type;
    action->data = data;
    action->description = g_strdup(description);
    action->timestamp = g_date_time_new_now_local();
    return action;
}

// Add this function to update the log window
static void update_log_window(UndoManager *manager) {
    if (!manager->log_store) return;

    // Clear the current store
    gtk_list_store_clear(manager->log_store);

    // Repopulate with all actions
    GtkTreeIter iter;
    for (GList *l = manager->action_log; l != NULL; l = l->next) {
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

void undo_manager_push_action(UndoManager *manager, ActionType type, gpointer data, const char *description) {
    Action *action = action_new(type, data, description);
    manager->undo_stack = g_list_append(manager->undo_stack, action);
    manager->action_log = g_list_append(manager->action_log, action_new(type, data, description));

    // Update the log window if it's open
    if (manager->log_window && gtk_widget_get_visible(manager->log_window)) {
        update_log_window(manager);
    }

    // Clear redo stack when new action is pushed
    g_list_free_full(manager->redo_stack, (GDestroyNotify)g_free);
    manager->redo_stack = NULL;
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
    return manager->undo_stack != NULL;
}

gboolean undo_manager_can_redo(UndoManager *manager) {
    return manager->redo_stack != NULL;
}

void undo_manager_undo(CanvasData *data) {
    if (!data->undo_manager || !data->undo_manager->undo_stack) return;

    GList *last = g_list_last(data->undo_manager->undo_stack);
    Action *action = (Action*)last->data;

    switch (action->type) {
        case ACTION_CREATE_NOTE:
        case ACTION_CREATE_PAPER_NOTE: {
            Element *element = (Element*)action->data;
            // Hide the element instead of removing it
            element->hidden = TRUE;
            break;
        }
        case ACTION_CREATE_CONNECTION: {
            Connection *conn = (Connection*)action->data;
            // Hide the connection
            conn->hidden = TRUE;
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
        case ACTION_CREATE_PAPER_NOTE: {
            Element *element = (Element*)action->data;
            element->hidden = FALSE;
            break;
        }
        case ACTION_CREATE_CONNECTION: {
            Connection *conn = (Connection*)action->data;
            conn->hidden = FALSE;
            break;
        }
        // Handle other action types...
    }

    // Move from redo to undo stack
    data->undo_manager->redo_stack = g_list_delete_link(data->undo_manager->redo_stack, last);
    data->undo_manager->undo_stack = g_list_append(data->undo_manager->undo_stack, action);

    gtk_widget_queue_draw(data->drawing_area);
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
