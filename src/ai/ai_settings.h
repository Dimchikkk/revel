#ifndef AI_SETTINGS_H
#define AI_SETTINGS_H

#include <glib.h>
#include <sqlite3.h>

typedef struct {
  gchar *selected_provider;
  GHashTable *cli_paths;     // provider_id -> char*
  guint timeout_ms;
  guint max_context_bytes;
  guint history_limit;
  gboolean include_grammar;
} AiSettings;

AiSettings *ai_settings_new(void);
AiSettings *ai_settings_load(sqlite3 *db);
int ai_settings_save(const AiSettings *settings, sqlite3 *db);

void ai_settings_set_selected_provider(AiSettings *settings, const gchar *provider_id);
const gchar *ai_settings_get_selected_provider(const AiSettings *settings);

void ai_settings_set_cli_path(AiSettings *settings, const gchar *provider_id, const gchar *path);
const gchar *ai_settings_get_cli_path(const AiSettings *settings, const gchar *provider_id);

void ai_settings_set_timeout(AiSettings *settings, guint timeout_ms);
void ai_settings_set_max_context(AiSettings *settings, guint bytes);
void ai_settings_set_history_limit(AiSettings *settings, guint limit);
void ai_settings_set_include_grammar(AiSettings *settings, gboolean include);
guint ai_settings_get_timeout(const AiSettings *settings);
guint ai_settings_get_max_context(const AiSettings *settings);
guint ai_settings_get_history_limit(const AiSettings *settings);
gboolean ai_settings_get_include_grammar(const AiSettings *settings);

void ai_settings_free(AiSettings *settings);

#endif
