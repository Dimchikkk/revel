#include "ai_runtime.h"

#include <glib.h>
#include <glib/gdatetime.h>

#include "ai_provider.h"

#define AI_HISTORY_DEFAULT_LIMIT 3
#define AI_HISTORY_PRUNE_DAYS 7
#define AI_HISTORY_RESTORE_MULTIPLIER 5
#define AI_HISTORY_RESTORE_MIN 20

static void ai_runtime_restore_history(AiRuntime *runtime, sqlite3 *db);

struct _CanvasData; // forward declaration satisfied by ai_context include

static AiProvider *find_provider(GPtrArray *providers, const gchar *id) {
  if (!providers || !id) {
    return NULL;
  }
  for (guint i = 0; i < providers->len; i++) {
    AiProvider *provider = g_ptr_array_index(providers, i);
    if (g_strcmp0(provider->id, id) == 0) {
      return provider;
    }
  }
  return NULL;
}

static guint ai_runtime_restore_limit(const AiRuntime *runtime) {
  guint base = runtime && runtime->history_limit ? runtime->history_limit : AI_HISTORY_DEFAULT_LIMIT;
  guint limit = base * AI_HISTORY_RESTORE_MULTIPLIER;
  if (limit < AI_HISTORY_RESTORE_MIN) {
    limit = AI_HISTORY_RESTORE_MIN;
  }
  return limit;
}

static void ai_runtime_restore_history(AiRuntime *runtime, sqlite3 *db) {
  if (!runtime || !db) {
    return;
  }

  GDateTime *now = g_date_time_new_now_local();
  GDateTime *cutoff = now ? g_date_time_add_days(now, -AI_HISTORY_PRUNE_DAYS) : NULL;
  gchar *cutoff_iso = cutoff ? g_date_time_format(cutoff, "%Y-%m-%d %H:%M:%S") : NULL;

  if (cutoff_iso) {
    const char *delete_sql = "DELETE FROM action_log WHERE origin='ai' AND created_at < ?";
    sqlite3_stmt *delete_stmt = NULL;
    if (sqlite3_prepare_v2(db, delete_sql, -1, &delete_stmt, NULL) == SQLITE_OK) {
      sqlite3_bind_text(delete_stmt, 1, cutoff_iso, -1, (sqlite3_destructor_type)SQLITE_TRANSIENT);
      sqlite3_step(delete_stmt);
      sqlite3_finalize(delete_stmt);
    }
  }

  const char *select_sql = "SELECT prompt, dsl, error, created_at FROM action_log WHERE origin='ai' ORDER BY id ASC";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK) {
    g_free(cutoff_iso);
    if (cutoff) g_date_time_unref(cutoff);
    if (now) g_date_time_unref(now);
    return;
  }

  GPtrArray *entries = g_ptr_array_new();
  guint restore_limit = ai_runtime_restore_limit(runtime);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *prompt = (const char *)sqlite3_column_text(stmt, 0);
    const char *dsl = (const char *)sqlite3_column_text(stmt, 1);
    const char *error_text = (const char *)sqlite3_column_text(stmt, 2);
    const char *created_at = (const char *)sqlite3_column_text(stmt, 3);

    if (cutoff_iso && created_at && g_strcmp0(created_at, cutoff_iso) < 0) {
      continue;
    }

    AiConversationEntry *entry = ai_conversation_entry_new(prompt, dsl, error_text);
    g_ptr_array_add(entries, entry);

    if (entries->len > restore_limit) {
      guint overflow = entries->len - restore_limit;
      for (guint i = 0; i < overflow; i++) {
        AiConversationEntry *old_entry = g_ptr_array_index(entries, i);
        ai_conversation_entry_free(old_entry);
      }
      g_ptr_array_remove_range(entries, 0, overflow);
    }
  }

  sqlite3_finalize(stmt);

  for (guint i = 0; i < entries->len; i++) {
    AiConversationEntry *entry = g_ptr_array_index(entries, i);
    ai_session_state_append_entry(runtime->session, entry);
  }
  g_ptr_array_free(entries, FALSE);

  g_free(cutoff_iso);
  if (cutoff) g_date_time_unref(cutoff);
  if (now) g_date_time_unref(now);
}

AiRuntime *ai_runtime_new(sqlite3 *db, const gchar *config_dir) {
  AiRuntime *runtime = g_new0(AiRuntime, 1);
  runtime->settings = ai_settings_load(db);
  runtime->providers = ai_provider_load_with_fallback(config_dir, NULL);
  runtime->session = ai_session_state_new();
  runtime->timeout_ms = ai_settings_get_timeout(runtime->settings);
  runtime->max_context_bytes = ai_settings_get_max_context(runtime->settings);
  runtime->history_limit = ai_settings_get_history_limit(runtime->settings);
  runtime->include_grammar = ai_settings_get_include_grammar(runtime->settings);

  if (runtime->providers) {
    for (guint i = 0; i < runtime->providers->len; i++) {
      AiProvider *provider = g_ptr_array_index(runtime->providers, i);
      const gchar *override_path = ai_settings_get_cli_path(runtime->settings, provider->id);
      if (override_path && *override_path) {
        ai_provider_set_binary(provider, override_path);
      } else {
        ai_provider_reset_binary(provider);
      }
    }
  }

  ai_runtime_restore_history(runtime, db);

  const gchar *selected = ai_settings_get_selected_provider(runtime->settings);
  AiProvider *active = selected ? find_provider(runtime->providers, selected) : NULL;
  if (!active && runtime->providers && runtime->providers->len > 0) {
    active = g_ptr_array_index(runtime->providers, 0);
    ai_settings_set_selected_provider(runtime->settings, active->id);
  }
  if (active) {
    ai_session_state_set_provider(runtime->session, active->id);
  }

  return runtime;
}

