#include "model.h"
#include "database.h"
#include "elements/element.h"
#include "elements/note.h"
#include "elements/connection.h"
#include <string.h>
#include <stdio.h>
#include "canvas/canvas_core.h"
#include <glib.h>

void model_free(Model *model) {
  if (!model) return;

  g_hash_table_destroy(model->elements);
  g_hash_table_destroy(model->types);
  g_hash_table_destroy(model->texts);
  g_hash_table_destroy(model->positions);
  g_hash_table_destroy(model->sizes);
  g_hash_table_destroy(model->colors);
  g_hash_table_destroy(model->images);
  g_hash_table_destroy(model->videos);
  g_free(model->current_space_uuid);
  g_free(model->current_space_background_color);
  g_free(model->current_space_name);
  g_free(model);
}

void model_video_free(ModelVideo *video) {
  if (!video) return;
  if (video->thumbnail_data) g_free(video->thumbnail_data);
  if (video->video_data) g_free(video->video_data);
  g_free(video);
}

void model_audio_free(ModelAudio *audio) {
  if (!audio) return;
  if (audio->audio_data) g_free(audio->audio_data);
  g_free(audio);
}

void model_image_free(ModelImage *image) {
  if (!image) return;
  if (image->image_data) {
    g_free(image->image_data);
  }
  g_free(image);
}

void model_type_free(ModelType *type) {
  if (!type) return;
  g_free(type);
}

void model_text_free(ModelText *text) {
  if (!text) return;
  g_free(text->text);
  g_free(text->font_description);
  g_free(text->alignment);
  g_free(text);
}

void model_position_free(ModelPosition *position) {
  if (!position) return;
  g_free(position);
}

void model_size_free(ModelSize *size) {
  if (!size) return;
  g_free(size);
}

void model_color_free(ModelColor *color) {
  if (!color) return;
  g_free(color);
}

void model_element_free(ModelElement *element) {
  if (!element) return;

  g_free(element->uuid);
  g_free(element->from_element_uuid);
  g_free(element->to_element_uuid);
  g_free(element->target_space_uuid);
  g_free(element->description);
  g_free(element->created_at);
  g_free(element->stroke_color);

  if (element->drawing_points != NULL) {
    g_array_free(element->drawing_points, TRUE);
  }

  // Important: Don't free shared resources here!
  // They are managed by the respective hash tables and will be
  // automatically freed when the hash tables are destroyed
  // or when their reference counts drop to zero

  g_free(element);
}


Model* model_new_with_file(const char *db_filename) {
  Model *model = g_new0(Model, 1);
  model->elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)model_element_free);
  model->types = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->texts = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->positions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->sizes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->colors = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->images = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->videos = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->audios = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->db = NULL;

  model->current_space_background_color = NULL;
  model->current_space_name = NULL;
  model->current_space_show_grid = FALSE;
  model->current_space_grid_color = (GdkRGBA){0.15, 0.15, 0.20, 0.4};

  if (!database_init(&model->db, db_filename)) {
    g_free(model);
    return NULL;
  }

  // Get current space UUID
  char *space_uuid = NULL;
  database_get_current_space_uuid(model->db, &space_uuid);
  model->current_space_uuid = space_uuid;

  // Load the space
  model_load_space(model);
  model_load_space_settings(model, model->current_space_uuid);

  return model;
}

void model_load_space(Model *model) {
  if (!model || !model->current_space_uuid) return;

  // Clear current elements and shared resources
  g_hash_table_remove_all(model->elements);
  g_hash_table_remove_all(model->types);
  g_hash_table_remove_all(model->texts);
  g_hash_table_remove_all(model->positions);
  g_hash_table_remove_all(model->sizes);
  g_hash_table_remove_all(model->colors);

  // Use database_load_space to populate the model
  database_load_space(model->db, model);
}

void model_load_space_settings(Model *model, const char *space_uuid) {
  if (!model || !space_uuid) return;

  // Load space name
  g_free(model->current_space_name);
  database_get_space_name(model->db, space_uuid, &model->current_space_name);

  // Load background color
  g_free(model->current_space_background_color);
  database_get_space_background(model->db, space_uuid, &model->current_space_background_color);

  // Load grid settings
  int grid_enabled = 0;
  char *grid_color_str = NULL;
  if (database_get_space_grid_settings(model->db, space_uuid, &grid_enabled, &grid_color_str)) {
    model->current_space_show_grid = grid_enabled;
    if (grid_color_str) {
      gdk_rgba_parse(&model->current_space_grid_color, grid_color_str);
      g_free(grid_color_str);
    }
  }
}

int model_get_space_name(Model *model, const char *space_uuid, char **space_name) {
  if (!model || !model->db || !space_uuid) {
    return 0;
  }

  return database_get_space_name(model->db, space_uuid, space_name);
}


gchar* model_generate_uuid(void) {
  uuid_t uuid;
  uuid_generate(uuid);

  char *buf = g_malloc(37); // 36 characters + null terminator
  uuid_unparse(uuid, buf);
  return buf;
}

