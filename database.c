#include "model.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

int database_init(sqlite3 **db, const char *filename) {
  int rc = sqlite3_open(filename, db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(*db));
    sqlite3_close(*db);
    return 0;
  }

  sqlite3_exec(*db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

  if (!database_create_tables(*db)) {
    sqlite3_close(*db);
    return 0;
  }

  if (!database_init_default_namespace(*db)) {
    sqlite3_close(*db);
    return 0;
  }

  return 1;
}

void database_close(sqlite3 *db) { sqlite3_close(db); }
 
int database_create_tables(sqlite3 *db) {
  char *err_msg = NULL;
  const char *sql =
    // Spaces table
    "CREATE TABLE IF NOT EXISTS spaces ("
    "    uuid TEXT PRIMARY KEY,"  // UUID
    "    name TEXT NOT NULL,"
    "    parent_uuid TEXT,"       // UUID of parent space
    "    is_current BOOLEAN DEFAULT 0,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "    FOREIGN KEY (parent_uuid) REFERENCES spaces(uuid)"
    ");"

    // Property reference tables
    "CREATE TABLE IF NOT EXISTS element_type_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    type INTEGER NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS position_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    x INTEGER NOT NULL,"
    "    y INTEGER NOT NULL,"
    "    z INTEGER NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS size_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    width INTEGER NOT NULL,"
    "    height INTEGER NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS text_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    text TEXT NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS color_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    r REAL NOT NULL,"
    "    g REAL NOT NULL,"
    "    b REAL NOT NULL,"
    "    a REAL NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    // Elements table
    "CREATE TABLE IF NOT EXISTS elements ("
    "    uuid TEXT PRIMARY KEY,"  // UUID
    "    space_uuid TEXT NOT NULL,"  // UUID of the space this element belongs to
    "    type_id INTEGER NOT NULL,"  // Reference to element_type_refs
    "    position_id INTEGER NOT NULL,"
    "    size_id INTEGER NOT NULL,"
    "    text_id INTEGER,"
    "    color_id INTEGER,"
    "    from_element_uuid TEXT,"    // UUID of the source element for connections
    "    to_element_uuid TEXT,"      // UUID of the target element for connections
    "    from_point INTEGER,"        // For connections 0,1,2,3 (connection point location)
    "    to_point INTEGER,"          // For connections 0,1,2,3 (connection point location)
    "    target_space_uuid TEXT,"    // For space elements, UUID of the target space
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "    FOREIGN KEY (space_uuid) REFERENCES spaces(uuid),"
    "    FOREIGN KEY (type_id) REFERENCES element_type_refs(id),"
    "    FOREIGN KEY (position_id) REFERENCES position_refs(id),"
    "    FOREIGN KEY (size_id) REFERENCES size_refs(id),"
    "    FOREIGN KEY (text_id) REFERENCES text_refs(id),"
    "    FOREIGN KEY (color_id) REFERENCES color_refs(id),"
    "    FOREIGN KEY (from_element_uuid) REFERENCES elements(uuid),"
    "    FOREIGN KEY (to_element_uuid) REFERENCES elements(uuid),"
    "    FOREIGN KEY (target_space_uuid) REFERENCES spaces(uuid)"
    ");";

  int rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }

  return 1;
}

int database_init_default_namespace(sqlite3 *db) {
  char *err_msg = NULL;
  sqlite3_stmt *stmt;

  // Check if any space is marked as current using the existing function
  char *current_space_uuid = NULL;
  if (database_get_current_space_uuid(db, &current_space_uuid)) {
    // We found a current space, no need to initialize
    g_free(current_space_uuid);
    return 1;
  }

  // Check if any spaces exist at all
  const char *sql = "SELECT COUNT(*) FROM spaces";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int count_total = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count_total = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);

  if (count_total == 0) {
    // Create default space using the existing function
    char *default_space_uuid = NULL;
    if (!database_create_space(db, "Default Space", NULL, &default_space_uuid)) {
      return 0;
    }

    // Set the newly created space as current
    if (!database_set_current_space_uuid(db, default_space_uuid)) {
      g_free(default_space_uuid);
      return 0;
    }

    g_free(default_space_uuid);
  } else {
    // Set the first existing space as current if none is marked
    sql = "UPDATE spaces SET is_current = 1 WHERE uuid = (SELECT uuid FROM spaces LIMIT 1)";
    if (sqlite3_exec(db, sql, NULL, 0, &err_msg) != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);
      return 0;
    }
  }

  return 1;
}