void ai_runtime_free(AiRuntime *runtime) {
  if (!runtime) {
    return;
  }
  ai_provider_list_free(runtime->providers);
  ai_settings_free(runtime->settings);
  ai_session_state_free(runtime->session);
  g_free(runtime);
}

AiProvider *ai_runtime_get_provider(AiRuntime *runtime, const gchar *id) {
  if (!runtime) {
    return NULL;
  }
  return find_provider(runtime->providers, id);
}

AiProvider *ai_runtime_get_active_provider(AiRuntime *runtime) {
  if (!runtime) {
    return NULL;
  }
  const gchar *selected = ai_settings_get_selected_provider(runtime->settings);
  if (!selected) {
    return NULL;
  }
  return find_provider(runtime->providers, selected);
}

void ai_runtime_set_active_provider(AiRuntime *runtime, const gchar *id) {
  if (!runtime) {
    return;
  }
  AiProvider *provider = find_provider(runtime->providers, id);
  if (!provider) {
    return;
  }
  ai_settings_set_selected_provider(runtime->settings, provider->id);
  ai_session_state_set_provider(runtime->session, provider->id);
}

void ai_runtime_set_cli_override(AiRuntime *runtime, const gchar *provider_id, const gchar *path) {
  if (!runtime || !provider_id) {
    return;
  }
  AiProvider *provider = find_provider(runtime->providers, provider_id);
  if (path && *path) {
    ai_settings_set_cli_path(runtime->settings, provider_id, path);
    if (provider) {
      ai_provider_set_binary(provider, path);
    }
  } else {
    ai_settings_set_cli_path(runtime->settings, provider_id, NULL);
    if (provider) {
      ai_provider_reset_binary(provider);
    }
  }
}

const gchar *ai_runtime_get_cli_override(const AiRuntime *runtime, const gchar *provider_id) {
  if (!runtime || !provider_id) {
    return NULL;
  }
  return ai_settings_get_cli_path(runtime->settings, provider_id);
}

char *ai_runtime_build_payload(AiRuntime *runtime,
                               CanvasData *data,
                               const char *prompt,
                               char **out_snapshot,
                               gboolean *out_truncated,
                               GError **error) {
  if (!runtime) {
    g_set_error(error, g_quark_from_static_string("ai_runtime"), 1, "AI runtime unavailable");
    return NULL;
  }

  AiContextOptions options = {
    .max_context_bytes = runtime->max_context_bytes,
    .include_grammar = runtime->include_grammar,
    .history_limit = runtime->history_limit,
  };

  char *payload = ai_context_build_payload(data, runtime->session, prompt, &options,
                                           out_snapshot, out_truncated, error);
  if (payload && runtime->session) {
    ai_session_state_set_provider(runtime->session, ai_settings_get_selected_provider(runtime->settings));
    if (out_snapshot && *out_snapshot) {
      ai_session_state_set_context_snapshot(runtime->session, *out_snapshot);
    }
  }
  return payload;
}

guint ai_runtime_get_timeout(const AiRuntime *runtime) {
  return runtime ? runtime->timeout_ms : 60000;
}

guint ai_runtime_get_max_context(const AiRuntime *runtime) {
  return runtime ? runtime->max_context_bytes : 0;
}

guint ai_runtime_get_history_limit(const AiRuntime *runtime) {
  return runtime ? runtime->history_limit : 0;
}

gboolean ai_runtime_get_include_grammar(const AiRuntime *runtime) {
  return runtime ? runtime->include_grammar : TRUE;
}

void ai_runtime_set_timeout(AiRuntime *runtime, guint timeout_ms) {
  if (!runtime) return;
  runtime->timeout_ms = timeout_ms;
  ai_settings_set_timeout(runtime->settings, timeout_ms);
}

void ai_runtime_set_max_context(AiRuntime *runtime, guint max_context_bytes) {
  if (!runtime) return;
  runtime->max_context_bytes = max_context_bytes;
  ai_settings_set_max_context(runtime->settings, max_context_bytes);
}

void ai_runtime_set_history_limit(AiRuntime *runtime, guint history_limit) {
  if (!runtime) return;
  runtime->history_limit = history_limit;
  ai_settings_set_history_limit(runtime->settings, history_limit);
}

void ai_runtime_set_include_grammar(AiRuntime *runtime, gboolean include) {
  if (!runtime) return;
  runtime->include_grammar = include;
  ai_settings_set_include_grammar(runtime->settings, include);
}

int ai_runtime_save_settings(AiRuntime *runtime, sqlite3 *db) {
  if (!runtime) {
    return 0;
  }
  return ai_settings_save(runtime->settings, db);
}
