#include "canvas_space_select.h"
#include "canvas_spaces.h"
#include "../model.h"
#include <gtk/gtk.h>
#include <string.h>
#include "../undo_manager.h"

// Structure to hold space selection data
typedef struct {
  GList *spaces;  // List of ModelSpaceInfo* (filtered to exclude current space)
  CanvasData *canvas_data;
} SpaceSelectData;

static void free_space_select_data(gpointer data) {
  SpaceSelectData *select_data = (SpaceSelectData*)data;
  if (select_data) {
    g_list_free_full(select_data->spaces, (GDestroyNotify)model_free_space_info);
    g_free(select_data);
  }
}

// Helper function to format date nicely
static gchar* format_date(const gchar *raw_date) {
  if (!raw_date) return g_strdup("");

  // Simple formatting: convert "2025-09-11 19:17:45" to "Sep 11, 2025"
  GDateTime *dt = g_date_time_new_from_iso8601(raw_date, NULL);
  if (!dt) return g_strdup(raw_date);

  gchar *formatted = g_date_time_format(dt, "%b %d, %Y");
  g_date_time_unref(dt);

  return formatted;
}

static void on_space_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
  if (!row) return;

  GtkWindow *dialog = g_object_get_data(G_OBJECT(list), "space_dialog");
  SpaceSelectData *select_data = g_object_get_data(G_OBJECT(dialog), "space_select_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(dialog), "element_uuid");

  if (!select_data || !select_data->spaces) {
    g_printerr("Error: No space data available\n");
    return;
  }

  // FIX: Use GTK's built-in function to get the row index
  gint index = gtk_list_box_row_get_index(row);

  ModelSpaceInfo *space = g_list_nth_data(select_data->spaces, index);

  if (!space || !space->uuid) {
    g_printerr("Error: Invalid space or space UUID\n");
    return;
  }


  // Move the element to the selected space
  if (element_uuid) {
    ModelElement *element = g_hash_table_lookup(select_data->canvas_data->model->elements, element_uuid);
    if (element) {
      undo_manager_remove_actions_for_element(select_data->canvas_data->undo_manager, element);
      model_save_elements(select_data->canvas_data->model);
      int result = move_element_to_space(select_data->canvas_data->model, element, space->uuid);
      if (result) {
        canvas_sync_with_model(select_data->canvas_data);
        gtk_widget_queue_draw(select_data->canvas_data->drawing_area);
      } else {
        g_printerr("Failed to move element\n");
      }
    } else {
      g_printerr("Element not found: %s\n", element_uuid);
    }
  }

  gtk_window_close(dialog);
}

static void on_search_entry_changed(GtkEntry *entry, gpointer user_data) {
  if (!entry || !user_data) return;

  GtkListBox *spaces_list = g_object_get_data(G_OBJECT(entry), "spaces_list");
  GtkWindow *dialog = g_object_get_data(G_OBJECT(entry), "space_dialog");
  if (!spaces_list || !dialog) return;

  const gchar *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Clear previous results
  GtkListBoxRow *row;
  while ((row = gtk_list_box_get_row_at_index(spaces_list, 0)) != NULL) {
    gtk_list_box_remove(spaces_list, GTK_WIDGET(row));
  }

  SpaceSelectData *select_data = g_object_get_data(G_OBJECT(dialog), "space_select_data");
  if (!select_data || !select_data->spaces) return;

  int index = 0;
  for (GList *iter = select_data->spaces; iter != NULL; iter = iter->next, index++) {
    ModelSpaceInfo *space = (ModelSpaceInfo*)iter->data;

    // Filter by search text if provided
    if (search_text && strlen(search_text) > 0) {
      if (!strstr(space->name, search_text) && !strstr(space->uuid, search_text)) {
        continue;
      }
    }

    GtkWidget *row_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(row_widget, 10);
    gtk_widget_set_margin_end(row_widget, 10);
    gtk_widget_set_margin_top(row_widget, 5);
    gtk_widget_set_margin_bottom(row_widget, 5);

    // Space name
    GtkWidget *name_label = gtk_label_new(space->name);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);

    // Format creation date
    gchar *formatted_date = format_date(space->created_at);
    GtkWidget *date_label = gtk_label_new(formatted_date);
    gtk_label_set_xalign(GTK_LABEL(date_label), 0.0);
    gtk_widget_add_css_class(date_label, "dim-label");
    g_free(formatted_date);

    gtk_box_append(GTK_BOX(row_widget), name_label);
    gtk_box_append(GTK_BOX(row_widget), date_label);

    gtk_list_box_append(GTK_LIST_BOX(spaces_list), row_widget);
  }
}

