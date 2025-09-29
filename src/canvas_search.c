#include "canvas_search.h"
#include "canvas_spaces.h"
#include "model.h"
#include <gtk/gtk.h>
#include <string.h>

// Structure to hold all search data
typedef struct {
    GList *search_results;  // List of ModelSearchResult*
    CanvasData *canvas_data;
} SearchDialogData;

static void free_search_dialog_data(gpointer data) {
    SearchDialogData *dialog_data = (SearchDialogData*)data;
    if (dialog_data) {
        g_list_free_full(dialog_data->search_results, (GDestroyNotify)model_free_search_result);
        g_free(dialog_data);
    }
}

// Helper function to truncate text with ellipsis
static gchar* truncate_text_with_ellipsis(const gchar *text, gsize max_length) {
    if (!text) return g_strdup("");

    gsize text_length = strlen(text);
    if (text_length <= max_length) {
        return g_strdup(text);
    }

    // Find a good breaking point (space or punctuation)
    gsize break_point = max_length - 3; // Reserve 3 chars for ellipsis
    while (break_point > 0 &&
           text[break_point] != ' ' &&
           text[break_point] != '.' &&
           text[break_point] != ',' &&
           text[break_point] != ';') {
        break_point--;
    }

    // If no good break point found, just truncate at max_length
    if (break_point < max_length / 2) {
        break_point = max_length - 3;
    }

    gchar *truncated = g_strdup_printf("%.*s...", (int)break_point, text);
    return truncated;
}

static void on_search_entry_changed(GtkEntry *entry, gpointer user_data) {
    if (!entry || !user_data) return;

    CanvasData *data = (CanvasData*)user_data;
    GtkListBox *results_list = g_object_get_data(G_OBJECT(entry), "results_list");
    GtkWindow *dialog = g_object_get_data(G_OBJECT(entry), "search_dialog");
    if (!results_list || !dialog) return;

    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!text) return;

    // Clear previous results
    GtkListBoxRow *row;
    while ((row = gtk_list_box_get_row_at_index(results_list, 0)) != NULL) {
        gtk_list_box_remove(results_list, GTK_WIDGET(row));
    }

    // Clear previous search data
    SearchDialogData *old_data = g_object_get_data(G_OBJECT(dialog), "search_dialog_data");
    if (old_data) {
        g_list_free_full(old_data->search_results, (GDestroyNotify)model_free_search_result);
        old_data->search_results = NULL;
    }

    if (strlen(text) < 3) return;

    GList *search_results = NULL;
    model_search_elements(data->model, text, &search_results);

    // Store the search results in dialog data
    SearchDialogData *dialog_data = g_new0(SearchDialogData, 1);
    dialog_data->search_results = search_results;
    dialog_data->canvas_data = data;

    g_object_set_data_full(G_OBJECT(dialog), "search_dialog_data",
                          dialog_data, free_search_dialog_data);

    int index = 0;
    for (GList *iter = search_results; iter != NULL; iter = iter->next, index++) {
        ModelSearchResult *result = (ModelSearchResult*)iter->data;

        GtkWidget *row_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_set_margin_start(row_widget, 10);
        gtk_widget_set_margin_end(row_widget, 10);
        gtk_widget_set_margin_top(row_widget, 5);
        gtk_widget_set_margin_bottom(row_widget, 5);

        // Truncate long text to 200 characters
        gchar *display_text = truncate_text_with_ellipsis(result->text_content, 200);

        GtkWidget *text_label = gtk_label_new(display_text);
        gtk_label_set_wrap(GTK_LABEL(text_label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(text_label), PANGO_WRAP_WORD);
        gtk_label_set_max_width_chars(GTK_LABEL(text_label), 50);
        gtk_label_set_ellipsize(GTK_LABEL(text_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(text_label), 0.0);

        g_free(display_text); // Free the truncated text

        GtkWidget *space_label = gtk_label_new(result->space_name);
        gtk_label_set_xalign(GTK_LABEL(space_label), 0.0);
        gtk_widget_add_css_class(space_label, "dim-label");

        gtk_box_append(GTK_BOX(row_widget), text_label);
        gtk_box_append(GTK_BOX(row_widget), space_label);

        gtk_list_box_append(results_list, row_widget);
    }
}

static void on_result_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;

    GtkWindow *dialog = g_object_get_data(G_OBJECT(list), "search_dialog");
    SearchDialogData *dialog_data = g_object_get_data(G_OBJECT(dialog), "search_dialog_data");

    if (!dialog_data || !dialog_data->search_results) {
        g_printerr("Error: No search data available\n");
        return;
    }

    // Get the row index using GTK's built-in function
    gint index = gtk_list_box_row_get_index(row);

    // Get the result using the index
    ModelSearchResult *result = g_list_nth_data(dialog_data->search_results, index);

    if (!result || !result->space_uuid) {
        g_printerr("Error: Invalid result or space UUID\n");
        return;
    }

    if (g_strcmp0(result->space_uuid, dialog_data->canvas_data->model->current_space_uuid) != 0) {
        model_save_elements(dialog_data->canvas_data->model);
        switch_to_space(dialog_data->canvas_data, result->space_uuid);
    }

    gtk_window_close(dialog);
}

void canvas_show_search_dialog(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    // Get the window from the drawing area's ancestor
    GtkWidget *window = gtk_widget_get_ancestor(data->drawing_area, GTK_TYPE_WINDOW);
    if (!window) {
        g_printerr("Failed to find parent window for search dialog\n");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Search Elements");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(content_area, 10);
    gtk_widget_set_margin_bottom(content_area, 10);
    gtk_widget_set_margin_start(content_area, 10);
    gtk_widget_set_margin_end(content_area, 10);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(content_area), vbox);

    // Search entry
    GtkWidget *search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Type to search elements...");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search_entry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
    gtk_box_append(GTK_BOX(vbox), search_entry);

    // Results list
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *results_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(results_list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), results_list);
    gtk_box_append(GTK_BOX(vbox), scrolled_window);

    // Connect signals
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_entry_changed), data);
    g_signal_connect(results_list, "row-activated", G_CALLBACK(on_result_selected), data);

    // Store references
    g_object_set_data(G_OBJECT(search_entry), "results_list", results_list);
    g_object_set_data(G_OBJECT(search_entry), "search_dialog", dialog);
    g_object_set_data(G_OBJECT(results_list), "search_dialog", dialog);
    g_object_set_data(G_OBJECT(dialog), "canvas_data", data);

    gtk_widget_show(dialog);
}
