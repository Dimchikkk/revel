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
int database_create_text_ref(sqlite3 *db, const char *text, int *text_id);
int database_read_text_ref(sqlite3 *db, int text_id, ModelText **text);
int database_update_text_ref(sqlite3 *db, ModelText *text);

// Color reference operations
int database_create_color_ref(sqlite3 *db, double r, double g, double b, double a, int *color_id);
int database_read_color_ref(sqlite3 *db, int color_id, ModelColor **color);
int database_update_color_ref(sqlite3 *db, ModelColor *color);

int cleanup_database_references(sqlite3 *db);

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
int database_get_amount_of_elements(sqlite3 *db, const char *space_uuid);

// This fills-in model state from DB
int database_load_space(sqlite3 *db, Model* model);

#endif
