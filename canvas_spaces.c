#include "canvas_spaces.h"
#include "canvas_core.h"
#include "canvas_placement.h"
#include "element.h"
#include "space.h"
#include "model.h"
#include "undo_manager.h"

void switch_to_space(CanvasData *data, const gchar* space_uuid) {
  if (!space_uuid) {
    return;
  }

  undo_manager_reset(data->undo_manager);

  if (data->model->current_space_uuid) {
    g_free(data->model->current_space_uuid);
  }
  data->model->current_space_uuid = g_strdup(space_uuid);

  model_load_space_settings(data->model, space_uuid);
  model_load_space(data->model);

  // Set flag to enable animations for space loading
  data->is_loading_space = TRUE;
  canvas_sync_with_model(data);
  data->is_loading_space = FALSE;

  if (data->drawing_area && GTK_IS_WIDGET(data->drawing_area)) {
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void go_back_to_parent_space(CanvasData *data) {
  model_save_elements(data->model);
  gchar *parent_space_name = NULL;
  model_get_space_parent_uuid(data->model, data->model->current_space_uuid, &parent_space_name);
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
        ElementSize size = {
          .width = 200,
          .height = 150,
        };

        // Find smart placement position
        int smart_x, smart_y;
        canvas_find_empty_position(data, size.width, size.height, &smart_x, &smart_y);

        ElementPosition position = {
          .x = smart_x,
          .y = smart_y,
          .z = data->next_z_index++,
        };
        ElementColor bg_color = {
          .r = 0.8,
          .g = 0.8,
          .b = 1.0,
          .a = 1.0,
        };
        ElementColor text_color = {
          .r = 0.1,
          .g = 0.1,
          .b = 0.1,
          .a = 1.0,
        };
        ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
        ElementConnection connection = {
          .from_element_uuid = NULL,
          .to_element_uuid = NULL,
          .from_point = -1,
          .to_point = -1,
        };
        ElementDrawing drawing = {
          .drawing_points = NULL,
          .stroke_width = 0,
        };
        ElementText text = {
          .text = g_strdup(space_name),
          .text_color = text_color,
          .font_description = g_strdup("Ubuntu Mono Bold 16"),
        };
        ElementConfig config = {
          .type = ELEMENT_SPACE,
          .bg_color = bg_color,
          .position = position,
          .size = size,
          .media = media,
          .drawing = drawing,
          .connection = connection,
          .text = text,
        };

        ModelElement *model_element = model_create_element(data->model, config);
        model_element->visual_element = create_visual_element(model_element, data);
        undo_manager_push_create_action(data->undo_manager, model_element);
        gtk_widget_queue_draw(data->drawing_area);
      } else {
        g_print("Space name cannot be empty\n");
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}
