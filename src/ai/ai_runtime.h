#ifndef AI_RUNTIME_H
#define AI_RUNTIME_H

#include <glib.h>
#include <sqlite3.h>
#include "ai_provider.h"
#include "ai_settings.h"
#include "ai_context.h"

typedef struct _AiRuntime {
  GPtrArray *providers;
  AiSettings *settings;
  AiSessionState *session;
  guint timeout_ms;
  guint max_context_bytes;
  guint history_limit;
  gboolean include_grammar;
} AiRuntime;

AiRuntime *ai_runtime_new(sqlite3 *db, const gchar *config_dir);
void ai_runtime_free(AiRuntime *runtime);

AiProvider *ai_runtime_get_provider(AiRuntime *runtime, const gchar *id);
AiProvider *ai_runtime_get_active_provider(AiRuntime *runtime);
void ai_runtime_set_active_provider(AiRuntime *runtime, const gchar *id);
void ai_runtime_set_cli_override(AiRuntime *runtime, const gchar *provider_id, const gchar *path);
const gchar *ai_runtime_get_cli_override(const AiRuntime *runtime, const gchar *provider_id);

char *ai_runtime_build_payload(AiRuntime *runtime,
                               CanvasData *data,
                               const char *prompt,
                               char **out_snapshot,
                               gboolean *out_truncated,
                               GError **error);

guint ai_runtime_get_timeout(const AiRuntime *runtime);
guint ai_runtime_get_max_context(const AiRuntime *runtime);
guint ai_runtime_get_history_limit(const AiRuntime *runtime);
gboolean ai_runtime_get_include_grammar(const AiRuntime *runtime);
void ai_runtime_set_timeout(AiRuntime *runtime, guint timeout_ms);
void ai_runtime_set_max_context(AiRuntime *runtime, guint max_context_bytes);
void ai_runtime_set_history_limit(AiRuntime *runtime, guint history_limit);
void ai_runtime_set_include_grammar(AiRuntime *runtime, gboolean include);

int ai_runtime_save_settings(AiRuntime *runtime, sqlite3 *db);

#endif
