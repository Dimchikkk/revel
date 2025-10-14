#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <uuid/uuid.h>
#include <glib.h>
#include "model.h"

// Initialize database
int database_init(sqlite3 **db, const char *filename);
void database_close(sqlite3 *db);
int database_create_tables(sqlite3 *db);
int database_init_default_namespace(sqlite3 *db);

// Transaction management
int database_begin_transaction(sqlite3 *db);
int database_commit_transaction(sqlite3 *db);
int database_rollback_transaction(sqlite3 *db);

// UUID helpers
void database_generate_uuid(char **uuid_str);
int database_is_valid_uuid(const char *uuid_str);

// Type reference operations
int database_create_type_ref(sqlite3 *db, ElementType element_type, int *type_id);
int database_read_type_ref(sqlite3 *db, int type_id, ModelType **type);
int database_update_type_ref(sqlite3 *db, ModelType *type);

// Position reference operations
int database_create_position_ref(sqlite3 *db, int x, int y, int z, int *position_id);
int database_read_position_ref(sqlite3 *db, int position_id, ModelPosition **position);
int database_update_position_ref(sqlite3 *db, ModelPosition *position);

// Size reference operations
int database_create_size_ref(sqlite3 *db, int width, int height, int *size_id);
int database_read_size_ref(sqlite3 *db, int size_id, ModelSize **size);
int database_update_size_ref(sqlite3 *db, ModelSize *size);

// Text reference operations
int database_create_text_ref(sqlite3 *db,
                             const char *text,
                             double r, double g, double b, double a,
                             const char *font_description,
                             gboolean strikethrough,
                             const char *alignment,
                             int *text_id);
int database_read_text_ref(sqlite3 *db, int text_id, ModelText **text);
int database_update_text_ref(sqlite3 *db, ModelText *text);

// Color reference operations
int database_create_color_ref(sqlite3 *db, double r, double g, double b, double a, int *bg_color_id);
int database_read_color_ref(sqlite3 *db, int bg_color_id, ModelColor **color);
int database_update_color_ref(sqlite3 *db, ModelColor *color);

// Image reference operations
int database_create_image_ref(sqlite3 *db, const unsigned char *image_data, int image_size, int *image_id);
int database_read_image_ref(sqlite3 *db, int image_id, ModelImage **image);
int database_update_image_ref(sqlite3 *db, ModelImage *image);

int cleanup_database_references(sqlite3 *db);

// Video reference operations
int database_create_video_ref(sqlite3 *db,
                             const unsigned char *thumbnail_data, int thumbnail_size,
                             const unsigned char *video_data, int video_size,
                             int duration, int *video_id);
int database_read_video_ref(sqlite3 *db, int video_id, ModelVideo **video);
// This method doesn't update video data (it needs to be like that since video data is loaded lazyly).
int database_update_video_ref(sqlite3 *db, ModelVideo *video);

// Audio reference operations
int database_create_audio_ref(sqlite3 *db,
                             const unsigned char *audio_data, int audio_size,
                             int duration, int *audio_id);
int database_read_audio_ref(sqlite3 *db, int audio_id, ModelAudio **audio);
int database_update_audio_ref(sqlite3 *db, ModelAudio *audio);
int database_load_audio_data(sqlite3 *db, int audio_id, unsigned char **audio_data, int *audio_size);

// Element operations
int database_create_element(sqlite3 *db, const char *space_uuid, ModelElement *element);
// Read element can be used to check whether element exists in table, if model element is NULL it doesn't exist
int database_read_element(sqlite3 *db, const char *element_uuid, ModelElement **element);
int database_update_element(sqlite3 *db, const char *element_uuid, const ModelElement *updates);
int database_delete_element(sqlite3 *db, const char *element_uuid);

// Space operations
int database_create_space(sqlite3 *db, const char *name, const char *parent_uuid, char **space_uuid);
int database_delete_space(sqlite3 *db, const char *space_uuid);
int database_get_current_space_uuid(sqlite3 *db, char **space_uuid);
int database_set_current_space_uuid(sqlite3 *db, const char *space_uuid);
int database_get_space_name(sqlite3 *db, const char *space_uuid, char **space_name);
int database_get_space_parent_id(sqlite3 *db, const char *space_uuid, char **space_parent_id);
int database_set_space_parent_id(sqlite3 *db, const char *space_uuid, const char *parent_uuid);
int database_get_amount_of_elements(sqlite3 *db, const char *space_uuid);

// This fills-in model state from DB
int database_load_space(sqlite3 *db, Model* model);

typedef struct {
    char *element_uuid;
    char *text_content;
    char *space_uuid;
    char *space_name;
} SearchResult;

int database_search_elements(sqlite3 *db, const char *search_term, GList **results);
void database_free_search_result(SearchResult *result);

typedef struct {
    char *uuid;
    char *name;
    char *created_at;
} SpaceInfo;

int database_get_all_spaces(sqlite3 *db, GList **spaces);
void database_free_space_info(SpaceInfo *space);

int database_load_video_data(sqlite3 *db, int video_id, unsigned char **video_data, int *video_size);

// Space background and grid operations
int database_get_space_background(sqlite3 *db, const char *space_uuid, char **background_color);
int database_set_space_background_color(sqlite3 *db, const char *space_uuid, const char *background_color);
int database_get_space_grid_settings(sqlite3 *db, const char *space_uuid, int *grid_enabled, char **grid_color);
int database_set_space_grid_settings(sqlite3 *db, const char *space_uuid, int grid_enabled, const char *grid_color);
int database_get_setting(sqlite3 *db, const char *key, char **value_out);
int database_set_setting(sqlite3 *db, const char *key, const char *value);
int database_insert_action_log(sqlite3 *db, const char *origin, const char *prompt, const char *dsl, const char *error_text);

#endif
