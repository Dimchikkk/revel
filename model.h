#ifndef MODEL_H
#define MODEL_H

#include <sqlite3.h>
#include <glib.h>
#include <uuid/uuid.h>
#include "element.h"

typedef enum {
    MODEL_STATE_NEW,      // Not yet saved to database
    MODEL_STATE_SAVED,    // Successfully saved to database
    MODEL_STATE_UPDATED,  // Saved but has been updated
    MODEL_STATE_DELETED,
} ModelState;

typedef struct _ModelText ModelText;
typedef struct _ModelType ModelType;
typedef struct _ModelSize ModelSize;
typedef struct _ModelColor ModelColor;
typedef struct _ModelPosition ModelPosition;
typedef struct _ModelElement ModelElement;
typedef struct _Model Model;

struct _ModelType {
  gint id;                    // Unique ID for this element type
  ElementType type;           // The actual enum value
  gint ref_count;
};

struct _ModelText {
  gint id;
  gchar *text;
  gint ref_count;
};

struct _ModelPosition {
  gint id;                    // Unique ID for this position
  gint x, y, z;
  gint ref_count;
};

struct _ModelSize {
  gint id;                    // Unique ID for this size
  gint width, height;
  gint ref_count;
};

struct _ModelColor {
  gint id;                    // Unique ID for this color
  gdouble r, g, b, a;
  gint ref_count;
};

struct _ModelElement {
  gchar* uuid;                // UUID string for the element
  gchar* space_uuid;
  ModelType* type;            // Shared element type reference
  ModelPosition* position;    // Shared position
  ModelSize* size;            // Shared size
  ModelText* text;            // Shared text
  ModelColor* color;          // Shared color
  Element* visual_element;    // Pointer to visual representation
  ModelState state;

  // For connections
  gchar *from_element_uuid;
  gchar *to_element_uuid;
  gint from_point;
  gint to_point;

  // For space elements
  gchar *target_space_uuid;
};

// Model manages all elements
struct _Model {
  gchar *current_space_uuid;
  GHashTable *elements;       // uuid string -> ModelElement*
  GHashTable *types;          // type_id -> ModelType* (shared types)
  GHashTable *texts;          // text_id -> ModelText* (shared texts)
  GHashTable *positions;      // positon_id -> ModelPosition* (shared position)
  GHashTable *sizes;          // size_id -> ModelSize* (shared size)
  GHashTable *colors;         // color_id -> ModelColor* (shared_color)
  sqlite3 *db;
};

// Model management
Model* model_new();
void model_free(Model *model);
void model_element_free(ModelElement *element);
void model_load_space(Model *model);
int model_save_elements(Model *model);

int model_get_space_name(Model *model, const char *space_uuid, char **space_name);
int model_get_parent_id(Model *model, char **space_parent_id);

// Creation
ModelElement* model_create_note(Model *model, int x, int y, int z, int width, int height, const char *text);
ModelElement* model_create_paper_note(Model *model, int x, int y, int z, int width, int height, const char *text);
ModelElement* model_create_connection(Model *model, const char *from_element_uuid, const char *to_element_uuid, int from_point, int to_point, int z);
ModelElement* model_create_space(Model *model, const char *name, int x, int y, int z, int width, int height);

// Fork/cloning
ModelElement* model_element_fork(Model *model, ModelElement *element);
ModelElement* model_element_clone_by_text(Model *model, ModelElement *element);
ModelElement* model_element_clone_by_size(Model *model, ModelElement *element);

// Updating props
int model_update_text(Model *model, ModelElement *element, const char *text);
int model_update_position(Model *model, ModelElement *element, int x, int y, int z);
int model_update_size(Model *model, ModelElement *element, int width, int height);

// Deletion
int model_delete_element(Model *model, ModelElement *element);

// Helper functions
gchar *model_generate_uuid(void);
ModelElement* model_get_by_visual(Model *model, Element *visual_element);
gint compare_model_elements_for_serialization(ModelElement *a, ModelElement *b);

#endif
