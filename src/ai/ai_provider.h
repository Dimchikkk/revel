#ifndef AI_PROVIDER_H
#define AI_PROVIDER_H

#include <glib.h>

typedef enum {
  AI_PAYLOAD_STDIN,
  AI_PAYLOAD_ARG
} AiPayloadMode;

typedef struct {
  gchar *id;
  gchar *label;
  gchar *binary;
  gchar **default_args;
  AiPayloadMode payload_mode;
  gchar *arg_flag;
  gchar *stdin_flag;
  gchar *default_binary;
} AiProvider;

typedef struct {
  gchar *prompt;
  gchar *dsl;
  gchar *error;
} AiConversationEntry;

typedef struct {
  gchar *provider_id;
  GPtrArray *log;
  gchar *last_context_snapshot;
} AiSessionState;

AiProvider *ai_provider_copy(const AiProvider *provider);
void ai_provider_free(AiProvider *provider);
GPtrArray *ai_provider_list_new(void);
void ai_provider_list_add(GPtrArray *providers, AiProvider *provider);
void ai_provider_list_free(GPtrArray *providers);
const AiProvider *ai_provider_find(const GPtrArray *providers, const gchar *id);
void ai_provider_set_binary(AiProvider *provider, const gchar *binary);
void ai_provider_reset_binary(AiProvider *provider);
const gchar *ai_provider_get_binary(const AiProvider *provider);
const gchar *ai_provider_get_default_binary(const AiProvider *provider);
AiPayloadMode ai_provider_get_payload_mode(const AiProvider *provider);
const gchar *ai_provider_get_arg_flag(const AiProvider *provider);
const gchar *ai_provider_get_stdin_flag(const AiProvider *provider);
GPtrArray *ai_provider_load_from_path(const gchar *path, GError **error);
GPtrArray *ai_provider_load_with_fallback(const gchar *config_dir, GError **error);

AiConversationEntry *ai_conversation_entry_new(const gchar *prompt, const gchar *dsl, const gchar *error);
void ai_conversation_entry_free(AiConversationEntry *entry);

AiSessionState *ai_session_state_new(void);
void ai_session_state_set_provider(AiSessionState *state, const gchar *provider_id);
const gchar *ai_session_state_get_provider(const AiSessionState *state);
void ai_session_state_append_entry(AiSessionState *state, AiConversationEntry *entry);
GPtrArray *ai_session_state_get_log(const AiSessionState *state);
void ai_session_state_set_context_snapshot(AiSessionState *state, const gchar *snapshot);
const gchar *ai_session_state_get_context_snapshot(const AiSessionState *state);
void ai_session_state_clear(AiSessionState *state);
void ai_session_state_free(AiSessionState *state);

#endif