void canvas_show_space_select_dialog(CanvasData *data, const gchar *element_uuid) {
  // Get the window from the drawing area's ancestor
  GtkWidget *window = gtk_widget_get_ancestor(data->drawing_area, GTK_TYPE_WINDOW);
  if (!window) {
    g_printerr("Failed to find parent window for space select dialog\n");
    return;
  }

  GtkWidget *dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Select Destination Space");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_set_margin_top(content_area, 10);
  gtk_widget_set_margin_bottom(content_area, 10);
  gtk_widget_set_margin_start(content_area, 10);
  gtk_widget_set_margin_end(content_area, 10);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append(GTK_BOX(content_area), vbox);

  // Title label
  GtkWidget *title_label = gtk_label_new("Select destination space:");
  gtk_label_set_xalign(GTK_LABEL(title_label), 0.0);
  gtk_box_append(GTK_BOX(vbox), title_label);

  // Search entry
  GtkWidget *search_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search spaces...");
  gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search_entry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
  gtk_box_append(GTK_BOX(vbox), search_entry);

  // Results list
  GtkWidget *scrolled_window = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(scrolled_window, TRUE);
  gtk_widget_set_vexpand(scrolled_window, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  GtkWidget *spaces_list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(spaces_list), GTK_SELECTION_SINGLE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), spaces_list);
  gtk_box_append(GTK_BOX(vbox), scrolled_window);

  // Get all available spaces using model function
  GList *all_spaces = NULL;
  if (!model_get_all_spaces(data->model, &all_spaces)) {
    g_printerr("Failed to get spaces list\n");
    gtk_window_close(GTK_WINDOW(dialog));
    return;
  }

  // Filter out current space
  GList *filtered_spaces = NULL;
  for (GList *iter = all_spaces; iter != NULL; iter = iter->next) {
    ModelSpaceInfo *space = (ModelSpaceInfo*)iter->data;
    if (g_strcmp0(space->uuid, data->model->current_space_uuid) != 0) {
      filtered_spaces = g_list_append(filtered_spaces, space);
    } else {
      // Free the current space since we don't want to show it
      model_free_space_info(space);
    }
  }
  g_list_free(all_spaces);

  // Store the filtered spaces in dialog data
  SpaceSelectData *select_data = g_new0(SpaceSelectData, 1);
  select_data->spaces = filtered_spaces;
  select_data->canvas_data = data;

  g_object_set_data_full(G_OBJECT(dialog), "space_select_data",
                         select_data, free_space_select_data);

  // Store element UUID separately
  g_object_set_data_full(G_OBJECT(dialog), "element_uuid", g_strdup(element_uuid), g_free);

  // Connect signals
  g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_entry_changed), data);
  g_signal_connect(spaces_list, "row-activated", G_CALLBACK(on_space_selected), NULL);

  // Store references
  g_object_set_data(G_OBJECT(search_entry), "spaces_list", spaces_list);
  g_object_set_data(G_OBJECT(search_entry), "space_dialog", dialog);
  g_object_set_data(G_OBJECT(spaces_list), "space_dialog", dialog);
  g_object_set_data(G_OBJECT(dialog), "canvas_data", data);

  // Populate the list with filtered spaces
  on_search_entry_changed(GTK_ENTRY(search_entry), data);

  gtk_widget_show(dialog);
}