ModelElement* model_create_element(Model *model, ElementConfig config) {
  if (model == NULL) {
    g_printerr("Error: model is NULL in model_create_element\n");
    return NULL;
  }

  // Create a new ModelElement
  ModelElement *element = g_new0(ModelElement, 1);
  element->uuid = model_generate_uuid();
  element->state = MODEL_STATE_NEW;

  element->space_uuid = g_strdup(model->current_space_uuid);

  // Create type reference
  ModelType *model_type = g_new0(ModelType, 1);
  model_type->id = -1;  // Temporary ID until saved to database
  model_type->type = config.type;
  model_type->ref_count = 1;
  element->type = model_type;

  // Create position reference
  ModelPosition *position = g_new0(ModelPosition, 1);
  position->id = -1;  // Temporary ID until saved to database
  position->x = config.position.x;
  position->y = config.position.y;
  position->z = config.position.z;
  position->ref_count = 1;
  element->position = position;

  // Create size reference
  ModelSize *size = g_new0(ModelSize, 1);
  size->id = -1;  // Temporary ID until saved to database
  size->width = config.size.width;
  size->height = config.size.height;
  size->ref_count = 1;
  element->size = size;

  // Create text reference if text is provided
  if (config.text.text != NULL) {
    ModelText *model_text = g_new0(ModelText, 1);
    model_text->id = -1;  // Temporary ID until saved to database
    model_text->text = g_strdup(config.text.text);
    model_text->font_description = g_strdup(config.text.font_description);
    model_text->r = config.text.text_color.r;
    model_text->g = config.text.text_color.g;
    model_text->b = config.text.text_color.b;
    model_text->a = config.text.text_color.a;
    model_text->ref_count = 1;

    // Use alignment from config if provided, otherwise set default based on element type
    if (config.text.alignment) {
      model_text->alignment = g_strdup(config.text.alignment);
    } else if (config.type == ELEMENT_PAPER_NOTE) {
      model_text->alignment = g_strdup("top-left");
    } else {
      model_text->alignment = g_strdup("center");
    }

    element->text = model_text;
  }

  // Always create background color, even if transparent
  ModelColor *color = g_new0(ModelColor, 1);
  color->id = -1;  // Temporary ID until saved to database
  color->r = config.bg_color.r;
  color->g = config.bg_color.g;
  color->b = config.bg_color.b;
  color->a = config.bg_color.a;
  color->ref_count = 1;
  element->bg_color = color;

  // Set connection properties
  if (config.connection.from_element_uuid) {
    element->from_element_uuid = g_strdup(config.connection.from_element_uuid);
  }
  if (config.connection.to_element_uuid) {
    element->to_element_uuid = g_strdup(config.connection.to_element_uuid);
  }
  if (config.connection.from_point) {
    element->from_point = config.connection.from_point;
  }
  if (config.connection.to_point) {
    element->to_point = config.connection.to_point;
  }

  if (config.media.type == MEDIA_TYPE_IMAGE && config.media.image_data && config.media.image_size > 0) {
    ModelImage *model_image = g_new0(ModelImage, 1);
    model_image->id = -1;
    model_image->image_data = g_malloc(config.media.image_size);
    memcpy(model_image->image_data, config.media.image_data, config.media.image_size);
    model_image->image_size = config.media.image_size;
    model_image->ref_count = 1;
    element->image = model_image;
  }

  if (config.media.type == MEDIA_TYPE_VIDEO && config.media.image_data && config.media.image_size > 0) {
    ModelVideo *model_video = g_new0(ModelVideo, 1);
    model_video->id = -1;
    model_video->thumbnail_data = g_malloc(config.media.image_size);
    memcpy(model_video->thumbnail_data, config.media.image_data, config.media.image_size);
    model_video->thumbnail_size = config.media.image_size;

    // Video data might be NULL if not loaded yet (lazy loading)
    if (config.media.video_data && config.media.video_size > 0) {
      model_video->video_data = g_malloc(config.media.video_size);
      memcpy(model_video->video_data, config.media.video_data, config.media.video_size);
      model_video->video_size = config.media.video_size;
      model_video->is_loaded = TRUE;
    } else {
      model_video->video_data = NULL;
      model_video->video_size = 0;
      model_video->is_loaded = FALSE;
    }

    model_video->duration = config.media.duration;
    model_video->ref_count = 1;
    element->video = model_video;
  }

  if (config.media.type == MEDIA_TYPE_AUDIO && config.media.video_data && config.media.video_size > 0) {
    ModelAudio *model_audio = g_new0(ModelAudio, 1);
    model_audio->id = -1;

    // Audio data might be NULL if not loaded yet (lazy loading)
    if (config.media.video_data && config.media.video_size > 0) {
      model_audio->audio_data = g_malloc(config.media.video_size);
      memcpy(model_audio->audio_data, config.media.video_data, config.media.video_size);
      model_audio->audio_size = config.media.video_size;
      model_audio->is_loaded = TRUE;
    } else {
      model_audio->audio_data = NULL;
      model_audio->audio_size = 0;
      model_audio->is_loaded = FALSE;
    }

    model_audio->duration = config.media.duration;
    model_audio->ref_count = 1;
    element->audio = model_audio;

    // Also store the thumbnail as an image
    if (config.media.image_data && config.media.image_size > 0) {
      ModelImage *model_image = g_new0(ModelImage, 1);
      model_image->id = -1;
      model_image->image_data = g_malloc(config.media.image_size);
      memcpy(model_image->image_data, config.media.image_data, config.media.image_size);
      model_image->image_size = config.media.image_size;
      model_image->ref_count = 1;
      element->image = model_image;
    }
  }

  if (config.drawing.drawing_points != NULL) {
    element->drawing_points = g_array_copy((GArray*)config.drawing.drawing_points);
  } else {
    element->drawing_points = NULL;
  }

  element->stroke_width = config.drawing.stroke_width != 0 ? config.drawing.stroke_width : config.shape.stroke_width;
  element->shape_type = config.shape.shape_type;
  element->filled = config.shape.filled;
  element->stroke_style = config.shape.stroke_style;
  element->fill_style = config.shape.fill_style;

  // Convert stroke color from ElementColor to hex string
  if (config.shape.stroke_color.a > 0.0) {
    element->stroke_color = g_strdup_printf("#%02x%02x%02x%02x",
      (int)(config.shape.stroke_color.r * 255),
      (int)(config.shape.stroke_color.g * 255),
      (int)(config.shape.stroke_color.b * 255),
      (int)(config.shape.stroke_color.a * 255));
  } else {
    element->stroke_color = NULL;
  }

  if (config.type == ELEMENT_CONNECTION) {
    element->arrowhead_type = config.connection.arrowhead_type;
    element->connection_type = config.connection.connection_type;

    if (element->arrowhead_type != ARROWHEAD_NONE &&
        element->arrowhead_type != ARROWHEAD_SINGLE &&
        element->arrowhead_type != ARROWHEAD_DOUBLE) {
      element->arrowhead_type = ARROWHEAD_SINGLE;
    }

    if (element->connection_type != CONNECTION_TYPE_PARALLEL &&
        element->connection_type != CONNECTION_TYPE_STRAIGHT) {
      element->connection_type = CONNECTION_TYPE_PARALLEL;
    }
  }

  // Set rotation from config
  element->rotation_degrees = config.rotation_degrees;

  g_hash_table_insert(model->elements, g_strdup(element->uuid), element);

  return element;
}

