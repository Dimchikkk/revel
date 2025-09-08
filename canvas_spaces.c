#include "canvas_spaces.h"
#include "canvas_core.h"
#include "space.h"
#include "model.h"

void switch_to_space(CanvasData *data, const gchar* space_uuid) {
  g_printerr("switch_to_space not implemented");
  gtk_widget_queue_draw(data->drawing_area);
}

void go_back_to_parent_space(CanvasData *data) {
  g_printerr("go_back_to_parent_space not implemented");
  gtk_widget_queue_draw(data->drawing_area);
}

void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    // Get the entry widget and canvas data from dialog
    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "space_name_entry");
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");

    if (entry && data && data->model) {
      const char *space_name = gtk_editable_get_text(GTK_EDITABLE(entry));

      if (space_name && strlen(space_name) > 0) {
        // Generate a new UUID for the target space
        gchar *target_space_uuid = model_generate_uuid();

        // Create a new space element in the model
        ModelElement *model_element = model_create_space(data->model, 100, 100, 200, 150, target_space_uuid);

        if (model_element) {
          // Update the text with the space name
          model_update_text(data->model, model_element, space_name);

          // Create visual space element
          SpaceElement *space_element = space_element_create(
                                                             model_element->position->x,
                                                             model_element->position->y,
                                                             model_element->position->z,
                                                             model_element->size->width,
                                                             model_element->size->height,
                                                             space_name,
                                                             target_space_uuid,
                                                             data
                                                             );

          if (space_element) {
            // Link model and visual elements
            model_element->visual_element = (Element*)space_element;

            // Update next_z_index
            if (model_element->position->z >= data->next_z_index) {
              data->next_z_index = model_element->position->z + 1;
            }

            gtk_widget_queue_draw(data->drawing_area);
          }

          g_free(target_space_uuid);
        }
      } else {
        g_print("Space name cannot be empty\n");
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}
