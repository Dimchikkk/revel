#include "model.h"
#include "database.h"
#include "element.h"
#include "note.h"
#include "connection.h"
#include <string.h>
#include <stdio.h>
#include "canvas_core.h"
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
  model->db = NULL;

  model->current_space_background_color = NULL;
  model->current_space_name = NULL;
  model->current_space_show_grid = FALSE;
  model->current_space_grid_color = (GdkRGBA){0.8, 0.8, 0.8, 1.0};

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

int model_get_parent_id(Model *model, char **space_parent_id) {
  if (!model || !model->db) return 0;
  return database_get_space_parent_id(model->db, model->current_space_uuid, space_parent_id);
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

  element->space_uuid = model->current_space_uuid;

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

  if (config.media.type == MEDIA_TYPE_VIDEO && config.media.video_data && config.media.video_size > 0) {
    ModelVideo *model_video = g_new0(ModelVideo, 1);
    model_video->id = -1;
    model_video->thumbnail_data = g_malloc(config.media.image_size);
    memcpy(model_video->thumbnail_data, config.media.image_data, config.media.image_size);
    model_video->thumbnail_size = config.media.image_size;
    model_video->video_data = g_malloc(config.media.video_size);
    memcpy(model_video->video_data, config.media.video_data, config.media.video_size);
    model_video->video_size = config.media.video_size;
    model_video->duration = config.media.duration;
    model_video->is_loaded = TRUE;
    model_video->ref_count = 1;
    element->video = model_video;
  }

  if (config.drawing.drawing_points != NULL) {
    element->drawing_points = g_array_copy((GArray*)config.drawing.drawing_points);
  } else {
    element->drawing_points = NULL;
  }

  element->stroke_width = config.drawing.stroke_width != 0 ? config.drawing.stroke_width : config.shape.stroke_width;
  element->shape_type = config.shape.shape_type;
  element->filled = config.shape.filled;

  // Set default arrowhead type for connections
  if (config.type == ELEMENT_CONNECTION) {
    element->arrowhead_type = ARROWHEAD_SINGLE;
    element->connection_type = CONNECTION_TYPE_PARALLEL;
  }

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

int model_delete_element(Model *model, ModelElement *element) {
  // If element is already deleted, do nothing
  if (!model || !element) {
    return 0;
  }

  if (element->state == MODEL_STATE_DELETED) {
    return 0;
  }

  // Decrement ref_count for shared resources with proper NULL checks
  if (element->type && element->type->ref_count > 0) element->type->ref_count--;
  if (element->position && element->position->ref_count > 0) element->position->ref_count--;
  if (element->size && element->size->ref_count > 0) element->size->ref_count--;
  if (element->text && element->text->ref_count > 0) element->text->ref_count--;
  if (element->bg_color && element->bg_color->ref_count > 0) element->bg_color->ref_count--;
  if (element->image && element->image->ref_count > 0) element->image->ref_count--;
  if (element->video && element->video->ref_count > 0) element->video->ref_count--;

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
  ElementColor text_color = {
    .r = element->text->r,
    .g = element->text->g,
    .b = element->text->b,
    .a = element->text->a,
  };
  ElementSize size = {
    .width = element->size->width,
    .height = element->size->height,
  };
  MediaType media_type = MEDIA_TYPE_NONE;
  if (element->image) {
    media_type = MEDIA_TYPE_IMAGE;
  } else if (element->video) {
    media_type = MEDIA_TYPE_VIDEO;
  }
  ElementMedia media = {
    .type = media_type,
    .image_data = element->image ? element->image->image_data : NULL,
    .image_size = element->image ? element->image->image_size : 0,
    .video_data = element->video ? element->video->video_data : NULL,
    .video_size = element->video ? element->video->video_size : 0,
    .duration = element->video ? element->video->duration : 0,
  };

  ElementConnection connection = {
    .from_element_uuid = element->from_element_uuid,
    .to_element_uuid = element->to_element_uuid,
    .from_point = element->from_point,
    .to_point = element->to_point,
  };
  ElementDrawing drawing = {
    .drawing_points = NULL,
    .stroke_width = 0,
  };
  ElementText text = {
    .text = element->text->text,
    .text_color = text_color,
    .font_description = element->text->font_description,
  };
  ElementShape shape = {
    .shape_type = element->shape_type,
    .stroke_width = element->stroke_width,
    .filled = element->filled,
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
  };

  return model_create_element(model, config);
}

ModelElement* model_element_clone_by_text(Model *model, ModelElement *element) {
  if (!model || !element || !element->type) {
    return NULL;
  }

  if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_DELETED) {
    return NULL;
  }

  ModelElement *cloned_element = model_element_fork(model, element);
  if (!cloned_element) {
    return NULL;
  }

  if (element->text && cloned_element->text) {
    g_free(cloned_element->text->text);
    g_free(cloned_element->text);

    cloned_element->text = element->text;
    cloned_element->text->ref_count++;
  }

  return cloned_element;
}

