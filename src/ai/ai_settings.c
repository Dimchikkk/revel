#include "ai_settings.h"

#include <json-glib/json-glib.h>
#include <stdlib.h>
#include "database.h"

#define KEY_SELECTED_PROVIDER "ai.selected_provider"
#define KEY_TIMEOUT_MS "ai.timeout_ms"
#define KEY_MAX_CONTEXT "ai.max_context_bytes"
#define KEY_HISTORY_LIMIT "ai.history_limit"
#define KEY_INCLUDE_GRAMMAR "ai.include_grammar"
#define KEY_CLI_PATHS "ai.cli_paths"

#define DEFAULT_TIMEOUT_MS 60000
#define DEFAULT_MAX_CONTEXT (4 * 1024)
#define DEFAULT_HISTORY_LIMIT 3
#define DEFAULT_INCLUDE_GRAMMAR TRUE

static guint parse_uint(const char *value, guint fallback) {
  if (!value || !*value) {
    return fallback;
  }
  char *endptr = NULL;
  unsigned long long parsed = strtoull(value, &endptr, 10);
  if (!endptr || *endptr != '\0') {
    return fallback;
  }
  return (guint)parsed;
}

static gboolean parse_bool(const char *value, gboolean fallback) {
  if (!value) {
    return fallback;
  }
  if (g_ascii_strcasecmp(value, "true") == 0 || g_strcmp0(value, "1") == 0 || g_ascii_strcasecmp(value, "yes") == 0) {
    return TRUE;
  }
  if (g_ascii_strcasecmp(value, "false") == 0 || g_strcmp0(value, "0") == 0 || g_ascii_strcasecmp(value, "no") == 0) {
    return FALSE;
  }
  return fallback;
}

AiSettings *ai_settings_new(void) {
  AiSettings *settings = g_new0(AiSettings, 1);
  settings->cli_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  settings->timeout_ms = DEFAULT_TIMEOUT_MS;
  settings->max_context_bytes = DEFAULT_MAX_CONTEXT;
  settings->history_limit = DEFAULT_HISTORY_LIMIT;
  settings->include_grammar = DEFAULT_INCLUDE_GRAMMAR;
  return settings;
}

static void load_cli_paths(AiSettings *settings, const char *json_text) {
  if (!json_text || !*json_text) {
    return;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  if (!json_parser_load_from_data(parser, json_text, -1, NULL)) {
    return;
  }
  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    return;
  }
  JsonObject *obj = json_node_get_object(root);
  GList *members = json_object_get_members(obj);
  for (GList *l = members; l; l = l->next) {
    const gchar *key = l->data;
    const gchar *value = json_object_get_string_member(obj, key);
    ai_settings_set_cli_path(settings, key, value);
  }
  g_list_free(members);
}

AiSettings *ai_settings_load(sqlite3 *db) {
  AiSettings *settings = ai_settings_new();
  if (!db) {
    return settings;
  }

  gchar *value = NULL;
  if (database_get_setting(db, KEY_SELECTED_PROVIDER, &value)) {
    ai_settings_set_selected_provider(settings, value);
    g_free(value);
  }
  if (database_get_setting(db, KEY_TIMEOUT_MS, &value)) {
    settings->timeout_ms = parse_uint(value, settings->timeout_ms);
    g_free(value);
  }
  if (database_get_setting(db, KEY_MAX_CONTEXT, &value)) {
    settings->max_context_bytes = parse_uint(value, settings->max_context_bytes);
    g_free(value);
  }
  if (database_get_setting(db, KEY_HISTORY_LIMIT, &value)) {
    settings->history_limit = parse_uint(value, settings->history_limit);
    g_free(value);
  }
  if (database_get_setting(db, KEY_INCLUDE_GRAMMAR, &value)) {
    settings->include_grammar = parse_bool(value, settings->include_grammar);
    g_free(value);
  }
  if (database_get_setting(db, KEY_CLI_PATHS, &value)) {
    load_cli_paths(settings, value);
    g_free(value);
  }

  return settings;
}

static gchar *cli_paths_to_json(const AiSettings *settings) {
  if (!settings || !settings->cli_paths || g_hash_table_size(settings->cli_paths) == 0) {
    return g_strdup("");
  }
  g_autoptr(JsonBuilder) builder = json_builder_new();
  json_builder_begin_object(builder);

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, settings->cli_paths);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    const gchar *provider_id = key;
    const gchar *path = value;
    json_builder_set_member_name(builder, provider_id);
    json_builder_add_string_value(builder, path);
  }

  json_builder_end_object(builder);
  g_autoptr(JsonGenerator) generator = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(generator, root);
  gchar *text = json_generator_to_data(generator, NULL);
  json_node_free(root);
  return text;
}

