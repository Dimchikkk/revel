#include "canvas_spaces.h"
#include "canvas_core.h"
#include "space.h"

void switch_to_space(CanvasData *data, const gchar* space_uuid) {
    // TODO: not implemented
    gtk_widget_queue_draw(data->drawing_area);
}

void go_back_to_parent_space(CanvasData *data) {
  // TODO: not_implemented
}

void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    if (response_id == GTK_RESPONSE_OK) {
        // Get the entry widget from dialog data
        GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "space_name_entry");
        const char *space_name = gtk_editable_get_text(GTK_EDITABLE(entry));

        if (space_name && strlen(space_name) > 0) {
            // TODO: create new space element
            gtk_widget_queue_draw(data->drawing_area);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}
