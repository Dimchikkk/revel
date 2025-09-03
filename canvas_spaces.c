#include "canvas_spaces.h"
#include "canvas_core.h"
#include "space.h"

void switch_to_space(CanvasData *data, Space *space) {
    if (!space || space == data->current_space) return;

    // Add current space to history
    data->space_history = g_list_append(data->space_history, data->current_space);

    // Switch to new space
    data->current_space = space;

    // NO NEED to copy elements - we'll use space->elements directly
    gtk_widget_queue_draw(data->drawing_area);
}

void go_back_to_parent_space(CanvasData *data) {
    if (!data->current_space || !data->current_space->parent) {
        // Already at root or no parent
        return;
    }

    // Switch to parent space
    Space *parent = data->current_space->parent;
    switch_to_space(data, parent);

    // Remove the last history entry
    if (data->space_history) {
        GList *last = g_list_last(data->space_history);
        data->space_history = g_list_delete_link(data->space_history, last);
    }
}

void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    if (response_id == GTK_RESPONSE_OK) {
        // Get the entry widget from dialog data
        GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "space_name_entry");
        const char *space_name = gtk_editable_get_text(GTK_EDITABLE(entry));

        if (space_name && strlen(space_name) > 0) {
            // Create a new space
            Space *new_space = space_new(space_name, data->current_space);

            // Create a visual representation of the space
            SpaceElement *space_elem = space_element_create(100, 100, 150, 100,
                                                           new_space, data->next_z_index++, data);

            // Add to current space
            data->current_space->elements = g_list_append(data->current_space->elements, (Element*)space_elem);

            gtk_widget_queue_draw(data->drawing_area);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}
