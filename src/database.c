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

  // Performance optimizations for faster loading
  sqlite3_exec(*db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
  sqlite3_exec(*db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
  sqlite3_exec(*db, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
  sqlite3_exec(*db, "PRAGMA cache_size = -64000;", NULL, NULL, NULL);  // 64MB cache
  sqlite3_exec(*db, "PRAGMA temp_store = MEMORY;", NULL, NULL, NULL);

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
    "    uuid TEXT PRIMARY KEY,"                   // UUID
    "    name TEXT NOT NULL,"
    "    parent_uuid TEXT,"                        // UUID of parent space
    "    is_current BOOLEAN DEFAULT 0,"
    "    background_color TEXT DEFAULT '#181818'," // Background color in hex format
    "    grid_enabled BOOLEAN DEFAULT 1,"          // Whether grid is enabled
    "    grid_color TEXT DEFAULT '#26262666',"     // Grid color in hex format
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

    "CREATE TABLE IF NOT EXISTS video_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    thumbnail_data BLOB NOT NULL,"      // Thumbnail image data
    "    thumbnail_size INTEGER NOT NULL,"   // Thumbnail size in bytes
    "    video_data BLOB NOT NULL,"          // Video file data (stored as BLOB)
    "    video_size INTEGER NOT NULL,"       // Video file size in bytes
    "    duration INTEGER,"                  // Video duration in seconds
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    "CREATE TABLE IF NOT EXISTS audio_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    audio_data BLOB NOT NULL,"          // Audio file data (stored as BLOB)
    "    audio_size INTEGER NOT NULL,"       // Audio file size in bytes
    "    duration INTEGER,"                  // Audio duration in seconds
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
    "    text_r REAL NOT NULL DEFAULT 0.1,"
    "    text_g REAL NOT NULL DEFAULT 0.1,"
    "    text_b REAL NOT NULL DEFAULT 0.1,"
    "    text_a REAL NOT NULL DEFAULT 1.0,"
    "    font_description TEXT DEFAULT 'Ubuntu Mono Bold 16',"
    "    strikethrough BOOLEAN DEFAULT 0,"
    "    alignment TEXT DEFAULT 'center',"
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

    "CREATE TABLE IF NOT EXISTS image_refs ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    image_data BLOB NOT NULL,"
    "    image_size INTEGER NOT NULL,"
    "    ref_count INTEGER DEFAULT 1,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");"

    // Elements table
    "CREATE TABLE IF NOT EXISTS elements ("
    "    uuid TEXT PRIMARY KEY,"          // UUID
    "    space_uuid TEXT NOT NULL,"       // UUID of the space this element belongs to
    "    type_id INTEGER NOT NULL,"       // Reference to element_type_refs
    "    position_id INTEGER NOT NULL,"
    "    size_id INTEGER NOT NULL,"
    "    text_id INTEGER,"
    "    bg_color_id INTEGER,"
    "    from_element_uuid TEXT,"            // UUID of the source element for connections
    "    to_element_uuid TEXT,"              // UUID of the target element for connections
    "    from_point INTEGER,"                // For connections 0,1,2,3 (connection point location)
    "    to_point INTEGER,"                  // For connections 0,1,2,3 (connection point location)
    "    target_space_uuid TEXT,"            // For space elements, UUID of the target space
    "    image_id INTEGER,"                  // Image note related
    "    video_id INTEGER,"                  // Video note related
    "    audio_id INTEGER,"                  // Audio note related
    "    drawing_points BLOB,"               // For freehand drawings: array of points
    "    stroke_width INTEGER,"              // For freehand drawings and shapes: stroke width
    "    shape_type INTEGER,"                // For shapes: type (circle, rectangle, triangle)
    "    filled INTEGER,"                    // For shapes: whether shape is filled (boolean)
    "    stroke_style INTEGER DEFAULT 0,"    // For shapes: stroke style (solid=0, dashed=1, dotted=2)
    "    fill_style INTEGER DEFAULT 0,"      // For shapes: fill style (solid=0, hachure=1, cross-hatch=2)
    "    stroke_color TEXT,"                 // For shapes: stroke color in hex format (separate from bg_color)
    "    connection_type INTEGER,"           // For connections: parallel, straight, curved
    "    arrowhead_type INTEGER,"            // For connections: none, single, double
    "    rotation_degrees REAL DEFAULT 0.0," // Rotation angle in degrees (0-360)
    "    description TEXT,"                  // Element description/comment
    "    locked INTEGER NOT NULL DEFAULT 0," // Whether element is locked (non-interactable except context menu)
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
    "    FOREIGN KEY (space_uuid) REFERENCES spaces(uuid),"
    "    FOREIGN KEY (type_id) REFERENCES element_type_refs(id),"
    "    FOREIGN KEY (position_id) REFERENCES position_refs(id),"
    "    FOREIGN KEY (size_id) REFERENCES size_refs(id),"
    "    FOREIGN KEY (text_id) REFERENCES text_refs(id),"
    "    FOREIGN KEY (bg_color_id) REFERENCES color_refs(id),"
    "    FOREIGN KEY (image_id) REFERENCES image_refs(id),"
    "    FOREIGN KEY (video_id) REFERENCES video_refs(id),"
    "    FOREIGN KEY (audio_id) REFERENCES audio_refs(id),"
    "    FOREIGN KEY (from_element_uuid) REFERENCES elements(uuid),"
    "    FOREIGN KEY (to_element_uuid) REFERENCES elements(uuid),"
    "    FOREIGN KEY (target_space_uuid) REFERENCES spaces(uuid)"
    ");"

    "CREATE TABLE IF NOT EXISTS app_settings ("
    "    key TEXT PRIMARY KEY,"
    "    value TEXT"
    ");"

    "CREATE TABLE IF NOT EXISTS action_log ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    origin TEXT NOT NULL,"
    "    prompt TEXT,"
    "    dsl TEXT,"
    "    error TEXT,"
    "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
    ");";

  int rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }

  const char *fts_sql =
    "CREATE VIRTUAL TABLE IF NOT EXISTS element_text_fts USING fts5("
    "    element_uuid,"
    "    space_uuid,"
    "    text_content,"
    "    tokenize = 'porter'"
    ");"

    "CREATE TRIGGER IF NOT EXISTS text_refs_after_update AFTER UPDATE ON text_refs "
    "WHEN OLD.text != NEW.text "
    "BEGIN"
    "    UPDATE element_text_fts "
    "    SET text_content = NEW.text "
    "    WHERE element_uuid IN ("
    "        SELECT e.uuid "
    "        FROM elements e "
    "        WHERE e.text_id = NEW.id"
    "    );"
    "END;"

    "CREATE TRIGGER IF NOT EXISTS elements_after_insert AFTER INSERT ON elements "
    "WHEN NEW.text_id IS NOT NULL "
    "BEGIN"
    "    INSERT INTO element_text_fts(element_uuid, space_uuid, text_content) "
    "    SELECT NEW.uuid, NEW.space_uuid, tr.text "
    "    FROM text_refs tr "
    "    WHERE tr.id = NEW.text_id;"
    "END;"

    "CREATE TRIGGER IF NOT EXISTS elements_after_update AFTER UPDATE ON elements "
    "WHEN NEW.text_id IS NOT NULL AND (OLD.text_id IS NULL OR OLD.text_id != NEW.text_id)"
    "BEGIN"
    "    INSERT OR REPLACE INTO element_text_fts(element_uuid, space_uuid, text_content) "
    "    SELECT NEW.uuid, NEW.space_uuid, tr.text "
    "    FROM text_refs tr "
    "    WHERE tr.id = NEW.text_id;"
    "END;"

    "CREATE TRIGGER IF NOT EXISTS elements_after_update_space AFTER UPDATE ON elements "
    "WHEN OLD.space_uuid != NEW.space_uuid AND NEW.text_id IS NOT NULL "
    "BEGIN"
    "    UPDATE element_text_fts "
    "    SET space_uuid = NEW.space_uuid "
    "    WHERE element_uuid = NEW.uuid;"
    "END;"

    "CREATE TRIGGER IF NOT EXISTS elements_after_delete AFTER DELETE ON elements "
    "BEGIN"
    "    DELETE FROM element_text_fts "
    "    WHERE element_uuid = OLD.uuid;"
    "END;";

  sqlite3_exec(db, fts_sql, NULL, NULL, NULL);

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

int database_begin_transaction(sqlite3 *db) {
  char *err_msg = NULL;
  if (sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }
  return 1;
}

int database_commit_transaction(sqlite3 *db) {
  char *err_msg = NULL;
  if (sqlite3_exec(db, "COMMIT;", NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
  }
  return 1;
}

int database_rollback_transaction(sqlite3 *db) {
  char *err_msg = NULL;
  if (sqlite3_exec(db, "ROLLBACK;", NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 0;
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

int database_create_text_ref(sqlite3 *db,
                             const char *text,
                             double text_r, double text_g, double text_b, double text_a,
                             const char *font_description,
                             gboolean strikethrough,
                             const char *alignment,
                             int *text_id) {
  const char *sql = "INSERT INTO text_refs (text, text_r, text_g, text_b, text_a, font_description, strikethrough, alignment) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, text, -1, SQLITE_STATIC);
  sqlite3_bind_double(stmt, 2, text_r);
  sqlite3_bind_double(stmt, 3, text_g);
  sqlite3_bind_double(stmt, 4, text_b);
  sqlite3_bind_double(stmt, 5, text_a);
  sqlite3_bind_text(stmt, 6, font_description, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 7, strikethrough ? 1 : 0);
  sqlite3_bind_text(stmt, 8, alignment, -1, SQLITE_STATIC);

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

  const char *sql = "SELECT text, text_r, text_g, text_b, text_a, font_description, strikethrough, alignment, ref_count FROM text_refs WHERE id = ?";
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
    model_text->r = sqlite3_column_double(stmt, 1);
    model_text->g = sqlite3_column_double(stmt, 2);
    model_text->b = sqlite3_column_double(stmt, 3);
    model_text->a = sqlite3_column_double(stmt, 4);

    const char *font_desc = (const char*)sqlite3_column_text(stmt, 5);
    if (font_desc) {
      model_text->font_description = g_strdup(font_desc);
    } else {
      model_text->font_description = g_strdup("Ubuntu Mono Bold 12"); // Default
    }

    model_text->strikethrough = sqlite3_column_int(stmt, 6);

    const char *alignment = (const char*)sqlite3_column_text(stmt, 7);
    if (alignment) {
      model_text->alignment = g_strdup(alignment);
    } else {
      model_text->alignment = NULL; // Let element type set its own default
    }

    model_text->ref_count = sqlite3_column_int(stmt, 8);

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

int database_read_color_ref(sqlite3 *db, int bg_color_id, ModelColor **color) {
  // Initialize output to NULL
  *color = NULL;

  if (bg_color_id <= 0) {
    fprintf(stderr, "Error: Invalid bg_color_id (%d) in database_read_color_ref\n", bg_color_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT r, g, b, a, ref_count FROM color_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, bg_color_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelColor *model_color = g_new0(ModelColor, 1);
    model_color->id = bg_color_id;
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
int database_create_color_ref(sqlite3 *db, double r, double g, double b, double a, int *bg_color_id) {
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

  *bg_color_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_create_element(sqlite3 *db, const char *space_uuid, ModelElement *element) {
  int type_id, position_id, size_id, text_id = 0, bg_color_id = 0, image_id = 0, video_id = 0;

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
      // Create new text reference with color and font
      if (!database_create_text_ref(db, element->text->text,
                                   element->text->r, element->text->g,
                                   element->text->b, element->text->a,
                                   element->text->font_description,
                                   element->text->strikethrough,
                                   element->text->alignment, &text_id)) return 0;
      element->text->id = text_id;
    } else {
      // Update existing text reference
      if (!database_update_text_ref(db, element->text)) return 0;
      text_id = element->text->id;
    }
  }

  if (element->image) {
    if (element->image->id == -1) {
      // Create new image reference
      if (!database_create_image_ref(db, element->image->image_data, element->image->image_size, &image_id)) return 0;
      element->image->id = image_id;
    } else {
      // Update existing image reference
      if (!database_update_image_ref(db, element->image)) return 0;
      image_id = element->image->id;
    }
  }

  if (element->video) {
    if (element->video->id == -1) {
      if (!database_create_video_ref(db,
                                     element->video->thumbnail_data, element->video->thumbnail_size,
                                     element->video->video_data, element->video->video_size,
                                     element->video->duration,
                                     &video_id)) {
        fprintf(stderr, "Failed to create video ref for element %s\n", element->uuid);
        return 0;
      }
      element->video->id = video_id;
    } else {
      if (!database_update_video_ref(db, element->video)) {
        fprintf(stderr, "Failed to update video ref for element %s\n", element->uuid);
        return 0;
      }
      video_id = element->video->id;
    }
  }

  int audio_id = -1;
  if (element->audio) {
    if (element->audio->id == -1) {
      if (!database_create_audio_ref(db,
                                     element->audio->audio_data, element->audio->audio_size,
                                     element->audio->duration,
                                     &audio_id)) {
        fprintf(stderr, "Failed to create audio ref for element %s\n", element->uuid);
        return 0;
      }
      element->audio->id = audio_id;
    } else {
      if (!database_update_audio_ref(db, element->audio)) {
        fprintf(stderr, "Failed to update audio ref for element %s\n", element->uuid);
        return 0;
      }
      audio_id = element->audio->id;
    }
  }

  // Handle color reference (optional)
  if (element->bg_color) {
    if (element->bg_color->id == -1) {
      // Create new color reference
      if (!database_create_color_ref(db, element->bg_color->r, element->bg_color->g, element->bg_color->b, element->bg_color->a, &bg_color_id)) return 0;
      element->bg_color->id = bg_color_id;
    } else {
      // Update existing color reference
      if (!database_update_color_ref(db, element->bg_color)) return 0;
      bg_color_id = element->bg_color->id;
    }
  }

  if (!element->uuid) {
    fprintf(stderr, "database_create_element: uuid should not be NULL while creating element");
    return 0;
  }

  const char *sql = "INSERT INTO elements (uuid, space_uuid, type_id, position_id, size_id, text_id, bg_color_id, from_element_uuid, to_element_uuid, from_point, to_point, target_space_uuid, image_id, video_id, audio_id, drawing_points, stroke_width, shape_type, filled, stroke_style, fill_style, stroke_color, connection_type, arrowhead_type, rotation_degrees, description, locked) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
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

  if (bg_color_id > 0) {
    sqlite3_bind_int(stmt, param_index++, bg_color_id);
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

  if (image_id > 0) {
    sqlite3_bind_int(stmt, param_index++, image_id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (video_id > 0) {
    sqlite3_bind_int(stmt, param_index++, video_id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (audio_id > 0) {
    sqlite3_bind_int(stmt, param_index++, audio_id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->drawing_points && element->drawing_points->len > 0) {
    sqlite3_bind_blob(stmt, param_index++,
                      element->drawing_points->data,
                      element->drawing_points->len * sizeof(DrawingPoint),
                      SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->stroke_width > 0) {
    sqlite3_bind_int(stmt, param_index++, element->stroke_width);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind shape_type (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->shape_type);
  // Bind filled (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->filled ? 1 : 0);
  // Bind stroke_style (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->stroke_style);
  // Bind fill_style (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->fill_style);
  // Bind stroke_color (for shapes)
  if (element->stroke_color) {
    sqlite3_bind_text(stmt, param_index++, element->stroke_color, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind connection_type (for connections)
  sqlite3_bind_int(stmt, param_index++, element->connection_type);
  // Bind arrowhead_type (for connections)
  sqlite3_bind_int(stmt, param_index++, element->arrowhead_type);
  // Bind rotation_degrees
  sqlite3_bind_double(stmt, param_index++, element->rotation_degrees);
  // Bind description
  if (element->description) {
    sqlite3_bind_text(stmt, param_index++, element->description, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind locked
  sqlite3_bind_int(stmt, param_index++, element->locked ? 1 : 0);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create element: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);

  return 1;
}

int database_read_element(sqlite3 *db, const char *element_uuid, ModelElement **element) {
  const char *sql = "SELECT type_id, position_id, size_id, text_id, bg_color_id, "
    "from_element_uuid, to_element_uuid, from_point, to_point, target_space_uuid, space_uuid, image_id, video_id, audio_id, "
    "drawing_points, stroke_width, shape_type, filled, stroke_style, fill_style, stroke_color, connection_type, arrowhead_type, rotation_degrees, description, created_at, locked "
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
    int bg_color_id = sqlite3_column_int(stmt, col++);
    if (bg_color_id > 0) {
      if (!database_read_color_ref(db, bg_color_id, &elem->bg_color)) {
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

    int image_id = sqlite3_column_int(stmt, col++);
    if (image_id > 0) {
      if (!database_read_image_ref(db, image_id, &elem->image)) {
        sqlite3_finalize(stmt);
        model_element_free(elem);
        return 0; // Error
      }
    }

    int video_id = sqlite3_column_int(stmt, col++);
    if (video_id > 0) {
      if (!database_read_video_ref(db, video_id, &elem->video)) {
        sqlite3_finalize(stmt);
        model_element_free(elem);
        return 0; // Error
      }
    }

    int audio_id = sqlite3_column_int(stmt, col++);
    if (audio_id > 0) {
      if (!database_read_audio_ref(db, audio_id, &elem->audio)) {
        sqlite3_finalize(stmt);
        model_element_free(elem);
        return 0; // Error
      }
    }

    const void *drawing_blob = sqlite3_column_blob(stmt, col);
    int drawing_blob_size = sqlite3_column_bytes(stmt, col++);

    if (drawing_blob && drawing_blob_size > 0) {
      int point_count = drawing_blob_size / sizeof(DrawingPoint);
      elem->drawing_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), point_count);
      g_array_append_vals(elem->drawing_points, drawing_blob, point_count);
    }

    elem->stroke_width = sqlite3_column_int(stmt, col++);
    elem->shape_type = sqlite3_column_int(stmt, col++);
    elem->filled = sqlite3_column_int(stmt, col++) ? TRUE : FALSE;
    elem->stroke_style = sqlite3_column_int(stmt, col++);
    elem->fill_style = sqlite3_column_int(stmt, col++);
    const char *stroke_color = (const char*)sqlite3_column_text(stmt, col++);
    if (stroke_color) {
      elem->stroke_color = g_strdup(stroke_color);
    } else {
      elem->stroke_color = NULL;
    }
    elem->connection_type = sqlite3_column_int(stmt, col++);
    elem->arrowhead_type = sqlite3_column_int(stmt, col++);
    elem->rotation_degrees = sqlite3_column_double(stmt, col++);

    // Read description
    const char *description = (const char*)sqlite3_column_text(stmt, col++);
    if (description) {
      elem->description = g_strdup(description);
    }

    // Read created_at
    const char *created_at = (const char*)sqlite3_column_text(stmt, col++);
    if (created_at) {
      elem->created_at = g_strdup(created_at);
    }

    // Read locked
    elem->locked = sqlite3_column_int(stmt, col++) ? TRUE : FALSE;

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
  if (element->bg_color && element->bg_color->id > 0) {
    if (!database_update_color_ref(db, element->bg_color)) {
      fprintf(stderr, "Failed to update color ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->image && element->image->id > 0) {
    if (!database_update_image_ref(db, element->image)) {
      fprintf(stderr, "Failed to update image ref for element %s\n", element->uuid);
      return 0;
    }
  }
  if (element->video && element->video->id > 0) {
    if (!database_update_video_ref(db, element->video)) {
      fprintf(stderr, "Failed to update video ref for element %s\n", element->uuid);
      return 0;
    }
  }

  // Update the element record with all reference IDs
  const char *sql = "UPDATE elements SET "
    "type_id = ?, position_id = ?, size_id = ?, "
    "text_id = ?, bg_color_id = ?, image_id = ?, video_id = ?, "
    "from_element_uuid = ?, to_element_uuid = ?, "
    "from_point = ?, to_point = ?, target_space_uuid = ?, "
    "space_uuid = ?, drawing_points = ?, stroke_width = ?, "
    "shape_type = ?, filled = ?, stroke_style = ?, fill_style = ?, stroke_color = ?, connection_type = ?, arrowhead_type = ?, rotation_degrees = ?, description = ?, locked = ? "
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

  if (element->bg_color && element->bg_color->id > 0) {
    sqlite3_bind_int(stmt, param_index++, element->bg_color->id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->image && element->image->id > 0) {
    sqlite3_bind_int(stmt, param_index++, element->image->id);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->video && element->video->id > 0) {
    sqlite3_bind_int(stmt, param_index++, element->video->id);
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

  if (element->target_space_uuid && database_is_valid_uuid(element->target_space_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->target_space_uuid, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->space_uuid && database_is_valid_uuid(element->space_uuid)) {
    sqlite3_bind_text(stmt, param_index++, element->space_uuid, -1, SQLITE_STATIC);
  } else {
    fprintf(stderr, "Error: space_uuid is required for element %s\n", element_uuid);
    sqlite3_finalize(stmt);
    return 0;
  }


  if (element->drawing_points && element->drawing_points->len > 0) {
    sqlite3_bind_blob(stmt, param_index++,
                      element->drawing_points->data,
                      element->drawing_points->len * sizeof(DrawingPoint),
                      SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }

  if (element->stroke_width > 0) {
    sqlite3_bind_int(stmt, param_index++, element->stroke_width);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind shape_type (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->shape_type);
  // Bind filled (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->filled ? 1 : 0);
  // Bind stroke_style (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->stroke_style);
  // Bind fill_style (for shapes)
  sqlite3_bind_int(stmt, param_index++, element->fill_style);
  // Bind stroke_color (for shapes)
  if (element->stroke_color) {
    sqlite3_bind_text(stmt, param_index++, element->stroke_color, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind connection_type (for connections)
  sqlite3_bind_int(stmt, param_index++, element->connection_type);
  // Bind arrowhead_type (for connections)
  sqlite3_bind_int(stmt, param_index++, element->arrowhead_type);
  // Bind rotation_degrees
  sqlite3_bind_double(stmt, param_index++, element->rotation_degrees);
  // Bind description
  if (element->description) {
    sqlite3_bind_text(stmt, param_index++, element->description, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, param_index++);
  }
  // Bind locked
  sqlite3_bind_int(stmt, param_index++, element->locked ? 1 : 0);

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
  // Use a read transaction for better performance when loading many elements
  char *err_msg = NULL;
  if (sqlite3_exec(db, "BEGIN DEFERRED TRANSACTION;", NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to begin read transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    // Continue without transaction - it's just an optimization
  }

  // Use JOINs to load all data in one query - much faster for bulk loading
  const char *sql =
    "SELECT e.uuid, e.type_id, e.position_id, e.size_id, e.text_id, e.bg_color_id, "
    "e.from_element_uuid, e.to_element_uuid, e.from_point, e.to_point, e.target_space_uuid, e.space_uuid, "
    "e.image_id, e.video_id, e.audio_id, "
    "e.drawing_points, e.stroke_width, e.shape_type, e.filled, e.stroke_style, e.fill_style, e.stroke_color, "
    "e.connection_type, e.arrowhead_type, e.rotation_degrees, e.description, e.created_at, e.locked, "
    "t.type, t.ref_count, "
    "p.x, p.y, p.z, p.ref_count, "
    "s.width, s.height, s.ref_count, "
    "txt.text, txt.text_r, txt.text_g, txt.text_b, txt.text_a, txt.font_description, txt.strikethrough, txt.alignment, txt.ref_count, "
    "c.r, c.g, c.b, c.a, c.ref_count "
    "FROM elements e "
    "LEFT JOIN element_type_refs t ON e.type_id = t.id "
    "LEFT JOIN position_refs p ON e.position_id = p.id "
    "LEFT JOIN size_refs s ON e.size_id = s.id "
    "LEFT JOIN text_refs txt ON e.text_id = txt.id "
    "LEFT JOIN color_refs c ON e.bg_color_id = c.id "
    "WHERE e.space_uuid = ?";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_exec(db, "ROLLBACK;", NULL, 0, NULL);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, model->current_space_uuid, -1, SQLITE_STATIC);

  // Define column indices for JOINed query
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
    COL_IMAGE_ID,
    COL_VIDEO_ID,
    COL_AUDIO_ID,
    COL_DRAWING_POINTS,
    COL_STROKE_WIDTH,
    COL_SHAPE_TYPE,
    COL_FILLED,
    COL_STROKE_STYLE,
    COL_FILL_STYLE,
    COL_STROKE_COLOR,
    COL_CONNECTION_TYPE,
    COL_ARROWHEAD_TYPE,
    COL_ROTATION_DEGREES,
    COL_DESCRIPTION,
    COL_CREATED_AT,
    COL_LOCKED,
    // JOINed data starts here
    COL_TYPE_TYPE,
    COL_TYPE_REF_COUNT,
    COL_POS_X,
    COL_POS_Y,
    COL_POS_Z,
    COL_POS_REF_COUNT,
    COL_SIZE_WIDTH,
    COL_SIZE_HEIGHT,
    COL_SIZE_REF_COUNT,
    COL_TEXT_TEXT,
    COL_TEXT_R,
    COL_TEXT_G,
    COL_TEXT_B,
    COL_TEXT_A,
    COL_TEXT_FONT,
    COL_TEXT_STRIKE,
    COL_TEXT_ALIGN,
    COL_TEXT_REF_COUNT,
    COL_COLOR_R,
    COL_COLOR_G,
    COL_COLOR_B,
    COL_COLOR_A,
    COL_COLOR_REF_COUNT,
  };

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelElement *element = g_new0(ModelElement, 1);

    // Extract UUID
    const char *uuid = (const char*)sqlite3_column_text(stmt, COL_UUID);
    element->uuid = g_strdup(uuid);

    // Set state to SAVED since we're loading from database
    element->state = MODEL_STATE_SAVED;

    // Extract type directly from JOIN
    int type_id = sqlite3_column_int(stmt, COL_TYPE_ID);
    if (type_id > 0) {
      ModelType *type = g_hash_table_lookup(model->types, GINT_TO_POINTER(type_id));
      if (!type) {
        type = g_new0(ModelType, 1);
        type->id = type_id;
        type->type = sqlite3_column_int(stmt, COL_TYPE_TYPE);
        type->ref_count = sqlite3_column_int(stmt, COL_TYPE_REF_COUNT);
        g_hash_table_insert(model->types, GINT_TO_POINTER(type_id), type);
      }
      element->type = type;
    }

    // Extract position directly from JOIN
    int position_id = sqlite3_column_int(stmt, COL_POSITION_ID);
    if (position_id > 0) {
      ModelPosition *position = g_hash_table_lookup(model->positions, GINT_TO_POINTER(position_id));
      if (!position) {
        position = g_new0(ModelPosition, 1);
        position->id = position_id;
        position->x = sqlite3_column_int(stmt, COL_POS_X);
        position->y = sqlite3_column_int(stmt, COL_POS_Y);
        position->z = sqlite3_column_int(stmt, COL_POS_Z);
        position->ref_count = sqlite3_column_int(stmt, COL_POS_REF_COUNT);
        g_hash_table_insert(model->positions, GINT_TO_POINTER(position_id), position);
      }
      element->position = position;
    }

    // Extract size directly from JOIN
    int size_id = sqlite3_column_int(stmt, COL_SIZE_ID);
    if (size_id > 0) {
      ModelSize *size = g_hash_table_lookup(model->sizes, GINT_TO_POINTER(size_id));
      if (!size) {
        size = g_new0(ModelSize, 1);
        size->id = size_id;
        size->width = sqlite3_column_int(stmt, COL_SIZE_WIDTH);
        size->height = sqlite3_column_int(stmt, COL_SIZE_HEIGHT);
        size->ref_count = sqlite3_column_int(stmt, COL_SIZE_REF_COUNT);
        g_hash_table_insert(model->sizes, GINT_TO_POINTER(size_id), size);
      }
      element->size = size;
    }

    // Extract text directly from JOIN
    int text_id = sqlite3_column_int(stmt, COL_TEXT_ID);
    if (text_id > 0) {
      ModelText *text = g_hash_table_lookup(model->texts, GINT_TO_POINTER(text_id));
      if (!text) {
        text = g_new0(ModelText, 1);
        text->id = text_id;
        const char *text_str = (const char*)sqlite3_column_text(stmt, COL_TEXT_TEXT);
        text->text = g_strdup(text_str ? text_str : "");
        text->r = sqlite3_column_double(stmt, COL_TEXT_R);
        text->g = sqlite3_column_double(stmt, COL_TEXT_G);
        text->b = sqlite3_column_double(stmt, COL_TEXT_B);
        text->a = sqlite3_column_double(stmt, COL_TEXT_A);
        const char *font = (const char*)sqlite3_column_text(stmt, COL_TEXT_FONT);
        text->font_description = g_strdup(font ? font : "Ubuntu Mono Bold 16");
        text->strikethrough = sqlite3_column_int(stmt, COL_TEXT_STRIKE) ? TRUE : FALSE;
        const char *align = (const char*)sqlite3_column_text(stmt, COL_TEXT_ALIGN);
        text->alignment = g_strdup(align ? align : "center");
        text->ref_count = sqlite3_column_int(stmt, COL_TEXT_REF_COUNT);
        g_hash_table_insert(model->texts, GINT_TO_POINTER(text_id), text);
      }
      element->text = text;
    }

    // Extract color directly from JOIN
    int bg_color_id = sqlite3_column_int(stmt, COL_COLOR_ID);
    if (bg_color_id > 0) {
      ModelColor *color = g_hash_table_lookup(model->colors, GINT_TO_POINTER(bg_color_id));
      if (!color) {
        color = g_new0(ModelColor, 1);
        color->id = bg_color_id;
        color->r = sqlite3_column_double(stmt, COL_COLOR_R);
        color->g = sqlite3_column_double(stmt, COL_COLOR_G);
        color->b = sqlite3_column_double(stmt, COL_COLOR_B);
        color->a = sqlite3_column_double(stmt, COL_COLOR_A);
        color->ref_count = sqlite3_column_int(stmt, COL_COLOR_REF_COUNT);
        g_hash_table_insert(model->colors, GINT_TO_POINTER(bg_color_id), color);
      }
      element->bg_color = color;
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

    // Extract image (check if already loaded in model)
    int image_id = sqlite3_column_int(stmt, COL_IMAGE_ID);
    if (image_id > 0) {
      ModelImage *image = g_hash_table_lookup(model->images, GINT_TO_POINTER(image_id));
      if (!image) {
        // Not in cache, load from database
        if (database_read_image_ref(db, image_id, &image)) {
          g_hash_table_insert(model->images, GINT_TO_POINTER(image_id), image);
        } else {
          fprintf(stderr, "Failed to load image %d for element %s\n", image_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->image = image;
    }

    // Extract video (check if already loaded in model)
    int video_id = sqlite3_column_int(stmt, COL_VIDEO_ID);
    if (video_id > 0) {
      ModelVideo *video = g_hash_table_lookup(model->videos, GINT_TO_POINTER(video_id));
      if (!video) {
        // Not in cache, load from database
        if (database_read_video_ref(db, video_id, &video)) {
          g_hash_table_insert(model->videos, GINT_TO_POINTER(video_id), video);
        } else {
          fprintf(stderr, "Failed to load video %d for element %s\n", video_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->video = video;
    }

    // Extract audio (check if already loaded in model)
    int audio_id = sqlite3_column_int(stmt, COL_AUDIO_ID);
    if (audio_id > 0) {
      ModelAudio *audio = g_hash_table_lookup(model->audios, GINT_TO_POINTER(audio_id));
      if (!audio) {
        // Not in cache, load from database
        if (database_read_audio_ref(db, audio_id, &audio)) {
          g_hash_table_insert(model->audios, GINT_TO_POINTER(audio_id), audio);
        } else {
          fprintf(stderr, "Failed to load audio %d for element %s\n", audio_id, uuid);
          model_element_free(element);
          continue;
        }
      }
      element->audio = audio;
    }

    const void *drawing_blob = sqlite3_column_blob(stmt, COL_DRAWING_POINTS);
    int drawing_blob_size = sqlite3_column_bytes(stmt, COL_DRAWING_POINTS);
    if (drawing_blob && drawing_blob_size > 0) {
      int point_count = drawing_blob_size / sizeof(DrawingPoint);
      element->drawing_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), point_count);
      g_array_append_vals(element->drawing_points, drawing_blob, point_count);
    }
    element->stroke_width = sqlite3_column_int(stmt, COL_STROKE_WIDTH);
    element->shape_type = sqlite3_column_int(stmt, COL_SHAPE_TYPE);
    element->filled = sqlite3_column_int(stmt, COL_FILLED) ? TRUE : FALSE;
    element->stroke_style = sqlite3_column_int(stmt, COL_STROKE_STYLE);
    element->fill_style = sqlite3_column_int(stmt, COL_FILL_STYLE);
    const char *stroke_color = (const char*)sqlite3_column_text(stmt, COL_STROKE_COLOR);
    if (stroke_color) {
      element->stroke_color = g_strdup(stroke_color);
    } else {
      element->stroke_color = NULL;
    }
    element->connection_type = sqlite3_column_int(stmt, COL_CONNECTION_TYPE);
    element->arrowhead_type = sqlite3_column_int(stmt, COL_ARROWHEAD_TYPE);
    element->rotation_degrees = sqlite3_column_double(stmt, COL_ROTATION_DEGREES);

    // Read description
    const char *description = (const char*)sqlite3_column_text(stmt, COL_DESCRIPTION);
    if (description) {
      element->description = g_strdup(description);
    }

    // Read created_at
    const char *created_at = (const char*)sqlite3_column_text(stmt, COL_CREATED_AT);
    if (created_at) {
      element->created_at = g_strdup(created_at);
    }

    // Read locked
    element->locked = sqlite3_column_int(stmt, COL_LOCKED) ? TRUE : FALSE;

    element->visual_element = NULL;

    g_hash_table_insert(model->elements, g_strdup(uuid), element);
  }

  sqlite3_finalize(stmt);

  // Commit the read transaction
  if (sqlite3_exec(db, "COMMIT;", NULL, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to commit read transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    // Continue - data is already loaded
  }

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

int database_set_space_parent_id(sqlite3 *db, const char *space_uuid, const char *parent_uuid) {
    const char *sql = "UPDATE spaces SET parent_uuid = ? WHERE uuid = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    // Bind the parent_uuid parameter (can be NULL to remove parent)
    if (parent_uuid != NULL) {
        sqlite3_bind_text(stmt, 1, parent_uuid, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 1);
    }

    // Bind the space_uuid parameter
    sqlite3_bind_text(stmt, 2, space_uuid, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        fprintf(stderr, "Failed to update parent ID: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    // Check if any row was actually updated
    if (sqlite3_changes(db) > 0) {
        return 1;
    } else {
        fprintf(stderr, "No space found with UUID: %s\n", space_uuid);
        return 0;
    }
}

// Remove references with ref_count < 1 from database and return total rows deleted
int cleanup_database_references(sqlite3 *db) {
  int total_deleted = 0;
  char *err_msg = NULL;

  // Note: This function is called from within an existing transaction
  // (from model_save_elements), so we don't start our own transaction

  const char *tables[] = {
    "element_type_refs",
    "position_refs",
    "size_refs",
    "image_refs",
    "video_refs",
    "audio_refs",
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

int database_update_text_ref(sqlite3 *db, ModelText *text) {
  if (text->id <= 0) {
    fprintf(stderr, "Error: Invalid text_id (%d) in database_update_text_ref\n", text->id);
    return 0;
  }

  const char *sql = "UPDATE text_refs SET text = ?, text_r = ?, text_g = ?, text_b = ?, text_a = ?, font_description = ?, strikethrough = ?, alignment = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, text->text, -1, SQLITE_STATIC);
  sqlite3_bind_double(stmt, 2, text->r);
  sqlite3_bind_double(stmt, 3, text->g);
  sqlite3_bind_double(stmt, 4, text->b);
  sqlite3_bind_double(stmt, 5, text->a);
  sqlite3_bind_text(stmt, 6, text->font_description, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 7, text->strikethrough ? 1 : 0);
  sqlite3_bind_text(stmt, 8, text->alignment, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 9, text->ref_count);
  sqlite3_bind_int(stmt, 10, text->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update text: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_update_color_ref(sqlite3 *db, ModelColor *color) {
  if (color->id <= 0) {
    fprintf(stderr, "Error: Invalid bg_color_id (%d) in database_update_color_ref\n", color->id);
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

int database_get_amount_of_elements(sqlite3 *db, const char *space_uuid) {
  if (!db || !space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid parameters in database_get_amount_of_elements\n");
    return 0;
  }

  const char *sql = "SELECT COUNT(*) FROM elements WHERE space_uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return count;
}

int database_delete_space(sqlite3 *db, const char *space_uuid) {
  if (!space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid space UUID in database_delete_space\n");
    return 0;
  }

  const char *sql = "DELETE FROM spaces WHERE uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare delete statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to delete space: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Image reference operations
int database_create_image_ref(sqlite3 *db, const unsigned char *image_data, int image_size, int *image_id) {
  const char *sql = "INSERT INTO image_refs (image_data, image_size) VALUES (?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_blob(stmt, 1, image_data, image_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, image_size);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create image: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *image_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_read_image_ref(sqlite3 *db, int image_id, ModelImage **image) {
  // Initialize output to NULL
  *image = NULL;

  if (image_id <= 0) {
    fprintf(stderr, "Error: Invalid image_id (%d) in database_read_image_ref\n", image_id);
    return 0; // Error - invalid input
  }

  const char *sql = "SELECT image_data, image_size, ref_count FROM image_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0; // Error
  }

  sqlite3_bind_int(stmt, 1, image_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelImage *model_image = g_new0(ModelImage, 1);
    model_image->id = image_id;

    // Get blob data
    const void *blob_data = sqlite3_column_blob(stmt, 0);
    int blob_size = sqlite3_column_bytes(stmt, 0);
    model_image->image_size = sqlite3_column_int(stmt, 1);

    if (blob_data && blob_size > 0) {
      model_image->image_data = g_malloc(blob_size);
      memcpy(model_image->image_data, blob_data, blob_size);
    }

    model_image->ref_count = sqlite3_column_int(stmt, 2);

    *image = model_image;
    sqlite3_finalize(stmt);
    return 1; // Success
  }

  sqlite3_finalize(stmt);
  // Image not found, but this is not an error - *image remains NULL
  return 1; // Success (no error occurred)
}

int database_update_image_ref(sqlite3 *db, ModelImage *image) {
  if (image->id <= 0) {
    fprintf(stderr, "Error: Invalid image_id (%d) in database_update_image_ref\n", image->id);
    return 0;
  }

  const char *sql = "UPDATE image_refs SET image_data = ?, image_size = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_blob(stmt, 1, image->image_data, image->image_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, image->image_size);
  sqlite3_bind_int(stmt, 3, image->ref_count);
  sqlite3_bind_int(stmt, 4, image->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update image: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

// Add to database.c
int database_search_elements(sqlite3 *db, const char *search_term, GList **results) {
  if (!db || !search_term || strlen(search_term) < 1) {
    return -1;
  }

  const char *sql =
    "SELECT ets.element_uuid, ets.text_content, ets.space_uuid, s.name as space_name "
    "FROM element_text_fts ets "
    "JOIN spaces s ON ets.space_uuid = s.uuid "
    "WHERE ets.text_content MATCH ? "
    "ORDER BY bm25(element_text_fts)";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    g_printerr("Failed to prepare search statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  // Prepare search term with wildcards for prefix matching
  char *search_pattern = g_strdup_printf("%s*", search_term);
  sqlite3_bind_text(stmt, 1, search_pattern, -1, SQLITE_TRANSIENT);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SearchResult *result = g_new0(SearchResult, 1);

    result->element_uuid = g_strdup((const char*)sqlite3_column_text(stmt, 0));
    result->text_content = g_strdup((const char*)sqlite3_column_text(stmt, 1));
    result->space_uuid = g_strdup((const char*)sqlite3_column_text(stmt, 2));
    result->space_name = g_strdup((const char*)sqlite3_column_text(stmt, 3));

    *results = g_list_append(*results, result);
  }

  g_free(search_pattern);
  sqlite3_finalize(stmt);
  return 0;
}

void database_free_search_result(SearchResult *result) {
  if (result) {
    g_free(result->element_uuid);
    g_free(result->text_content);
    g_free(result->space_uuid);
    g_free(result->space_name);
    g_free(result);
  }
}

int database_get_all_spaces(sqlite3 *db, GList **spaces) {
  const char *sql = "SELECT uuid, name, created_at FROM spaces ORDER BY created_at DESC";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SpaceInfo *space = g_new0(SpaceInfo, 1);
    space->uuid = g_strdup((const char*)sqlite3_column_text(stmt, 0));
    space->name = g_strdup((const char*)sqlite3_column_text(stmt, 1));
    space->created_at = g_strdup((const char*)sqlite3_column_text(stmt, 2));
    *spaces = g_list_append(*spaces, space);
  }

  sqlite3_finalize(stmt);
  return 1;
}

void database_free_space_info(SpaceInfo *space) {
  if (space) {
    g_free(space->uuid);
    g_free(space->name);
    g_free(space->created_at);
    g_free(space);
  }
}

int database_load_video_data(sqlite3 *db, int video_id, unsigned char **video_data, int *video_size) {
  if (video_id <= 0) {
    fprintf(stderr, "Error: Invalid video_id (%d) in database_load_video_data\n", video_id);
    return 0;
  }

  const char *sql = "SELECT video_data, video_size FROM video_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, video_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const void *blob_data = sqlite3_column_blob(stmt, 0);
    *video_size = sqlite3_column_int(stmt, 1);

    if (blob_data && *video_size > 0) {
      *video_data = g_malloc(*video_size);
      memcpy(*video_data, blob_data, *video_size);
      sqlite3_finalize(stmt);
      return 1;
    }
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_create_video_ref(sqlite3 *db,
                             const unsigned char *thumbnail_data, int thumbnail_size,
                             const unsigned char *video_data, int video_size,
                             int duration, int *video_id) {
  const char *sql = "INSERT INTO video_refs (thumbnail_data, thumbnail_size, video_data, video_size, duration) VALUES (?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;
  sqlite3_bind_blob(stmt, param_index++, thumbnail_data, thumbnail_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param_index++, thumbnail_size);
  sqlite3_bind_blob(stmt, param_index++, video_data, video_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param_index++, video_size);
  sqlite3_bind_int(stmt, param_index++, duration);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create video: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *video_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_read_video_ref(sqlite3 *db, int video_id, ModelVideo **video) {
  // Initialize output to NULL
  *video = NULL;

  if (video_id <= 0) {
    fprintf(stderr, "Error: Invalid video_id (%d) in database_read_video_ref\n", video_id);
    return 0;
  }

  const char *sql = "SELECT thumbnail_data, thumbnail_size, video_size, duration, ref_count FROM video_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, video_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ModelVideo *model_video = g_new0(ModelVideo, 1);
    model_video->id = video_id;
    model_video->is_loaded = FALSE;

    // Get thumbnail data (load this immediately)
    const void *thumb_data = sqlite3_column_blob(stmt, 0);
    int thumb_size = sqlite3_column_bytes(stmt, 0);
    model_video->thumbnail_size = sqlite3_column_int(stmt, 1);

    if (thumb_data && thumb_size > 0) {
      model_video->thumbnail_data = g_malloc(thumb_size);
      memcpy(model_video->thumbnail_data, thumb_data, thumb_size);
    }

    // Store video metadata but don't load video data yet
    model_video->video_size = sqlite3_column_int(stmt, 2);
    model_video->duration = sqlite3_column_int(stmt, 3);

    model_video->ref_count = sqlite3_column_int(stmt, 4);
    model_video->video_data = NULL;  // Video data will be loaded on demand

    *video = model_video;
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_update_video_ref(sqlite3 *db, ModelVideo *video) {
  if (video->id <= 0) {
    fprintf(stderr, "Error: Invalid video_id (%d) in database_update_video_ref\n", video->id);
    return 0;
  }

  const char *sql = "UPDATE video_refs SET thumbnail_data = ?, thumbnail_size = ?, video_size = ?, duration = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;
  sqlite3_bind_blob(stmt, param_index++, video->thumbnail_data, video->thumbnail_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param_index++, video->thumbnail_size);
  sqlite3_bind_int(stmt, param_index++, video->video_size);
  sqlite3_bind_int(stmt, param_index++, video->duration);
  sqlite3_bind_int(stmt, param_index++, video->ref_count);
  sqlite3_bind_int(stmt, param_index++, video->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update video: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_create_audio_ref(sqlite3 *db,
                             const unsigned char *audio_data, int audio_size,
                             int duration, int *audio_id) {
  const char *sql = "INSERT INTO audio_refs (audio_data, audio_size, duration) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;
  sqlite3_bind_blob(stmt, param_index++, audio_data, audio_size, SQLITE_STATIC);
  sqlite3_bind_int(stmt, param_index++, audio_size);
  sqlite3_bind_int(stmt, param_index++, duration);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to create audio: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  *audio_id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return 1;
}

int database_read_audio_ref(sqlite3 *db, int audio_id, ModelAudio **audio) {
  if (audio_id <= 0) {
    fprintf(stderr, "Error: Invalid audio_id (%d) in database_read_audio_ref\n", audio_id);
    return 0;
  }

  const char *sql = "SELECT id, audio_size, duration, ref_count FROM audio_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, audio_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    *audio = g_new0(ModelAudio, 1);
    (*audio)->id = sqlite3_column_int(stmt, 0);
    (*audio)->audio_size = sqlite3_column_int(stmt, 1);
    (*audio)->duration = sqlite3_column_int(stmt, 2);
    (*audio)->ref_count = sqlite3_column_int(stmt, 3);
    (*audio)->audio_data = NULL;  // Audio data not loaded yet
    (*audio)->is_loaded = FALSE;

    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_update_audio_ref(sqlite3 *db, ModelAudio *audio) {
  if (audio->id <= 0) {
    fprintf(stderr, "Error: Invalid audio_id (%d) in database_update_audio_ref\n", audio->id);
    return 0;
  }

  const char *sql = "UPDATE audio_refs SET audio_size = ?, duration = ?, ref_count = ? WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int param_index = 1;
  sqlite3_bind_int(stmt, param_index++, audio->audio_size);
  sqlite3_bind_int(stmt, param_index++, audio->duration);
  sqlite3_bind_int(stmt, param_index++, audio->ref_count);
  sqlite3_bind_int(stmt, param_index++, audio->id);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update audio: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}

int database_load_audio_data(sqlite3 *db, int audio_id, unsigned char **audio_data, int *audio_size) {
  if (audio_id <= 0) {
    fprintf(stderr, "Error: Invalid audio_id (%d) in database_load_audio_data\n", audio_id);
    return 0;
  }

  const char *sql = "SELECT audio_data, audio_size FROM audio_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_int(stmt, 1, audio_id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const void *blob_data = sqlite3_column_blob(stmt, 0);
    *audio_size = sqlite3_column_int(stmt, 1);

    if (blob_data && *audio_size > 0) {
      *audio_data = g_malloc(*audio_size);
      memcpy(*audio_data, blob_data, *audio_size);
      sqlite3_finalize(stmt);
      return 1;
    }
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_get_space_background(sqlite3 *db, const char *space_uuid, char **background_color) {
  if (!space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid space UUID in database_get_space_background\n");
    return 0;
  }

  const char *sql = "SELECT background_color FROM spaces WHERE uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    // Get background color
    const char *color = (const char*)sqlite3_column_text(stmt, 0);
    if (color && background_color) {
      *background_color = g_strdup(color);
    }


    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int database_set_space_background_color(sqlite3 *db, const char *space_uuid, const char *background_color) {
  if (!space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid space UUID in database_set_space_background_color\n");
    return 0;
  }

  const char *sql = "UPDATE spaces SET background_color = ? WHERE uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, background_color, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, space_uuid, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update space background color: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}


int database_get_space_grid_settings(sqlite3 *db, const char *space_uuid, int *grid_enabled, char **grid_color) {
  if (!space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid space UUID in database_get_space_grid_settings\n");
    return 0;
  }
  const char *sql = "SELECT grid_enabled, grid_color FROM spaces WHERE uuid = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  sqlite3_bind_text(stmt, 1, space_uuid, -1, SQLITE_STATIC);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    // Get grid enabled flag
    if (grid_enabled) {
      *grid_enabled = sqlite3_column_int(stmt, 0);
    }
    // Get grid color
    const char *color = (const char*)sqlite3_column_text(stmt, 1);
    if (color && grid_color) {
      *grid_color = g_strdup(color);
    }
    sqlite3_finalize(stmt);
    return 1;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int database_set_space_grid_settings(sqlite3 *db, const char *space_uuid, int grid_enabled, const char *grid_color) {
  if (!space_uuid || !database_is_valid_uuid(space_uuid)) {
    fprintf(stderr, "Error: Invalid space UUID in database_set_space_grid_settings\n");
    return 0;
  }
  const char *sql = "UPDATE spaces SET grid_enabled = ?, grid_color = ? WHERE uuid = ?";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  sqlite3_bind_int(stmt, 1, grid_enabled);
  sqlite3_bind_text(stmt, 2, grid_color, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, space_uuid, -1, SQLITE_STATIC);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update space grid settings: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }
  sqlite3_finalize(stmt);
  return 1;
}

int database_get_setting(sqlite3 *db, const char *key, char **value_out) {
  if (!db || !key) {
    return 0;
  }

  const char *sql = "SELECT value FROM app_settings WHERE key = ?";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare get setting: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  int result = 0;
  if (rc == SQLITE_ROW) {
    const char *value = (const char *)sqlite3_column_text(stmt, 0);
    if (value_out) {
      *value_out = g_strdup(value ? value : "");
    }
    result = 1;
  }

  sqlite3_finalize(stmt);
  return result;
}

int database_set_setting(sqlite3 *db, const char *key, const char *value) {
  if (!db || !key) {
    return 0;
  }

  const char *sql = "INSERT INTO app_settings(key, value) VALUES(?, ?) "
                    "ON CONFLICT(key) DO UPDATE SET value = excluded.value";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare set setting: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
  if (value) {
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 2);
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to set setting %s: %s\n", key, sqlite3_errmsg(db));
    return 0;
  }

  return 1;
}

int database_insert_action_log(sqlite3 *db, const char *origin, const char *prompt, const char *dsl, const char *error_text) {
  if (!db || !origin) {
    return 0;
  }

  const char *sql = "INSERT INTO action_log(origin, prompt, dsl, error) VALUES(?, ?, ?, ?)";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare insert action log: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, origin, -1, SQLITE_STATIC);
  if (prompt) {
    sqlite3_bind_text(stmt, 2, prompt, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 2);
  }
  if (dsl) {
    sqlite3_bind_text(stmt, 3, dsl, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 3);
  }
  if (error_text) {
    sqlite3_bind_text(stmt, 4, error_text, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null(stmt, 4);
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to insert action log: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  return 1;
}