ModelElement* model_element_clone_by_size(Model *model, ModelElement *element) {
  if (!model || !element || !element->type) {
    return NULL;
  }

  if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_DELETED) {
    return NULL;
  }

  ModelElement *cloned_element = model_element_fork(model, element);
  if (!cloned_element) {
    return NULL;
  }

  if (element->size && cloned_element->size) {
    g_free(cloned_element->size);

    cloned_element->size = element->size;
    cloned_element->size->ref_count++;
  }

  return cloned_element;
}

int model_save_elements(Model *model) {
  if (!model || !model->db) {
    return 0;
  }

  database_set_current_space_uuid(model->db, model->current_space_uuid);

  int saved_count = 0;
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
        // Update reference counts in database
        if (element->type && element->type->id > 0) {
          database_update_type_ref(model->db, element->type);
          if (element->type->ref_count < 1) {
            g_hash_table_remove(model->types, GINT_TO_POINTER(element->type->id));
            model_type_free(element->type);
          }
        }

        if (element->position && element->position->id > 0) {
          database_update_position_ref(model->db, element->position);
          if (element->position->ref_count < 1) {
            g_hash_table_remove(model->positions, GINT_TO_POINTER(element->position->id));
            model_position_free(element->position);
          }
        }

        if (element->size && element->size->id > 0) {
          database_update_size_ref(model->db, element->size);
          if (element->size->ref_count < 1) {
            g_hash_table_remove(model->sizes, GINT_TO_POINTER(element->size->id));
            model_size_free(element->size);
          }
        }

        if (element->text && element->text->id > 0) {
          database_update_text_ref(model->db, element->text);
          if (element->text->ref_count < 1) {
            g_hash_table_remove(model->texts, GINT_TO_POINTER(element->text->id));
            model_text_free(element->text);
          }
        }

        if (element->bg_color && element->bg_color->id > 0) {
          database_update_color_ref(model->db, element->bg_color);
          if (element->bg_color->ref_count < 1) {
            g_hash_table_remove(model->colors, GINT_TO_POINTER(element->bg_color->id));
            model_color_free(element->bg_color);
          }
        }

        if (element->image && element->image->id > 0) {
          database_update_image_ref(model->db, element->image);
          if (element->image->ref_count < 1) {
            g_hash_table_remove(model->images, GINT_TO_POINTER(element->image->id));
            model_image_free(element->image);
          }
        }

        if (element->video && element->video->id > 0) {
          database_update_video_ref(model->db, element->video);
          if (element->video->ref_count < 1) {
            g_hash_table_remove(model->videos, GINT_TO_POINTER(element->video->id));
            model_video_free(element->video);
          }
        }

        cleanup_database_references(model->db);
        saved_count++;
      } else {
        fprintf(stderr, "Failed to delete element %s from database\n", element->uuid);
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
      if (database_create_element(model->db, model->current_space_uuid, element)) {
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
  if (!model || !visual_element) return NULL;

  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_elem = (ModelElement*)value;
    if (model_elem->visual_element == visual_element) {
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

    // Check if this space UUID is the same content as model's current space UUID
    if (elem->space_uuid && model->current_space_uuid &&
        strcmp(elem->space_uuid, model->current_space_uuid) == 0) {
      // This element's space UUID matches the model's current space UUID
      // Don't free it - just duplicate the new one
      elem->space_uuid = g_strdup(new_space_uuid);
    } else {
      // Safe to free and replace
      g_free(elem->space_uuid);
      elem->space_uuid = g_strdup(new_space_uuid);
    }

    elem->state = MODEL_STATE_UPDATED;
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
