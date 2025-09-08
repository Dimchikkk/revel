#include "model.h"
#include "database.h"
#include "element.h"
#include "note.h"
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
  g_free(model->current_space_uuid);
  g_free(model);
}

void model_type_free(ModelType *type) {
  if (!type) return;
  g_free(type);
}

void model_text_free(ModelText *text) {
  if (!text) return;
  g_free(text->text);
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

  // Important: Don't free shared resources here!
  // They are managed by the respective hash tables and will be
  // automatically freed when the hash tables are destroyed
  // or when their reference counts drop to zero

  g_free(element);
}

Model* model_new() {
  Model *model = g_new0(Model, 1);
  model->elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)model_element_free);
  model->types = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->texts = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->positions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->sizes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->colors = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  model->db = NULL;

  if (!database_init(&model->db, "velo2.db")) {
    g_free(model);
    return NULL;
  }

  // Get current space UUID
  char *space_uuid = NULL;
  database_get_current_space_uuid(model->db, &space_uuid);
  model->current_space_uuid = space_uuid;

  // Load the space
  model_load_space(model);

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

// Create a note element
ModelElement* model_create_note(Model *model, int x, int y, int z, int width, int height, const char *text) {
  if (model == NULL) {
    g_printerr("Error: model is NULL in model_create_note\n");
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
  model_type->type = ELEMENT_NOTE;
  model_type->ref_count = 1;
  element->type = model_type;

  // Create position reference
  ModelPosition *position = g_new0(ModelPosition, 1);
  position->id = -1;  // Temporary ID until saved to database
  position->x = x;
  position->y = y;
  position->z = z;
  position->ref_count = 1;
  element->position = position;

  // Create size reference
  ModelSize *size = g_new0(ModelSize, 1);
  size->id = -1;  // Temporary ID until saved to database
  size->width = width;
  size->height = height;
  size->ref_count = 1;
  element->size = size;

  // Create text reference if text is provided
  if (text != NULL) {
    ModelText *model_text = g_new0(ModelText, 1);
    model_text->id = -1;  // Temporary ID until saved to database
    model_text->text = g_strdup(text);
    model_text->ref_count = 1;
    element->text = model_text;
  }

  // Add the element to the model's elements hash table
  g_hash_table_insert(model->elements, g_strdup(element->uuid), element);

  return element;
}

// Create a paper note element
ModelElement* model_create_paper_note(Model *model, int x, int y, int z, int width, int height, const char *text) {
  if (model == NULL) {
    g_printerr("Error: model is NULL in model_create_paper_note\n");
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
  model_type->type = ELEMENT_PAPER_NOTE;
  model_type->ref_count = 1;
  element->type = model_type;

  // Create position reference
  ModelPosition *position = g_new0(ModelPosition, 1);
  position->id = -1;  // Temporary ID until saved to database
  position->x = x;
  position->y = y;
  position->z = z;
  position->ref_count = 1;
  element->position = position;

  // Create size reference
  ModelSize *size = g_new0(ModelSize, 1);
  size->id = -1;  // Temporary ID until saved to database
  size->width = width;
  size->height = height;
  size->ref_count = 1;
  element->size = size;

  // Create text reference if text is provided
  if (text != NULL) {
    ModelText *model_text = g_new0(ModelText, 1);
    model_text->id = -1;  // Temporary ID until saved to database
    model_text->text = g_strdup(text);
    model_text->ref_count = 1;
    element->text = model_text;
  }

  // Add the element to the model's elements hash table
  g_hash_table_insert(model->elements, g_strdup(element->uuid), element);

  return element;
}

// Create a connection element
ModelElement* model_create_connection(Model *model, const char *from_element_uuid, const char *to_element_uuid,
                                      int from_point, int to_point, int z) {
  if (model == NULL) {
    g_printerr("Error: model is NULL in model_create_connection\n");
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
  model_type->type = ELEMENT_CONNECTION;
  model_type->ref_count = 1;
  element->type = model_type;

  // Create position reference (connections don't need position, but we need to satisfy the schema)
  ModelPosition *position = g_new0(ModelPosition, 1);
  position->id = -1;  // Temporary ID until saved to database
  position->x = 0;
  position->y = 0;
  position->z = z;
  position->ref_count = 1;
  element->position = position;

  // Create size reference (connections don't need size, but we need to satisfy the schema)
  ModelSize *size = g_new0(ModelSize, 1);
  size->id = -1;  // Temporary ID until saved to database
  size->width = 1;
  size->height = 1;
  size->ref_count = 1;
  element->size = size;

  // Set connection properties
  element->from_element_uuid = g_strdup(from_element_uuid);
  element->to_element_uuid = g_strdup(to_element_uuid);
  element->from_point = from_point;
  element->to_point = to_point;

  // Add the element to the model's elements hash table
  g_hash_table_insert(model->elements, g_strdup(element->uuid), element);

  return element;
}

// Create a space element
ModelElement* model_create_space(Model *model, const char *name, int x, int y, int z, int width, int height) {
  if (model == NULL) {
    g_printerr("Error: model is NULL in model_create_space\n");
    return NULL;
  }

  // Create a new ModelElement
  ModelElement *element = g_new0(ModelElement, 1);
  element->uuid = model_generate_uuid();
  element->state = MODEL_STATE_NEW;

  element->space_uuid = model->current_space_uuid;

  // Create type reference
  ModelText *model_text = g_new0(ModelText, 1);
  model_text->id = -1;  // Temporary ID until saved to database
  model_text->text = g_strdup(name);
  model_text->ref_count = 1;
  element->text = model_text;

  ModelType *model_type = g_new0(ModelType, 1);
  model_type->id = -1;  // Temporary ID until saved to database
  model_type->type = ELEMENT_SPACE;
  model_type->ref_count = 1;
  element->type = model_type;

  // Create position reference
  ModelPosition *position = g_new0(ModelPosition, 1);
  position->id = -1;  // Temporary ID until saved to database
  position->x = x;
  position->y = y;
  position->z = z;
  position->ref_count = 1;
  element->position = position;

  // Create size reference
  ModelSize *size = g_new0(ModelSize, 1);
  size->id = -1;  // Temporary ID until saved to database
  size->width = width;
  size->height = height;
  size->ref_count = 1;
  element->size = size;

  // Add the element to the model's elements hash table
  g_hash_table_insert(model->elements, g_strdup(element->uuid), element);

  return element;
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
  if (element->color && element->color->ref_count > 0) element->color->ref_count--;

  // Mark element as deleted
  element->state = MODEL_STATE_DELETED;
  return 1;
}

ModelElement* model_element_fork(Model *model, ModelElement *element) {
  if (!model || !element || !element->type) {
    return NULL;
  }

  if (element->state == MODEL_STATE_NEW || element->state == MODEL_STATE_DELETED) {
    return NULL;
  }

  switch (element->type->type) {
  case ELEMENT_NOTE:
    if (element->position && element->size && element->text) {
      return model_create_note(model,
                               element->position->x,
                               element->position->y,
                               element->position->z,
                               element->size->width,
                               element->size->height,
                               element->text->text);
    }
    break;

  case ELEMENT_PAPER_NOTE:
    if (element->position && element->size && element->text) {
      return model_create_paper_note(model,
                                     element->position->x,
                                     element->position->y,
                                     element->position->z,
                                     element->size->width,
                                     element->size->height,
                                     element->text->text);
    }
    break;

  case ELEMENT_CONNECTION:
    if (element->from_element_uuid && element->to_element_uuid) {
      return model_create_connection(model,
                                     element->from_element_uuid,
                                     element->to_element_uuid,
                                     element->from_point,
                                     element->to_point,
                                     element->position->z
                                     );
    }
    break;

  case ELEMENT_SPACE:
    if (element->position && element->size && element->target_space_uuid) {
      return model_create_space(model,
                                element->text->text,
                                element->position->x,
                                element->position->y,
                                element->position->z,
                                element->size->width,
                                element->size->height);
    }
    break;

  default:
    g_warning("Unknown element type: %d", element->type->type);
    break;
  }

  return NULL;
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
  GList *to_remove = NULL;  // List of UUIDs to remove after iteration

  // Get all elements and sort them so CONNECTIONS come last
  GList *elements_list = g_hash_table_get_values(model->elements);
  elements_list = g_list_sort(elements_list, (GCompareFunc)compare_model_elements_for_serialization);

  // Iterate through sorted elements
  GList *iter = elements_list;
  while (iter != NULL) {
    ModelElement *element = (ModelElement *)iter->data;

    switch (element->state) {
    case MODEL_STATE_NEW: {
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
        if (element->color && element->color->id > 0) {
          g_hash_table_insert(model->colors, GINT_TO_POINTER(element->color->id), element->color);
        }

        // Change state to SAVED
        element->state = MODEL_STATE_SAVED;
        saved_count++;
      } else {
        fprintf(stderr, "Failed to save element %s to database\n", element->uuid);
      }
      break;
    }

      // Assumes that element can't have new property with UPDATED state
    case MODEL_STATE_UPDATED:
      if (database_update_element(model->db, element->uuid, element)) {
        // Change state back to SAVED
        element->state = MODEL_STATE_SAVED;
        saved_count++;
      } else {
        fprintf(stderr, "Failed to update element %s in database\n", element->uuid);
      }
      break;

    case MODEL_STATE_DELETED: {
      // Check if element exists in database
      ModelElement *db_element = NULL;
      int exists = database_read_element(model->db, element->uuid, &db_element);

      if (exists && db_element) {
        // Element exists in database, delete it
        if (database_delete_element(model->db, element->uuid)) {
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

          if (element->color && element->color->id > 0) {
            database_update_color_ref(model->db, element->color);
            if (element->color->ref_count < 1) {
              g_hash_table_remove(model->colors, GINT_TO_POINTER(element->color->id));
              model_color_free(element->color);
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
      break;
    }

    case MODEL_STATE_SAVED:
      // Already saved, nothing to do
      break;
    }

    iter = iter->next;
  }

  // Free the sorted list (doesn't free the elements themselves)
  g_list_free(elements_list);

  // Remove deleted elements after iteration completes
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

// Comparison function to sort elements with CONNECTIONS type last
gint compare_model_elements_for_serialization(ModelElement *a, ModelElement *b) {
  if (a->type->type == ELEMENT_CONNECTION && b->type->type != ELEMENT_CONNECTION) {
    return 1; // a (connection) comes after b
  } else if (a->type->type != ELEMENT_CONNECTION && b->type->type == ELEMENT_CONNECTION) {
    return -1; // a comes before b (connection)
  } else {
    return 0; // equal order
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