void database_generate_uuid(char **uuid_str) {
  uuid_t uuid;
  uuid_generate(uuid);

  char *buf = g_malloc(37); // 36 characters + null terminator
  uuid_unparse(uuid, buf);
  *uuid_str = buf;
}

int database_is_valid_uuid(const char *uuid_str) {
  uuid_t uuid;
  return uuid_parse(uuid_str, uuid) == 0 ? 1 : 0;
}

// Text reference operations
int database_create_text_ref(sqlite3 *db, const char *text, int *text_id) {
  const char *sql = "INSERT INTO text_refs (text) VALUES (?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, text, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create text: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *text_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_read_text_ref(sqlite3 *db, int text_id, ModelText **text) {
  // Initialize output to NULL
  *text = NULL;

  if (text_id <= 0) {
    fprintf(stderr, "Error: Invalid text_id (%d) in database_read_text_ref\n", text_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT text, ref_count FROM text_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, text_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelText *model_text = g_new0(ModelText, 1);
    model_text->id = text_id;
    model_text->text = g_strdup((const char*)sqlite3_column_text(stmt, 0));
    model_text->ref_count = sqlite3_column_int(stmt, 1);

    *text = model_text;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Text not found, but this is not an error - *text remains NULL
  return 1; // Success (no error occurred)
}

// Type reference operations
int database_create_type_ref(sqlite3 *db, ElementType element_type, int *type_id) {
  const char *sql = "INSERT INTO element_type_refs (type) VALUES (?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, element_type);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create type: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  // If the type already existed, we need to get its ID
  if (sqlite3_changes(db) == 0) {
    sqlite3_finalize(stmt);
    const char *select_sql = "SELECT id FROM element_type_refs WHERE type = ?";
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK) {
      fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
      return 0;
    }
    sqlite3_bind_int(stmt, 1, element_type);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      *type_id = sqlite3_column_int(stmt, 0);
    } else {
      fprintf(stderr, "Failed to find type: %d\n", element_type);
      sqlite3_finalize(stmt);
      return 0;
    }
  } else {
    *type_id = sqlite3_last_insert_rowid(db);
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_read_type_ref(sqlite3 *db, int type_id, ModelType **type) {
  // Initialize output to NULL
  *type = NULL;

  if (type_id <= 0) {
    fprintf(stderr, "Error: Invalid type_id (%d) in database_read_type_ref\n", type_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT type, ref_count FROM element_type_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, type_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelType *model_type = g_new0(ModelType, 1);
    model_type->id = type_id;
    model_type->type = sqlite3_column_int(stmt, 0);
    model_type->ref_count = sqlite3_column_int(stmt, 1);

    *type = model_type;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Type not found, but this is not an error - *type remains NULL
  return 1; // Success (no error occurred)
}

int database_read_size_ref(sqlite3 *db, int size_id, ModelSize **size) {
  // Initialize output to NULL
  *size = NULL;

  if (size_id <= 0) {
    fprintf(stderr, "Error: Invalid size_id (%d) in database_read_size_ref\n", size_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT width, height, ref_count FROM size_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, size_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelSize *model_size = g_new0(ModelSize, 1);
    model_size->id = size_id;
    model_size->width = sqlite3_column_int(stmt, 0);
    model_size->height = sqlite3_column_int(stmt, 1);
    model_size->ref_count = sqlite3_column_int(stmt, 2);

    *size = model_size;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Size not found, but this is not an error - *size remains NULL
  return 1; // Success (no error occurred)
}

int database_read_position_ref(sqlite3 *db, int position_id, ModelPosition **position) {
  // Initialize output to NULL
  *position = NULL;

  if (position_id <= 0) {
    fprintf(stderr, "Error: Invalid position_id (%d) in database_read_position_ref\n", position_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT x, y, z, ref_count FROM position_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, position_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelPosition *model_position = g_new0(ModelPosition, 1);
    model_position->id = position_id;
    model_position->x = sqlite3_column_int(stmt, 0);
    model_position->y = sqlite3_column_int(stmt, 1);
    model_position->z = sqlite3_column_int(stmt, 2);
    model_position->ref_count = sqlite3_column_int(stmt, 3);

    *position = model_position;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Position not found, but this is not an error - *position remains NULL
  return 1; // Success (no error occurred)
}

int database_read_color_ref(sqlite3 *db, int color_id, ModelColor **color) {
  // Initialize output to NULL
  *color = NULL;

  if (color_id <= 0) {
    fprintf(stderr, "Error: Invalid color_id (%d) in database_read_color_ref\n", color_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT r, g, b, a, ref_count FROM color_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, color_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelColor *model_color = g_new0(ModelColor, 1);
    model_color->id = color_id;
    model_color->r = sqlite3_column_double(stmt, 0);
    model_color->g = sqlite3_column_double(stmt, 1);
    model_color->b = sqlite3_column_double(stmt, 2);
    model_color->a = sqlite3_column_double(stmt, 3);
    model_color->ref_count = sqlite3_column_int(stmt, 4);

    *color = model_color;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Color not found, but this is not an error - *color remains NULL
  return 1; // Success (no error occurred)
}

// Position reference operations
int database_create_position_ref(sqlite3 *db, int x, int y, int z, int *position_id) {
  const char *sql = "INSERT INTO position_refs (x, y, z) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, x);
  sqlite3_bind_int(stmt, 2, y);
  sqlite3_bind_int(stmt, 3, z);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create position: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *position_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

// Size reference operations
int database_create_size_ref(sqlite3 *db, int width, int height, int *size_id) {
  const char *sql = "INSERT INTO size_refs (width, height) VALUES (?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, width);
  sqlite3_bind_int(stmt, 2, height);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create size: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *size_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

// Color reference operations
int database_create_color_ref(sqlite3 *db, double r, double g, double b, double a, int *color_id) {
  const char *sql = "INSERT INTO color_refs (r, g, b, a) VALUES (?, ?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_double(stmt, 1, r);
  sqlite3_bind_double(stmt, 2, g);
  sqlite3_bind_double(stmt, 3, b);
  sqlite3_bind_double(stmt, 4, a);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create color: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *color_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_create_element(sqlite3 *db, const char *space_uuid, ModelElement *element) {
  int type_id, position_id, size_id, text_id = 0, color_id = 0;

  // Handle type reference
  if (element->type->id == -1) {
    // Create new type reference
    if (!database_create_type_ref(db, element->type->type, &type_id)) return 0;
    element->type->id = type_id;
  } else {
    // Update existing type reference
    if (!database_update_type_ref(db, element->type)) return 0;
    type_id = element->type->id;
  }

  // Handle position reference
  if (element->position->id == -1) {
    // Create new position reference
    if (!database_create_position_ref(db, element->position->x, element->position->y, element->position->z, &position_id)) return 0;
    element->position->id = position_id;
  } else {
    // Update existing position reference
    if (!database_update_position_ref(db, element->position)) return 0;
    position_id = element->position->id;
  }

  // Handle size reference
  if (element->size->id == -1) {
    // Create new size reference
    if (!database_create_size_ref(db, element->size->width, element->size->height, &size_id)) return 0;
    element->size->id = size_id;
  } else {
    // Update existing size reference
    if (!database_update_size_ref(db, element->size)) return 0;
    size_id = element->size->id;
  }

  // Handle text reference (optional)
  if (element->text) {
    if (element->text->id == -1) {
      // Create new text reference
      if (!database_create_text_ref(db, element->text->text, &text_id)) return 0;
      element->text->id = text_id;
    } else {
      // Update existing text reference
      if (!database_update_text_ref(db, element->text)) return 0;
      text_id = element->text->id;
    }
  }

  // Handle color reference (optional)
  if (element->color) {
    if (element->color->id == -1) {
      // Create new color reference
      if (!database_create_color_ref(db, element->color->r, element->color->g, element->color->b, element->color->a, &color_id)) return 0;
      element->color->id = color_id;
    } else {
      // Update existing color reference
      if (!database_update_color_ref(db, element->color)) return 0;
      color_id = element->color->id;
    }
  }

  if (!element->uuid) {
    fprintf(stderr, "database_create_element: uuid should not be NULL while creating element");
    return 0;
  }

  const char *sql = "INSERT INTO elements (uuid, space_uuid, type_id, position_id, size_id, text_id, color_id, from_element_uuid, to_element_uuid, from_point, to_point, target_space_uuid) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;
  sqlite3_bind_text(stmt, param_index++, element->uuid, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, param_index++, space_uuid, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param_index++, type_id);
  sqlite3_bind_int(stmt, param_index++, position_id);
  sqlite3_bind_int(stmt, param_index++, size_id);

  if (text_id > 0) {
    sqlite3_bind_int(stmt, param_index++, text_id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (color_id > 0) {
    sqlite3_bind_int(stmt, param_index++, color_id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->from_element_uuid && database_is_valid_uuid(element->from_element_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->from_element_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->to_element_uuid && database_is_valid_uuid(element->to_element_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->to_element_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  sqlite3_bind_int(stmt, param_index++, element->from_point);
  sqlite3_bind_int(stmt, param_index++, element->to_point);

  if (element->target_space_uuid && database_is_valid_uuid(element->target_space_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->target_space_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create element: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);

  return 1;
}

int database_read_element(sqlite3 *db, const char *element_uuid, ModelElement **element) {
  const char *sql = "SELECT type_id, position_id, size_id, text_id, color_id, "
    "from_element_uuid, to_element_uuid, from_point, to_point, target_space_uuid, space_uuid "
    "FROM elements WHERE uuid = ?";
  sqlite3_stmt *stmt;

  // Initialize output to NULL in case of error or not found
  *element = NULL;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_text(stmt, 1, element_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelElement *elem = g_new0(ModelElement, 1);
    elem->uuid = g_strdup(element_uuid);

    int col = 0;

    // Read type
    int type_id = sqlite3_column_int(stmt, col++);
    if (!database_read_type_ref(db, type_id, &elem->type)) {
      sqlite3_finalize(stmt);
      model_element_free(elem);
      return 0; // Error
    }

    // Read position
    int position_id = sqlite3_column_int(stmt, col++);
    if (!database_read_position_ref(db, position_id, &elem->position)) {
      sqlite3_finalize(stmt);
      model_element_free(elem);
      return 0; // Error
    }

    // Read size
    int size_id = sqlite3_column_int(stmt, col++);
    if (!database_read_size_ref(db, size_id, &elem->size)) {
      sqlite3_finalize(stmt);
      model_element_free(elem);
      return 0; // Error
    }

    // Read text
    int text_id = sqlite3_column_int(stmt, col++);
    if (text_id > 0) {
      if (!database_read_text_ref(db, text_id, &elem->text)) {
        sqlite3_finalize(stmt);
        model_element_free(elem);
        return 0; // Error
      }
    }

    // Read color
    int color_id = sqlite3_column_int(stmt, col++);
    if (color_id > 0) {
      if (!database_read_color_ref(db, color_id, &elem->color)) {
        sqlite3_finalize(stmt);
        model_element_free(elem);
        return 0; // Error
      }
    }

    // Read connection data
    const char *from_uuid = (const char*)sqlite3_column_text(stmt, col++);
    if (from_uuid) {
      elem->from_element_uuid = g_strdup(from_uuid);
    }

    const char *to_uuid = (const char*)sqlite3_column_text(stmt, col++);
    if (to_uuid) {
      elem->to_element_uuid = g_strdup(to_uuid);
    }

    elem->from_point = sqlite3_column_int(stmt, col++);
    elem->to_point = sqlite3_column_int(stmt, col++);

    const char *target_uuid = (const char*)sqlite3_column_text(stmt, col++);
    if (target_uuid) {
      elem->target_space_uuid = g_strdup(target_uuid);
    }

    const char *space_uuid = (const char*)sqlite3_column_text(stmt, col++);
    if (space_uuid) {
      elem->space_uuid = g_strdup(space_uuid);
    }

    *element = elem;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Element not found, but this is not an error - *element remains NULL
  return 1; // Success (no error occurred)
}

int database_update_element(sqlite3 *db, const char *element_uuid, const ModelElement *element) {
  if (element->type && element->type->id > 0) {
    if (!database_update_type_ref(db, element->type)) {
      fprintf(stderr, "Failed to update type ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->position && element->position->id > 0) {
    if (!database_update_position_ref(db, element->position)) {
      fprintf(stderr, "Failed to update position ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->size && element->size->id > 0) {
    if (!database_update_size_ref(db, element->size)) {
      fprintf(stderr, "Failed to update size ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->text && element->text->text && element->text->id > 0) {
    if (!database_update_text_ref(db, element->text)) {
      fprintf(stderr, "Failed to update text ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->color && element->color->id > 0) {
    if (!database_update_color_ref(db, element->color)) {
      fprintf(stderr, "Failed to update color ref for element %s\n", element->uuid);
      return 0;
    }
  }

  // Update the element record with all reference IDs
  const char *sql = "UPDATE elements SET "
    "type_id = ?, position_id = ?, size_id = ?, "
    "text_id = ?, color_id = ?, "
    "from_element_uuid = ?, to_element_uuid = ?, "
    "from_point = ?, to_point = ?, target_space_uuid = ? "
    "WHERE uuid = ?";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;

  // Bind reference IDs (required references should always be present)
  sqlite3_bind_int(stmt, param_index++, element->type->id);
  sqlite3_bind_int(stmt, param_index++, element->position->id);
  sqlite3_bind_int(stmt, param_index++, element->size->id);

  // Optional references
  if (element->text && element->text->id > 0) {
    sqlite3_bind_int(stmt, param_index++, element->text->id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->color && element->color->id > 0) {
    sqlite3_bind_int(stmt, param_index++, element->color->id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  // Handle connection properties
  if (element->from_element_uuid && database_is_valid_uuid(element->from_element_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->from_element_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->to_element_uuid && database_is_valid_uuid(element->to_element_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->to_element_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  sqlite3_bind_int(stmt, param_index++, element->from_point);
  sqlite3_bind_int(stmt, param_index++, element->to_point);

  // Handle target_space_uuid
  if (element->target_space_uuid && database_is_valid_uuid(element->target_space_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->target_space_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  // Where clause
  sqlite3_bind_text(stmt, param_index++, element_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update element: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_create_space(sqlite3 *db, const char *name, const char *parent_uuid, char **space_uuid) {
  // Generate UUID for the space
  database_generate_uuid(space_uuid);

  const char *sql = "INSERT INTO spaces (uuid, name, parent_uuid) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    g_free(*space_uuid);
    *space_uuid = NULL;
    return 0;
  }

  sqlite3_bind_text(stmt, 1, *space_uuid, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

  if (parent_uuid && database_is_valid_uuid(parent_uuid)) {
    sqlite3_bind_text(stmt, 3, parent_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 3);
  }

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create space: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    g_free(*space_uuid);
    *space_uuid = NULL;
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_get_current_space_uuid(sqlite3 *db, char **space_uuid) {
  const char *sql = "SELECT uuid FROM spaces WHERE is_current = 1 LIMIT 1";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *uuid = (const char*)sqlite3_column_text(stmt, 0);
    *space_uuid = g_strdup(uuid);
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_set_current_space_uuid(sqlite3 *db, const char *space_uuid) {
  // First, clear any current space
  const char *sql = "UPDATE spaces SET is_current = 0";
  char *err_msg = NULL;

  if (sqlite3_exec(db, sql, NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }

  // Set the new current space
  sql = "UPDATE spaces SET is_current = 1 WHERE uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to set current space: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_load_space(sqlite3 *db, Model *model) {
  const char *sql =
    "SELECT e.uuid, e.type_id, e.position_id, e.size_id, e.text_id, e.color_id, "
    "e.from_element_uuid, e.to_element_uuid, e.from_point, e.to_point, e.target_space_uuid, e.space_uuid "
    "FROM elements e "
    "WHERE e.space_uuid = ?";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, model->current_space_uuid, -1, SQLITE_STATIC);

  // Define column indices for better readability and maintainability
  enum {
    COL_UUID = 0,
    COL_TYPE_ID,
    COL_POSITION_ID,
    COL_SIZE_ID,
    COL_TEXT_ID,
    COL_COLOR_ID,
    COL_FROM_ELEMENT_UUID,
    COL_TO_ELEMENT_UUID,
    COL_FROM_POINT,
    COL_TO_POINT,
    COL_TARGET_SPACE_UUID,
    COL_SPACE_UUID,
  };

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelElement *element = g_new0(ModelElement, 1);

    // Extract UUID
    const char *uuid = (const char*)sqlite3_column_text(stmt, COL_UUID);
    element->uuid = g_strdup(uuid);

    // Set state to SAVED since we're loading from database
    element->state = MODEL_STATE_SAVED;

    // Extract type (check if already loaded in model)
    int type_id = sqlite3_column_int(stmt, COL_TYPE_ID);
    if (type_id > 0) {
      ModelType *type = g_hash_table_lookup(model->types, GINT_TO_POINTER(type_id));
      if (!type) {
        // Not in cache, load from database
        if (database_read_type_ref(db, type_id, &type)) {
          g_hash_table_insert(model->types, GINT_TO_POINTER(type_id), type);
        } else {
          fprintf(stderr, "Failed to load type %d for element %s\n", type_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->type = type;
    }

    // Extract position (check if already loaded in model)
    int position_id = sqlite3_column_int(stmt, COL_POSITION_ID);
    if (position_id > 0) {
      ModelPosition *position = g_hash_table_lookup(model->positions, GINT_TO_POINTER(position_id));
      if (!position) {
        // Not in cache, load from database
        if (database_read_position_ref(db, position_id, &position)) {
          g_hash_table_insert(model->positions, GINT_TO_POINTER(position_id), position);
        } else {
          fprintf(stderr, "Failed to load position %d for element %s\n", position_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->position = position;
    }

    // Extract size (check if already loaded in model)
    int size_id = sqlite3_column_int(stmt, COL_SIZE_ID);
    if (size_id > 0) {
      ModelSize *size = g_hash_table_lookup(model->sizes, GINT_TO_POINTER(size_id));
      if (!size) {
        // Not in cache, load from database
        if (database_read_size_ref(db, size_id, &size)) {
          g_hash_table_insert(model->sizes, GINT_TO_POINTER(size_id), size);
        } else {
          fprintf(stderr, "Failed to load size %d for element %s\n", size_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->size = size;
    }

    // Extract text (check if already loaded in model)
    int text_id = sqlite3_column_int(stmt, COL_TEXT_ID);
    if (text_id > 0) {
      ModelText *text = g_hash_table_lookup(model->texts, GINT_TO_POINTER(text_id));
      if (!text) {
        // Not in cache, load from database
        if (database_read_text_ref(db, text_id, &text)) {
          g_hash_table_insert(model->texts, GINT_TO_POINTER(text_id), text);
        } else {
          fprintf(stderr, "Failed to load text %d for element %s\n", text_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->text = text;
    }

    // Extract color (check if already loaded in model)
    int color_id = sqlite3_column_int(stmt, COL_COLOR_ID);
    if (color_id > 0) {
      ModelColor *color = g_hash_table_lookup(model->colors, GINT_TO_POINTER(color_id));
      if (!color) {
        // Not in cache, load from database
        if (database_read_color_ref(db, color_id, &color)) {
          g_hash_table_insert(model->colors, GINT_TO_POINTER(color_id), color);
        } else {
          fprintf(stderr, "Failed to load color %d for element %s\n", color_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->color = color;
    }

    // Extract connection data
    const char *from_uuid = (const char*)sqlite3_column_text(stmt, COL_FROM_ELEMENT_UUID);
    if (from_uuid) {
      element->from_element_uuid = g_strdup(from_uuid);
    }

    const char *to_uuid = (const char*)sqlite3_column_text(stmt, COL_TO_ELEMENT_UUID);
    if (to_uuid) {
      element->to_element_uuid = g_strdup(to_uuid);
    }

    element->from_point = sqlite3_column_int(stmt, COL_FROM_POINT);
    element->to_point = sqlite3_column_int(stmt, COL_TO_POINT);

    // Extract target space UUID
    const char *target_uuid = (const char*)sqlite3_column_text(stmt, COL_TARGET_SPACE_UUID);
    if (target_uuid) {
      element->target_space_uuid = g_strdup(target_uuid);
    }

    // Extract space UUID
    const char *space_uuid = (const char*)sqlite3_column_text(stmt, COL_SPACE_UUID);
    if (space_uuid) {
      element->space_uuid = g_strdup(space_uuid);
    }

    element->visual_element = NULL;

    g_hash_table_insert(model->elements, g_strdup(uuid), element);
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_get_space_name(sqlite3 *db, const char *space_uuid, char **space_name) {
  const char *sql = "SELECT name FROM spaces WHERE uuid = ? LIMIT 1";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *name = (const char*)sqlite3_column_text(stmt, 0);
    *space_name = g_strdup(name);
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_get_space_parent_id(sqlite3 *db, const char *space_uuid, char **space_parent_id) {
  const char *sql = "SELECT parent_uuid FROM spaces WHERE uuid = ? LIMIT 1";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *parent_uuid = (const char*)sqlite3_column_text(stmt, 0);
    if (parent_uuid != NULL) {
      *space_parent_id = g_strdup(parent_uuid);
    } else {
      *space_parent_id = NULL;
    }
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

// Remove references with ref_count < 1 from database and return total rows deleted
// Remove references with ref_count < 1 from database and return total rows deleted
int cleanup_database_references(sqlite3 *db) {
  int total_deleted = 0;
  char *err_msg = NULL;

  // Use a single transaction for all cleanup operations
  if (sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }

  const char *tables[] = {
    "element_type_refs",
    "position_refs",
    "size_refs",
    "text_refs",
    "color_refs",
    NULL
  };

  for (int i = 0; tables[i] != NULL; i++) {
    // Build the SQL statement dynamically
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE ref_count < 1", tables[i]);

    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
      fprintf(stderr, "Failed to cleanup %s: %s\n", tables[i], err_msg);
      sqlite3_free(err_msg);
      err_msg = NULL;
      continue; // Continue with other tables
    }

    total_deleted += sqlite3_changes(db);
  }

  // Commit the transaction
  if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to commit transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return 0;
  }

  return total_deleted;
}

int database_delete_element(sqlite3 *db, const char *element_uuid) {
  if (!element_uuid || !database_is_valid_uuid(element_uuid)) {
    fprintf(stderr, "Error: Invalid element UUID in database_delete_element\n");
    return 0;
  }

  const char *sql = "DELETE FROM elements WHERE uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare delete statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, element_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to delete element: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Type reference update with ModelType struct
int database_update_type_ref(sqlite3 *db, ModelType *type) {
  if (type->id <= 0) {
    fprintf(stderr, "Error: Invalid type_id (%d) in database_update_type_ref\n", type->id);
    return 0;
  }

  const char *sql = "UPDATE element_type_refs SET type = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, type->type);
  sqlite3_bind_int(stmt, 2, type->ref_count);
  sqlite3_bind_int(stmt, 3, type->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update type: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Position reference update with ModelPosition struct
int database_update_position_ref(sqlite3 *db, ModelPosition *position) {
  if (position->id <= 0) {
    fprintf(stderr, "Error: Invalid position_id (%d) in database_update_position_ref\n", position->id);
    return 0;
  }

  const char *sql = "UPDATE position_refs SET x = ?, y = ?, z = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, position->x);
  sqlite3_bind_int(stmt, 2, position->y);
  sqlite3_bind_int(stmt, 3, position->z);
  sqlite3_bind_int(stmt, 4, position->ref_count);
  sqlite3_bind_int(stmt, 5, position->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update position: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Size reference update with ModelSize struct
int database_update_size_ref(sqlite3 *db, ModelSize *size) {
  if (size->id <= 0) {
    fprintf(stderr, "Error: Invalid size_id (%d) in database_update_size_ref\n", size->id);
    return 0;
  }

  const char *sql = "UPDATE size_refs SET width = ?, height = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, size->width);
  sqlite3_bind_int(stmt, 2, size->height);
  sqlite3_bind_int(stmt, 3, size->ref_count);
  sqlite3_bind_int(stmt, 4, size->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update size: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Text reference update with ModelText struct
int database_update_text_ref(sqlite3 *db, ModelText *text) {
  if (text->id <= 0) {
    fprintf(stderr, "Error: Invalid text_id (%d) in database_update_text_ref\n", text->id);
    return 0;
  }

  const char *sql = "UPDATE text_refs SET text = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, text->text, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, text->ref_count);
  sqlite3_bind_int(stmt, 3, text->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update text: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Color reference update with ModelColor struct
int database_update_color_ref(sqlite3 *db, ModelColor *color) {
  if (color->id <= 0) {
    fprintf(stderr, "Error: Invalid color_id (%d) in database_update_color_ref\n", color->id);
    return 0;
  }

  const char *sql = "UPDATE color_refs SET r = ?, g = ?, b = ?, a = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_double(stmt, 1, color->r);
  sqlite3_bind_double(stmt, 2, color->g);
  sqlite3_bind_double(stmt, 3, color->b);
  sqlite3_bind_double(stmt, 4, color->a);
  sqlite3_bind_int(stmt, 5, color->ref_count);
  sqlite3_bind_int(stmt, 6, color->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update color: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}
