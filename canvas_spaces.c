#include "canvas_spaces.h"
#include "canvas_core.h"
#include "element.h"
#include "space.h"
#include "model.h"

void switch_to_space(CanvasData *data, const gchar* space_uuid) {
  if (!space_uuid) {
    return;
  }

  if (data->model->current_space_uuid) {
    g_free(data->model->current_space_uuid);
  }
  data->model->current_space_uuid = g_strdup(space_uuid);;

  model_load_space(data->model);
  canvas_sync_with_model(data);

  gtk_widget_queue_draw(data->drawing_area);
}

void go_back_to_parent_space(CanvasData *data) {
  model_save_elements(data->model);
  gchar *parent_space_name = NULL;
  model_get_parent_id(data->model, &parent_space_name);
  switch_to_space(data, parent_space_name);
}

void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    // Get the entry widget and canvas data from dialog
    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "space_name_entry");
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");

    if (entry && data && data->model) {
      const char *space_name = gtk_editable_get_text(GTK_EDITABLE(entry));

      if (space_name && strlen(space_name) > 0) {
        // Create a new space element in the model
        ElementPosition position = {
          .x = 100,
          .y = 100,
          .z = data->next_z_index++,
        };
        ElementColor bg_color = {
          .r = 0.8,
          .g = 0.8,
          .b = 1.0,
          .a = 1.0,
        };
        ElementSize size = {
          .width = 200,
          .height = 150,
        };
        ModelElement *model_element = model_create_element(data->model, ELEMENT_SPACE, bg_color, position, size, NULL, 0, 0, NULL, -1, -1, space_name);
        SpaceElement *space_element = space_element_create(position, bg_color, size, space_name, data);

        // Link model and visual elements
        model_element->visual_element = (Element*)space_element;

        // Update next_z_index
        if (model_element->position->z >= data->next_z_index) {
          data->next_z_index = model_element->position->z + 1;
        }

        gtk_widget_queue_draw(data->drawing_area);
      } else {
        g_print("Space name cannot be empty\n");
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}