ModelElement* model_create_element_from_visual(Model *model, Element *element) {
  if (!model || !element) {
    return NULL;
  }

  ElementConfig config = {0};
  config.type = element->type;
  config.position.x = element->x;
  config.position.y = element->y;
  config.position.z = element->z;
  config.size.width = element->width;
  config.size.height = element->height;
  config.bg_color.r = element->bg_r;
  config.bg_color.g = element->bg_g;
  config.bg_color.b = element->bg_b;
  config.bg_color.a = element->bg_a;

  if (element->type == ELEMENT_SHAPE) {
    Shape *shape = (Shape*)element;
    config.text.text = shape->text;
    config.text.text_color.r = shape->text_r;
    config.text.text_color.g = shape->text_g;
    config.text.text_color.b = shape->text_b;
    config.text.text_color.a = shape->text_a;
    config.text.font_description = shape->font_description;
    config.shape.shape_type = shape->shape_type;
    config.shape.stroke_width = shape->stroke_width;
    config.shape.filled = shape->filled;
    config.shape.stroke_style = shape->stroke_style;
    config.shape.fill_style = shape->fill_style;
    config.shape.stroke_color.r = shape->stroke_r;
    config.shape.stroke_color.g = shape->stroke_g;
    config.shape.stroke_color.b = shape->stroke_b;
    config.shape.stroke_color.a = shape->stroke_a;
  }

  return model_create_element(model, config);
}

