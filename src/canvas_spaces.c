#include "canvas_spaces.h"
#include "canvas_core.h"
#include "canvas_placement.h"
#include "element.h"
#include "media_note.h"
#include "space.h"
#include "model.h"
#include "undo_manager.h"

static void capture_audio_playback_states(CanvasData *data) {
  if (!data || !data->audio_playback_states || !data->model) {
    return;
  }

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, data->model->elements);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement*)value;
    if (!element || !element->uuid || !element->visual_element) {
      continue;
    }

    if (element->type && element->type->type == ELEMENT_MEDIA_FILE) {
      MediaNote *media_note = (MediaNote*)element->visual_element;
      if (media_note && media_note->media_type == MEDIA_TYPE_AUDIO) {
        if (media_note->media_playing) {
          AudioPlaybackState *state = g_new0(AudioPlaybackState, 1);
          state->element = (Element*)media_note;
          state->playing = TRUE;
          g_hash_table_replace(data->audio_playback_states,
                               g_strdup(element->uuid),
                               state);
        } else {
          g_hash_table_remove(data->audio_playback_states, element->uuid);
        }
      }
    }
  }
}

void switch_to_space(CanvasData *data, const gchar* space_uuid) {
  if (!space_uuid) {
    return;
  }

  capture_audio_playback_states(data);

  undo_manager_reset(data->undo_manager);

  // Clear selection when switching spaces to avoid stale pointers
  if (data->selected_elements) {
    g_list_free(data->selected_elements);
    data->selected_elements = NULL;
  }

  // Clear copied elements when switching spaces
  if (data->copied_elements) {
    g_list_free(data->copied_elements);
    data->copied_elements = NULL;
  }

  if (data->model->current_space_uuid) {
    g_free(data->model->current_space_uuid);
  }
  data->model->current_space_uuid = g_strdup(space_uuid);

  model_load_space_settings(data->model, space_uuid);
  model_load_space(data->model);

  // Clear quadtree when switching spaces since elements are freed
  if (data->quadtree) {
    quadtree_clear(data->quadtree);
  }

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
