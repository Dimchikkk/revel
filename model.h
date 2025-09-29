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
typedef struct _ModelImage ModelImage;
typedef struct _ModelVideo ModelVideo;

struct _ModelVideo {
  gint id;
  unsigned char *thumbnail_data;  // Thumbnail image data (loaded immediately)
  int thumbnail_size;             // Thumbnail size in bytes

  // Video data - loaded on demand
  unsigned char *video_data;      // Video file data (NULL until loaded)
  int video_size;                 // Video file size in bytes
  gint duration;                  // Video duration in seconds
  gboolean is_loaded;             // Flag to track if video data is loaded
  gint ref_count;
};

struct _ModelType {
  gint id;                    // Unique ID for this element type
  ElementType type;           // The actual enum value
  gint ref_count;
};

struct _ModelText {
  gint id;
  gchar *text;
  gdouble r;
  gdouble g;
  gdouble b;
  gdouble a;
  gchar *font_description;
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

struct _ModelImage {
  gint id;
  unsigned char *image_data;
  int image_size;
  gint ref_count;
};

typedef graphene_point_t DrawingPoint;

struct _ModelElement {
  gchar* uuid;                // UUID string for the element
  gchar* space_uuid;
  ModelType* type;            // Shared element type reference
  ModelPosition* position;    // Shared position
  ModelSize* size;            // Shared size
  ModelText* text;            // Shared text
  ModelColor* bg_color;       // Shared bg_color
  ModelVideo* video;          // Shared video
  ModelImage* image;
  Element* visual_element;    // Pointer to visual representation
  ModelState state;

  // For connections
  gchar *from_element_uuid;
  gchar *to_element_uuid;
  gint from_point;
  gint to_point;

  gint stroke_width;
  GArray* drawing_points; // Array of DrawingPoint

  // For shape elements
  gint shape_type;
  gboolean filled;

  // For connection elements
  gint connection_type;
  gint arrowhead_type;

  // For space elements
  gchar *target_space_uuid;

  // Element description
  gchar *description;
  gchar *created_at;
};

// Model manages all elements
struct _Model {
  gchar *current_space_uuid;
  GHashTable *elements;       // uuid string -> ModelElement*
  GHashTable *types;          // type_id -> ModelType* (shared types)
  GHashTable *texts;          // text_id -> ModelText* (shared texts)
  GHashTable *positions;      // positon_id -> ModelPosition* (shared position)
  GHashTable *sizes;          // size_id -> ModelSize* (shared size)
  GHashTable *colors;         // bg_color_id -> ModelColor* (shared color)
  GHashTable *images;         // image_id -> ModelImage (shared image)
  GHashTable *videos;         // video_id -> ModelVideo (shared video)
  sqlite3 *db;

  // Cached space settings
  char *current_space_background_color;
  char *current_space_name;
  gboolean current_space_show_grid;
  GdkRGBA current_space_grid_color;
};

// Model management
Model* model_new_with_file(const char *db_filename);
void model_free(Model *model);
void model_element_free(ModelElement *element);
void model_load_space(Model *model);
int model_save_elements(Model *model);

int model_get_space_name(Model *model, const char *space_uuid, char **space_name);
int model_get_space_parent_uuid(Model *model, const char *space_uuid, char **parent_uuid);
int model_get_amount_of_elements(Model *model, const char *space_uuid);

// Creation
ModelElement* model_create_element(Model *model, ElementConfig config);

// Fork/cloning
ModelElement* model_element_fork(Model *model, ModelElement *element);
ModelElement* model_element_clone_by_text(Model *model, ModelElement *element);
ModelElement* model_element_clone_by_size(Model *model, ModelElement *element);

// Updating props
int model_update_text(Model *model, ModelElement *element, const char *text);
int model_update_text_color(Model *model, ModelElement *element, double r, double g, double b, double a);
int model_update_font(Model *model, ModelElement *element, const char *font_description);
int model_update_position(Model *model, ModelElement *element, int x, int y, int z);
int model_update_size(Model *model, ModelElement *element, int width, int height);
// This method slightly inconsistent with other update methods: it doesn't create ModelColor if it is NULL
int model_update_color(Model *model, ModelElement *element, double r, double g, double b, double a);

// Deletion
int model_delete_element(Model *model, ModelElement *element);

// Helper functions
gchar *model_generate_uuid(void);
ModelElement* model_get_by_visual(Model *model, Element *visual_element);
gint model_compare_for_saving_loading(const ModelElement *a, const ModelElement *b);
gint model_compare_for_deletion(const ModelElement *a, const ModelElement *b);

typedef struct {
    char *element_uuid;
    char *text_content;
    char *space_uuid;
    char *space_name;
} ModelSearchResult;

int model_search_elements(Model *model, const char *search_term, GList **results);
void model_free_search_result(ModelSearchResult *result);

int move_element_to_space(Model *model, ModelElement *element, const char *new_space_uuid);
GList* find_connected_elements_bfs(Model *model, const char *start_uuid);
GList* find_children_bfs(Model *model, const char *parent_uuid);

typedef struct {
    char *uuid;
    char *name;
    char *created_at;
} ModelSpaceInfo;

int model_get_all_spaces(Model *model, GList **spaces);
void model_free_space_info(ModelSpaceInfo *space);

int model_load_video_data(Model *model, ModelVideo *video);

// Space background and grid operations
void model_load_space_settings(Model *model, const char *space_uuid);
int model_set_space_background_color(Model *model, const char *space_uuid, const char *background_color);
int model_set_space_grid_settings(Model *model, const char *space_uuid, int grid_enabled, const char *grid_color);


#endif