int model_update_text(Model *model, ModelElement *element, const char *text) {
  if (!model || !element || !text) {
    return 0;
  }

  // If element has no text reference, create one
  if (!element->text) {
    element->text = g_new0(ModelText, 1);
    element->text->text = g_strdup(text);
    element->text->ref_count = 1;

    // Set default alignment based on element type
    if (element->type && element->type->type == ELEMENT_PAPER_NOTE) {
      element->text->alignment = g_strdup("top-left");
    } else {
      element->text->alignment = g_strdup("center");
    }

    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  // If text content changed, update it
  if (!element->text->text || strcmp(element->text->text, text) != 0) {
    g_free(element->text->text);
    element->text->text = g_strdup(text);
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_text_color(Model *model, ModelElement *element, double r, double g, double b, double a) {
  if (!model || !element) {
    return 0;
  }

  // If color values changed, update them
  if (fabs(element->text->r - r) > 1e-9 ||
      fabs(element->text->g - g) > 1e-9 ||
      fabs(element->text->b - b) > 1e-9 ||
      fabs(element->text->a - a) > 1e-9) {
    element->text->r = r;
    element->text->g = g;
    element->text->b = b;
    element->text->a = a;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_font(Model *model, ModelElement *element, const char *font_description) {
  if (!model || !element || !font_description) {
    return 0;
  }

  // If font description changed, update it
  if (!element->text->font_description ||
      strcmp(element->text->font_description, font_description) != 0) {
    g_free(element->text->font_description);
    element->text->font_description = g_strdup(font_description);
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_text_alignment(Model *model, ModelElement *element, const char *alignment) {
  if (!model || !element || !alignment) {
    return 0;
  }

  // If element has no text reference, cannot set alignment
  if (!element->text) {
    return 0;
  }

  // If alignment changed, update it
  if (!element->text->alignment ||
      strcmp(element->text->alignment, alignment) != 0) {
    g_free(element->text->alignment);
    element->text->alignment = g_strdup(alignment);
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_strikethrough(Model *model, ModelElement *element, gboolean strikethrough) {
  if (!model || !element) {
    return 0;
  }

  // If element has no text reference, cannot set strikethrough
  if (!element->text) {
    return 0;
  }

  // If strikethrough changed, update it
  if (element->text->strikethrough != strikethrough) {
    element->text->strikethrough = strikethrough;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_color(Model *model, ModelElement *element, double r, double g, double b, double a) {
  if (!model || !element || !element->bg_color) return 0;

  ModelColor *color = element->bg_color;

  color->r = r;
  color->g = g;
  color->b = b;
  color->a = a;

  if (element->state != MODEL_STATE_NEW) {
    element->state = MODEL_STATE_UPDATED;
  }

  return 1;
}

int model_update_position(Model *model, ModelElement *element, int x, int y, int z) {
  if (!model || !element) {
    return 0;
  }

  // If element has no position reference, create one
  if (!element->position) {
    element->position = g_new0(ModelPosition, 1);
    element->position->x = x;
    element->position->y = y;
    element->position->z = z;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  // If position values changed, update them
  if (element->position->x != x || element->position->y != y || element->position->z != z) {
    element->position->x = x;
    element->position->y = y;
    element->position->z = z;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_size(Model *model, ModelElement *element, int width, int height) {
  if (!model || !element) {
    return 0;
  }

  // If element has no size reference, create one
  if (!element->size) {
    element->size = g_new0(ModelSize, 1);
    element->size->width = width;
    element->size->height = height;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  // If size values changed, update them
  if (element->size->width != width || element->size->height != height) {
    element->size->width = width;
    element->size->height = height;
    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_update_locked(Model *model, ModelElement *element, gboolean locked) {
  if (!model || !element) {
    return 0;
  }

  // If locked value changed, update it
  if (element->locked != locked) {
    element->locked = locked;

    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
  }

  return 1;
}

int model_update_rotation(Model *model, ModelElement *element, double rotation_degrees) {
  if (!model || !element) {
    return 0;
  }

  // Normalize rotation to 0-360 range
  while (rotation_degrees < 0) rotation_degrees += 360.0;
  while (rotation_degrees >= 360.0) rotation_degrees -= 360.0;

  // If rotation value changed, update it
  if (element->rotation_degrees != rotation_degrees) {
    element->rotation_degrees = rotation_degrees;

    // Update visual element if it exists
    if (element->visual_element) {
      element->visual_element->rotation_degrees = rotation_degrees;
    }

    if (element->state != MODEL_STATE_NEW) {
      element->state = MODEL_STATE_UPDATED;
    }
    return 1;
  }

  return 0; // No change needed
}

int model_delete_element(Model *model, ModelElement *element) {
  // If element is already deleted, do nothing
  if (!model || !element) {
    return 0;
  }

  if (element->state == MODEL_STATE_DELETED) {
    return 0;
  }

  // Mark element as deleted
  element->state = MODEL_STATE_DELETED;

  // If this is NOT a connection element, find and mark any connections that reference it
  if (element->type->type != ELEMENT_CONNECTION) {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, model->elements);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      ModelElement *current_element = (ModelElement*)value;

      // Only check connection elements that aren't already marked for deletion
      if (current_element->type->type == ELEMENT_CONNECTION &&
          current_element->state != MODEL_STATE_DELETED) {

        // Check if this connection references the element being deleted
        if ((current_element->from_element_uuid && element->uuid &&
             strcmp(current_element->from_element_uuid, element->uuid) == 0) ||
            (current_element->to_element_uuid && element->uuid &&
             strcmp(current_element->to_element_uuid, element->uuid) == 0)) {

          // Mark this connection for deletion too
          model_delete_element(model, current_element);
        }
      }
    }
  }

  return 1;
}

ModelElement* model_element_fork(Model *model, ModelElement *element) {
  if (!model || !element || !element->type) {
    return NULL;
  }

  if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_DELETED) {
    return NULL;
  }

  ElementPosition position = {
    .x = element->position->x,
    .y = element->position->y,
    .z = element->position->z,
  };
  ElementColor bg_color = {
    .r = element->bg_color->r,
    .g = element->bg_color->g,
    .b = element->bg_color->b,
    .a = element->bg_color->a,
  };
  ElementColor text_color = {0};
  if (element->text) {
    text_color.r = element->text->r;
    text_color.g = element->text->g;
    text_color.b = element->text->b;
    text_color.a = element->text->a;
  }
  ElementSize size = {
    .width = element->size->width,
    .height = element->size->height,
  };
  MediaType media_type = MEDIA_TYPE_NONE;
  if (element->audio) {
    media_type = MEDIA_TYPE_AUDIO;
  } else if (element->video) {
    media_type = MEDIA_TYPE_VIDEO;
  } else if (element->image) {
    media_type = MEDIA_TYPE_IMAGE;
  }
  ElementMedia media = {
    .type = media_type,
    .image_data = element->image ? element->image->image_data :
                  (element->video ? element->video->thumbnail_data : NULL),
    .image_size = element->image ? element->image->image_size :
                  (element->video ? element->video->thumbnail_size : 0),
    .video_data = element->video ? element->video->video_data :
                  (element->audio ? element->audio->audio_data : NULL),
    .video_size = element->video ? element->video->video_size :
                  (element->audio ? element->audio->audio_size : 0),
    .duration = element->video ? element->video->duration :
                (element->audio ? element->audio->duration : 0),
  };

  ElementConnection connection = {
    .from_element_uuid = element->from_element_uuid,
    .to_element_uuid = element->to_element_uuid,
    .from_point = element->from_point,
    .to_point = element->to_point,
    .connection_type = element->connection_type,
    .arrowhead_type = element->arrowhead_type,
  };
  // Copy drawing points if they exist (for LINE/ARROW/BEZIER shapes)
  GArray *copied_drawing_points = NULL;
  if (element->drawing_points && element->drawing_points->len > 0) {
    copied_drawing_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), element->drawing_points->len);
    g_array_append_vals(copied_drawing_points, element->drawing_points->data, element->drawing_points->len);
  }

  ElementDrawing drawing = {
    .drawing_points = copied_drawing_points,
    .stroke_width = element->stroke_width,
  };
  ElementText text = {
    .text = element->text ? element->text->text : "",
    .text_color = text_color,
    .font_description = element->text ? element->text->font_description : NULL,
    .strikethrough = element->text ? element->text->strikethrough : FALSE,
    .alignment = element->text ? element->text->alignment : NULL,
  };
  ElementColor stroke_color = {0};
  if (element->stroke_color) {
    // Parse hex color back to ElementColor
    unsigned int r, g, b, a;
    if (sscanf(element->stroke_color, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
      stroke_color.r = r / 255.0;
      stroke_color.g = g / 255.0;
      stroke_color.b = b / 255.0;
      stroke_color.a = a / 255.0;
    }
  }

  ElementShape shape = {
    .shape_type = element->shape_type,
    .stroke_width = element->stroke_width,
    .filled = element->filled,
    .stroke_style = element->stroke_style,
    .fill_style = element->fill_style,
    .stroke_color = stroke_color,
  };
  ElementConfig config = {
    .type = element->type->type,
    .bg_color = bg_color,
    .position = position,
    .size = size,
    .media = media,
    .drawing = drawing,
    .connection = connection,
    .text = text,
    .shape = shape,
    .rotation_degrees = element->rotation_degrees,
  };

  ModelElement *cloned = model_create_element(model, config);
  if (!cloned) {
    return NULL;
  }

  if (element->video) {
    if (cloned->video) {
      model_video_free(cloned->video);
    }
    cloned->video = element->video;
    cloned->video->ref_count++;
  }

  if (element->audio) {
    if (cloned->audio) {
      model_audio_free(cloned->audio);
    }
    cloned->audio = element->audio;
    cloned->audio->ref_count++;

    if (element->image) {
      if (cloned->image) {
        model_image_free(cloned->image);
      }
      cloned->image = element->image;
      cloned->image->ref_count++;
    }
  }

  return cloned;
}

ModelElement* model_element_clone(Model *model, ModelElement *element, CloneFlags flags) {
  if (!model || !element || !element->type) {
    return NULL;
  }

  if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_DELETED) {
    return NULL;
  }

  // If no flags set, just fork (independent copy)
  if (flags == CLONE_FLAG_NONE) {
    return model_element_fork(model, element);
  }

  ModelElement *cloned_element = model_element_fork(model, element);
  if (!cloned_element) {
    return NULL;
  }

  // Clone text if requested and available
  if ((flags & CLONE_FLAG_TEXT) && element->text && cloned_element->text) {
    g_free(cloned_element->text->text);
    g_free(cloned_element->text->font_description);
    g_free(cloned_element->text->alignment);
    g_free(cloned_element->text);

    cloned_element->text = element->text;
    cloned_element->text->ref_count++;
  }

  // Clone size if requested and available
  if ((flags & CLONE_FLAG_SIZE) && element->size && cloned_element->size) {
    g_free(cloned_element->size);

    cloned_element->size = element->size;
    cloned_element->size->ref_count++;
  }

  // Clone position if requested and available
  if ((flags & CLONE_FLAG_POSITION) && element->position && cloned_element->position) {
    g_free(cloned_element->position);

    cloned_element->position = element->position;
    cloned_element->position->ref_count++;
  }

  // Clone color if requested and available
  if ((flags & CLONE_FLAG_COLOR) && element->bg_color && cloned_element->bg_color) {
    g_free(cloned_element->bg_color);

    cloned_element->bg_color = element->bg_color;
    cloned_element->bg_color->ref_count++;
  }

  return cloned_element;
}

int model_save_elements(Model *model) {
  if (!model || !model->db) {
    return 0;
  }

  if (!database_begin_transaction(model->db)) {
    fprintf(stderr, "Failed to begin transaction\n");
    return 0;
  }

  database_set_current_space_uuid(model->db, model->current_space_uuid);

  int saved_count = 0;
  int error_occurred = 0;
  GList *to_remove = NULL;

  // FIRST: Process DELETIONS with connections first order
  GList *deleted_elements = NULL;
  GHashTableIter iter;
  gpointer key, value;

  // Collect all deleted elements
  g_hash_table_iter_init(&iter, model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement *)value;
    if (element->state == MODEL_STATE_DELETED) {
      deleted_elements = g_list_prepend(deleted_elements, element);
    }
  }

  // Sort deletions: connections first, then elements
  deleted_elements = g_list_sort(deleted_elements, (GCompareFunc)model_compare_for_deletion);

  // Process deletions in proper order
  GList *del_iter = deleted_elements;
  while (del_iter != NULL) {
    ModelElement *element = (ModelElement *)del_iter->data;

    // Check if element exists in database
    ModelElement *db_element = NULL;
    int exists = database_read_element(model->db, element->uuid, &db_element);

    if (exists && db_element) {
      // Element exists in database, delete it
      int delete_success = 0;

      // Special handling for space elements - delete from spaces table
      if (element->type->type == ELEMENT_SPACE && element->target_space_uuid) {
        // Delete the element itself
        delete_success = database_delete_element(model->db, element->uuid);
        if (delete_success) {
          // Also delete the space from the spaces table
          delete_success = database_delete_space(model->db, element->target_space_uuid);
        }
      } else {
        // Regular element deletion
        delete_success = database_delete_element(model->db, element->uuid);
      }

      if (delete_success) {
        saved_count++;
      } else {
        fprintf(stderr, "Failed to delete element %s from database\n", element->uuid);
        error_occurred = 1;
      }

      if (db_element) {
        g_free(db_element->uuid);
        g_free(db_element);
      }
    }

    to_remove = g_list_append(to_remove, g_strdup(element->uuid));
    del_iter = del_iter->next;
  }

  g_list_free(deleted_elements);

  // Recalculate ref_counts from actual DB state, then cleanup orphaned refs
  database_recalculate_ref_counts(model->db);
  cleanup_database_references(model->db);

  // SECOND: Process NEW and UPDATED elements with elements first order
  GList *elements_to_save = NULL;

  // Collect all elements that need to be saved (NEW or UPDATED)
  g_hash_table_iter_init(&iter, model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement *)value;
    if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_UPDATED) {
      elements_to_save = g_list_prepend(elements_to_save, element);
    }
  }

  // Sort for saving: elements first, then connections
  elements_to_save = g_list_sort(elements_to_save, (GCompareFunc)model_compare_for_saving_loading);

  // Process saves in proper order
  GList *save_iter = elements_to_save;
  while (save_iter != NULL) {
    ModelElement *element = (ModelElement *)save_iter->data;

    if (element->state == MODEL_STATE_NEW) {
      if (element->type->type == ELEMENT_SPACE) {
        char *target_space_uuid = NULL;
        if (!database_create_space(model->db, element->text->text, model->current_space_uuid, &target_space_uuid)) {
          g_error("Failed to create target space");
        }
        element->target_space_uuid = target_space_uuid;
      }

      // Save NEW elements to database
      const char *target_space_uuid = element->space_uuid ? element->space_uuid : model->current_space_uuid;
      if (database_create_element(model->db, target_space_uuid, element)) {
        // Add shared resources to model caches
        if (element->type && element->type->id > 0) {
          g_hash_table_insert(model->types, GINT_TO_POINTER(element->type->id), element->type);
        }
        if (element->position && element->position->id > 0) {
          g_hash_table_insert(model->positions, GINT_TO_POINTER(element->position->id), element->position);
        }
        if (element->size && element->size->id > 0) {
          g_hash_table_insert(model->sizes, GINT_TO_POINTER(element->size->id), element->size);
        }
        if (element->text && element->text->id > 0) {
          g_hash_table_insert(model->texts, GINT_TO_POINTER(element->text->id), element->text);
        }
        if (element->bg_color && element->bg_color->id > 0) {
          g_hash_table_insert(model->colors, GINT_TO_POINTER(element->bg_color->id), element->bg_color);
        }
        if (element->image && element->image->id > 0) {
          g_hash_table_insert(model->images, GINT_TO_POINTER(element->image->id), element->image);
        }
        if (element->video && element->video->id > 0) {
          g_hash_table_insert(model->videos, GINT_TO_POINTER(element->video->id), element->video);
        }

        // Change state to SAVED
        element->state = MODEL_STATE_SAVED;
        saved_count++;
      } else {
        fprintf(stderr, "Failed to save element %s to database\n", element->uuid);
        error_occurred = 1;
      }
    } else if (element->state == MODEL_STATE_UPDATED) {
      // Handle space element updates - set parent UUID
      if (element->type->type == ELEMENT_SPACE && element->target_space_uuid) {
        if (!database_set_space_parent_id(model->db, element->target_space_uuid, element->space_uuid)) {
          fprintf(stderr, "Failed to update parent for space %s\n", element->target_space_uuid);
        }
      }

      if (database_update_element(model->db, element->uuid, element)) {
        // Change state back to SAVED
        element->state = MODEL_STATE_SAVED;
        saved_count++;
      } else {
        fprintf(stderr, "Failed to update element %s in database\n", element->uuid);
        error_occurred = 1;
      }
    }

    save_iter = save_iter->next;
  }

  g_list_free(elements_to_save);

  // Remove deleted elements from model after all processing is complete
  GList *iter_list = to_remove;
  while (iter_list) {
    char *uuid = (char *)iter_list->data;
    g_hash_table_remove(model->elements, uuid);
    g_free(uuid);
    iter_list = iter_list->next;
  }
  g_list_free(to_remove);

  if (error_occurred) {
    database_rollback_transaction(model->db);
    return 0;
  }

  if (!database_commit_transaction(model->db)) {
    fprintf(stderr, "Failed to commit transaction\n");
    return 0;
  }

  return saved_count;
}

// Comparison function for saving: elements first, then connections
gint model_compare_for_saving_loading(const ModelElement *a, const ModelElement *b) {
  if (a->type->type == ELEMENT_CONNECTION && b->type->type != ELEMENT_CONNECTION) {
    return 1; // connection comes after non-connection
  } else if (a->type->type != ELEMENT_CONNECTION && b->type->type == ELEMENT_CONNECTION) {
    return -1; // non-connection comes before connection
  } else {
    return 0;
  }
}

// Comparison function for deletion: connections first, then elements
gint model_compare_for_deletion(const ModelElement *a, const ModelElement *b) {
  if (a->type->type == ELEMENT_CONNECTION && b->type->type != ELEMENT_CONNECTION) {
    return -1; // connection comes before non-connection
  } else if (a->type->type != ELEMENT_CONNECTION && b->type->type == ELEMENT_CONNECTION) {
    return 1; // non-connection comes after connection
  } else {
    return 0;
  }
}

ModelElement* model_get_by_visual(Model *model, Element *visual_element) {
  if (!model || !visual_element) {
    return NULL;
  }

  // Fast path: rely on the reverse pointer stored on the visual element.
  if (visual_element->model_element &&
      visual_element->model_element->visual_element == visual_element) {
    return visual_element->model_element;
  }

  // Fallback: scan elements once, then register the reverse link to avoid
  // repeating the lookup in future calls.
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_elem = (ModelElement*)value;
    if (model_elem->visual_element == visual_element) {
      visual_element->model_element = model_elem;
      return model_elem;
    }
  }

  return NULL;
}

int model_get_amount_of_elements(Model *model, const char *space_uuid) {
  return database_get_amount_of_elements(model->db, space_uuid);
}

int model_search_elements(Model *model, const char *search_term, GList **results) {
  if (!model || !model->db) return -1;

  GList *db_results = NULL;
  int rc = database_search_elements(model->db, search_term, &db_results);
  if (rc != 0) return rc;

  // Convert SearchResult to ModelSearchResult
  for (GList *iter = db_results; iter != NULL; iter = iter->next) {
    SearchResult *db_result = (SearchResult*)iter->data;
    ModelSearchResult *model_result = g_new0(ModelSearchResult, 1);

    model_result->element_uuid = g_strdup(db_result->element_uuid);
    model_result->text_content = g_strdup(db_result->text_content);
    model_result->space_uuid = g_strdup(db_result->space_uuid);
    model_result->space_name = g_strdup(db_result->space_name);

    *results = g_list_append(*results, model_result);
  }

  // Free the database results list (but not the items - they were copied)
  g_list_free(db_results);
  return 0;
}

void model_free_search_result(ModelSearchResult *result) {
  if (result) {
    g_free(result->element_uuid);
    g_free(result->text_content);
    g_free(result->space_uuid);
    g_free(result->space_name);
    g_free(result);
  }
}

GList* find_connected_elements_bfs(Model *model, const char *start_uuid) {
  GList *result = NULL;
  GQueue *queue = g_queue_new();
  GHashTable *visited = g_hash_table_new(g_str_hash, g_str_equal);

  g_queue_push_tail(queue, g_strdup(start_uuid));
  g_hash_table_add(visited, g_strdup(start_uuid));

  while (!g_queue_is_empty(queue)) {
    char *current_uuid = g_queue_pop_head(queue);
    ModelElement *current_element = g_hash_table_lookup(model->elements, current_uuid);

    if (current_element) {
      // Add current element to result
      result = g_list_append(result, current_element);

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, model->elements);

      while (g_hash_table_iter_next(&iter, &key, &value)) {
        ModelElement *element = (ModelElement*)value;
        char *element_uuid = (char*)key;

        // Check if this element is connected to the current element
        int is_connected = 0;
        if ((element->from_element_uuid && g_strcmp0(element->from_element_uuid, current_uuid) == 0) ||
            (element->to_element_uuid && g_strcmp0(element->to_element_uuid, current_uuid) == 0)) {
          is_connected = 1;
        }

        // Also check if current element is connected to this element
        if (!is_connected && current_element->from_element_uuid &&
            g_strcmp0(current_element->from_element_uuid, element_uuid) == 0) {
          is_connected = 1;
        }
        if (!is_connected && current_element->to_element_uuid &&
            g_strcmp0(current_element->to_element_uuid, element_uuid) == 0) {
          is_connected = 1;
        }

        if (is_connected && !g_hash_table_contains(visited, element_uuid)) {
          g_queue_push_tail(queue, g_strdup(element_uuid));
          g_hash_table_add(visited, g_strdup(element_uuid));
        }
      }
    }

    g_free(current_uuid);
  }

  g_queue_free(queue);
  g_hash_table_destroy(visited);
  return result;
}

GList* find_children_bfs(Model *model, const char *parent_uuid) {
  GList *result = NULL;
  GQueue *queue = g_queue_new();
  GHashTable *visited = g_hash_table_new(g_str_hash, g_str_equal);

  g_queue_push_tail(queue, g_strdup(parent_uuid));
  g_hash_table_add(visited, g_strdup(parent_uuid));

  while (!g_queue_is_empty(queue)) {
    char *current_uuid = g_queue_pop_head(queue);
    ModelElement *current_element = g_hash_table_lookup(model->elements, current_uuid);

    if (current_element) {
      // Add current element to result, but exclude the starting parent
      if (g_strcmp0(current_uuid, parent_uuid) != 0) {
        result = g_list_append(result, current_element);
      }

      // Find all connections that originate FROM the current element
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, model->elements);

      while (g_hash_table_iter_next(&iter, &key, &value)) {
        ModelElement *element = (ModelElement*)value;

        // Only follow outgoing connections (arrows going FROM current element TO other elements)
        if (element->type->type == ELEMENT_CONNECTION &&
            element->from_element_uuid && g_strcmp0(element->from_element_uuid, current_uuid) == 0) {

          // This connection goes FROM current_uuid TO element->to_element_uuid
          if (element->to_element_uuid && !g_hash_table_contains(visited, element->to_element_uuid)) {
            g_queue_push_tail(queue, g_strdup(element->to_element_uuid));
            g_hash_table_add(visited, g_strdup(element->to_element_uuid));
          }

          // Also include the connection itself in the result (but not in the starting case)
          if (g_strcmp0(current_uuid, parent_uuid) != 0 ||
              g_strcmp0(element->from_element_uuid, parent_uuid) == 0) {
            result = g_list_append(result, element);
          }
        }
      }
    }

    g_free(current_uuid);
  }

  g_queue_free(queue);
  g_hash_table_destroy(visited);
  return result;
}

int move_element_to_space(Model *model, ModelElement *element, const char *new_space_uuid) {
  if (!element || !new_space_uuid) return 0;

  // Find all connected elements using BFS
  GList *all_elements_to_move = find_connected_elements_bfs(model, element->uuid);
  all_elements_to_move = g_list_prepend(all_elements_to_move, element);

  // Update space and mark as updated
  for (GList *iter = all_elements_to_move; iter != NULL; iter = iter->next) {
    ModelElement *elem = (ModelElement*)iter->data;

    g_free(elem->space_uuid);
    elem->space_uuid = g_strdup(new_space_uuid);

    if (elem->state != MODEL_STATE_NEW) {
      elem->state = MODEL_STATE_UPDATED;
    }
  }

  g_list_free(all_elements_to_move);
  return 1;
}

int model_get_all_spaces(Model *model, GList **spaces) {
  if (!model || !model->db) return 0;

  GList *db_spaces = NULL;
  int rc = database_get_all_spaces(model->db, &db_spaces);
  if (!rc) return 0;

  // Convert SpaceInfo to ModelSpaceInfo
  for (GList *iter = db_spaces; iter != NULL; iter = iter->next) {
    SpaceInfo *db_space = (SpaceInfo*)iter->data;
    ModelSpaceInfo *model_space = g_new0(ModelSpaceInfo, 1);

    model_space->uuid = g_strdup(db_space->uuid);
    model_space->name = g_strdup(db_space->name);
    model_space->created_at = g_strdup(db_space->created_at);

    *spaces = g_list_append(*spaces, model_space);
  }

  // Free the database results list (but not the items - they were copied)
  g_list_free(db_spaces);
  return 1;
}

void model_free_space_info(ModelSpaceInfo *space) {
  if (space) {
    g_free(space->uuid);
    g_free(space->name);
    g_free(space->created_at);
    g_free(space);
  }
}

int model_load_video_data(Model *model, ModelVideo *video) {
  if (!video || video->is_loaded) {
    return 0;  // Already loaded or invalid
  }

  if (database_load_video_data(model->db, video->id, &video->video_data, &video->video_size)) {
    video->is_loaded = TRUE;
    return 1;
  }

  return 0;
}

int model_load_audio_data(Model *model, ModelAudio *audio) {
  if (!audio || audio->is_loaded) {
    return 0;  // Already loaded or invalid
  }

  if (database_load_audio_data(model->db, audio->id, &audio->audio_data, &audio->audio_size)) {
    audio->is_loaded = TRUE;
    return 1;
  }

  return 0;
}

// Space background and grid operations
int model_set_space_background_color(Model *model, const char *space_uuid, const char *background_color) {
  if (!model || !model->db) {
    return 0;
  }

  if (database_set_space_background_color(model->db, space_uuid, background_color)) {
    g_free(model->current_space_background_color);
    model->current_space_background_color = g_strdup(background_color);
    return 1;
  }

  return 0;
}

int model_set_space_grid_settings(Model *model, const char *space_uuid, int grid_enabled, const char *grid_color) {
  if (!model || !model->db) {
    return 0;
  }

  if (database_set_space_grid_settings(model->db, space_uuid, grid_enabled, grid_color)) {
    model->current_space_show_grid = grid_enabled;
    if (grid_color) {
      gdk_rgba_parse(&model->current_space_grid_color, grid_color);
    }
    return 1;
  }

  return 0;
}

// Space hierarchy operations
int model_get_space_parent_uuid(Model *model, const char *space_uuid, char **parent_uuid) {
  if (!model || !model->db || !space_uuid || !parent_uuid) {
    return 0;
  }

  return database_get_space_parent_id(model->db, space_uuid, parent_uuid);
}