int ai_settings_save(const AiSettings *settings, sqlite3 *db) {
  if (!settings || !db) {
    return 0;
  }

  gchar buffer[32];

  if (!database_set_setting(db, KEY_SELECTED_PROVIDER, settings->selected_provider)) return 0;

  g_snprintf(buffer, sizeof(buffer), "%u", settings->timeout_ms);
  if (!database_set_setting(db, KEY_TIMEOUT_MS, buffer)) return 0;

  g_snprintf(buffer, sizeof(buffer), "%u", settings->max_context_bytes);
  if (!database_set_setting(db, KEY_MAX_CONTEXT, buffer)) return 0;

  g_snprintf(buffer, sizeof(buffer), "%u", settings->history_limit);
  if (!database_set_setting(db, KEY_HISTORY_LIMIT, buffer)) return 0;

  g_snprintf(buffer, sizeof(buffer), "%u", settings->include_grammar ? 1 : 0);
  if (!database_set_setting(db, KEY_INCLUDE_GRAMMAR, buffer)) return 0;

  gchar *paths_json = cli_paths_to_json(settings);
  gboolean paths_ok = database_set_setting(db, KEY_CLI_PATHS, paths_json && *paths_json ? paths_json : NULL);
  g_free(paths_json);
  if (!paths_ok) {
    return 0;
  }

  return 1;
}

void ai_settings_set_selected_provider(AiSettings *settings, const gchar *provider_id) {
  if (!settings) {
    return;
  }
  g_free(settings->selected_provider);
  settings->selected_provider = provider_id ? g_strdup(provider_id) : NULL;
}

const gchar *ai_settings_get_selected_provider(const AiSettings *settings) {
  return settings ? settings->selected_provider : NULL;
}

void ai_settings_set_cli_path(AiSettings *settings, const gchar *provider_id, const gchar *path) {
  if (!settings || !provider_id) {
    return;
  }
  if (!settings->cli_paths) {
    settings->cli_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  }
  if (path && *path) {
    g_hash_table_replace(settings->cli_paths, g_strdup(provider_id), g_strdup(path));
  } else {
    g_hash_table_remove(settings->cli_paths, provider_id);
  }
}

const gchar *ai_settings_get_cli_path(const AiSettings *settings, const gchar *provider_id) {
  if (!settings || !settings->cli_paths || !provider_id) {
    return NULL;
  }
  return g_hash_table_lookup(settings->cli_paths, provider_id);
}

void ai_settings_set_timeout(AiSettings *settings, guint timeout_ms) {
  if (!settings) return;
  settings->timeout_ms = timeout_ms ? timeout_ms : DEFAULT_TIMEOUT_MS;
}

void ai_settings_set_max_context(AiSettings *settings, guint bytes) {
  if (!settings) return;
  settings->max_context_bytes = bytes ? bytes : DEFAULT_MAX_CONTEXT;
}

void ai_settings_set_history_limit(AiSettings *settings, guint limit) {
  if (!settings) return;
  settings->history_limit = limit ? limit : DEFAULT_HISTORY_LIMIT;
}

void ai_settings_set_include_grammar(AiSettings *settings, gboolean include) {
  if (!settings) return;
  settings->include_grammar = include;
}

guint ai_settings_get_timeout(const AiSettings *settings) {
  return settings ? settings->timeout_ms : DEFAULT_TIMEOUT_MS;
}

guint ai_settings_get_max_context(const AiSettings *settings) {
  return settings ? settings->max_context_bytes : DEFAULT_MAX_CONTEXT;
}

guint ai_settings_get_history_limit(const AiSettings *settings) {
  return settings ? settings->history_limit : DEFAULT_HISTORY_LIMIT;
}

gboolean ai_settings_get_include_grammar(const AiSettings *settings) {
  return settings ? settings->include_grammar : DEFAULT_INCLUDE_GRAMMAR;
}

void ai_settings_free(AiSettings *settings) {
  if (!settings) {
    return;
  }
  g_free(settings->selected_provider);
  if (settings->cli_paths) {
    g_hash_table_destroy(settings->cli_paths);
  }
  g_free(settings);
}
