#include "ai_provider.h"

#include <json-glib/json-glib.h>
#include <string.h>

static AiProvider *ai_provider_new(const gchar *id,
                                   const gchar *label,
                                   const gchar *binary,
                                   gchar **args,
                                   AiPayloadMode payload_mode,
                                   const gchar *arg_flag,
                                   const gchar *stdin_flag) {
  AiProvider *provider = g_new0(AiProvider, 1);
  provider->id = g_strdup(id);
  provider->label = g_strdup(label);
  provider->binary = g_strdup(binary);
  provider->default_binary = g_strdup(binary);
  provider->default_args = args;
  provider->payload_mode = payload_mode;
  provider->arg_flag = g_strdup(arg_flag);
  provider->stdin_flag = g_strdup(stdin_flag);
  return provider;
}

AiProvider *ai_provider_copy(const AiProvider *provider) {
  if (!provider) {
    return NULL;
  }

  gchar **args_copy = provider->default_args ? g_strdupv(provider->default_args) : NULL;
  AiProvider *copy = ai_provider_new(provider->id,
                                     provider->label,
                                     provider->binary,
                                     args_copy,
                                     provider->payload_mode,
                                     provider->arg_flag,
                                     provider->stdin_flag);
  ai_provider_set_binary(copy, provider->binary);
  g_free(copy->default_binary);
  copy->default_binary = g_strdup(provider->default_binary);
  return copy;
}

void ai_provider_free(AiProvider *provider) {
  if (!provider) {
    return;
  }
  g_free(provider->id);
  g_free(provider->label);
  g_free(provider->binary);
  g_free(provider->default_binary);
  g_free(provider->arg_flag);
  g_free(provider->stdin_flag);
  g_strfreev(provider->default_args);
  g_free(provider);
}

GPtrArray *ai_provider_list_new(void) {
  return g_ptr_array_new_with_free_func((GDestroyNotify)ai_provider_free);
}

void ai_provider_list_add(GPtrArray *providers, AiProvider *provider) {
  if (!providers || !provider) {
    ai_provider_free(provider);
    return;
  }
  g_ptr_array_add(providers, provider);
}

void ai_provider_set_binary(AiProvider *provider, const gchar *binary) {
  if (!provider) {
    return;
  }
  g_free(provider->binary);
  provider->binary = g_strdup(binary);
}

void ai_provider_reset_binary(AiProvider *provider) {
  if (!provider) {
    return;
  }
  ai_provider_set_binary(provider, provider->default_binary);
}

const gchar *ai_provider_get_binary(const AiProvider *provider) {
  return provider ? provider->binary : NULL;
}

const gchar *ai_provider_get_default_binary(const AiProvider *provider) {
  return provider ? provider->default_binary : NULL;
}

AiPayloadMode ai_provider_get_payload_mode(const AiProvider *provider) {
  return provider ? provider->payload_mode : AI_PAYLOAD_STDIN;
}

const gchar *ai_provider_get_arg_flag(const AiProvider *provider) {
  return provider ? provider->arg_flag : NULL;
}

const gchar *ai_provider_get_stdin_flag(const AiProvider *provider) {
  return provider ? provider->stdin_flag : NULL;
}

void ai_provider_list_free(GPtrArray *providers) {
  if (!providers) {
    return;
  }
  g_ptr_array_free(providers, TRUE);
}

const AiProvider *ai_provider_find(const GPtrArray *providers, const gchar *id) {
  if (!providers || !id) {
    return NULL;
  }
  for (guint i = 0; i < providers->len; i++) {
    AiProvider *provider = g_ptr_array_index((GPtrArray *)providers, i);
    if (g_strcmp0(provider->id, id) == 0) {
      return provider;
    }
  }
  return NULL;
}

static gchar **strv_from_json_array(JsonArray *array, GError **error) {
  if (!array) {
    return NULL;
  }

  guint length = json_array_get_length(array);
  gchar **values = g_new0(gchar *, length + 1);
  for (guint i = 0; i < length; i++) {
    JsonNode *node = json_array_get_element(array, i);
    if (!JSON_NODE_HOLDS_VALUE(node) || json_node_get_value_type(node) != G_TYPE_STRING) {
      g_set_error(error, g_quark_from_static_string("ai_provider"), 1, "Invalid args entry type");
      g_strfreev(values);
      return NULL;
    }
    values[i] = g_strdup(json_node_get_string(node));
  }
  return values;
}

typedef struct {
  const gchar *id;
  const gchar *label;
  const gchar *binary;
  const gchar * const *args;
  AiPayloadMode payload_mode;
  const gchar *arg_flag;
  const gchar *stdin_flag;
} AiProviderDefaultSpec;

static const gchar * const empty_args[] = { NULL };

static const AiProviderDefaultSpec default_specs[] = {
  { "claude", "Claude", "claude", empty_args, AI_PAYLOAD_STDIN, NULL, NULL },
  { "gemini", "Gemini", "gemini", empty_args, AI_PAYLOAD_STDIN, NULL, NULL },
  { "grok", "Grok", "grok", empty_args, AI_PAYLOAD_STDIN, NULL, NULL },
  { "codex", "Codex", "codex", empty_args, AI_PAYLOAD_STDIN, NULL, NULL }
};

static void populate_defaults(GPtrArray *providers) {
  for (guint i = 0; i < G_N_ELEMENTS(default_specs); i++) {
    const AiProviderDefaultSpec *spec = &default_specs[i];
    gchar **args_copy = NULL;
    if (spec->args) {
      guint count = 0;
      while (spec->args[count]) {
        count++;
      }
      if (count > 0) {
        args_copy = g_new0(gchar *, count + 1);
        for (guint j = 0; j < count; j++) {
          args_copy[j] = g_strdup(spec->args[j]);
        }
      }
    }
    ai_provider_list_add(providers, ai_provider_new(spec->id,
                                                   spec->label,
                                                   spec->binary,
                                                   args_copy,
                                                   spec->payload_mode,
                                                   spec->arg_flag,
                                                   spec->stdin_flag));
  }
}

static AiPayloadMode parse_payload_mode(const gchar *value) {
  if (!value) {
    return AI_PAYLOAD_STDIN;
  }
  if (g_ascii_strcasecmp(value, "arg") == 0 || g_ascii_strcasecmp(value, "argument") == 0) {
    return AI_PAYLOAD_ARG;
  }
  return AI_PAYLOAD_STDIN;
}

GPtrArray *ai_provider_load_from_path(const gchar *path, GError **error) {
  g_return_val_if_fail(path != NULL, NULL);

  gchar *contents = NULL;
  gsize length = 0;
  if (!g_file_get_contents(path, &contents, &length, error)) {
    return NULL;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  GError *local_error = NULL;
  if (!json_parser_load_from_data(parser, contents, length, &local_error)) {
    g_propagate_error(error, local_error);
    g_free(contents);
    return NULL;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_set_error(error, g_quark_from_static_string("ai_provider"), 2, "Root must be object");
    g_free(contents);
    return NULL;
  }

  JsonObject *root_obj = json_node_get_object(root);
  if (!json_object_has_member(root_obj, "providers")) {
    g_set_error(error, g_quark_from_static_string("ai_provider"), 3, "Missing providers array");
    g_free(contents);
    return NULL;
  }

  JsonArray *providers_array = json_object_get_array_member(root_obj, "providers");
  GPtrArray *providers = ai_provider_list_new();

  for (guint i = 0; i < json_array_get_length(providers_array); i++) {
    JsonObject *obj = json_array_get_object_element(providers_array, i);
    if (!obj) {
      g_set_error(error, g_quark_from_static_string("ai_provider"), 4, "Provider entry must be object");
      ai_provider_list_free(providers);
      g_free(contents);
      return NULL;
    }

    const gchar *id = json_object_get_string_member(obj, "id");
    const gchar *label = json_object_has_member(obj, "label") ? json_object_get_string_member(obj, "label") : id;
    const gchar *binary = json_object_has_member(obj, "binary") ? json_object_get_string_member(obj, "binary") : id;

    gchar **args = NULL;
    if (json_object_has_member(obj, "args")) {
      JsonArray *args_array = json_object_get_array_member(obj, "args");
      args = strv_from_json_array(args_array, error);
      if (error && *error) {
        ai_provider_list_free(providers);
        g_free(contents);
        return NULL;
      }
    }

    AiPayloadMode payload_mode = AI_PAYLOAD_STDIN;
    const gchar *arg_flag = NULL;
    if (json_object_has_member(obj, "input_mode")) {
      payload_mode = parse_payload_mode(json_object_get_string_member(obj, "input_mode"));
    }
    if (json_object_has_member(obj, "arg_flag")) {
      arg_flag = json_object_get_string_member(obj, "arg_flag");
    }
    const gchar *stdin_flag = NULL;
    if (json_object_has_member(obj, "stdin_flag")) {
      stdin_flag = json_object_get_string_member(obj, "stdin_flag");
    }

    ai_provider_list_add(providers, ai_provider_new(id,
                                                   label,
                                                   binary,
                                                   args,
                                                   payload_mode,
                                                   arg_flag,
                                                   stdin_flag));
  }

  g_free(contents);
  return providers;
}

GPtrArray *ai_provider_load_with_fallback(const gchar *config_dir, GError **error) {
  gchar *path = g_build_filename(config_dir ? config_dir : "config", "ai_providers.json", NULL);
  GError *local_error = NULL;
  GPtrArray *providers = ai_provider_load_from_path(path, &local_error);

  if (!providers) {
    if (local_error) {
      g_clear_error(&local_error);
    }
    providers = ai_provider_list_new();
    populate_defaults(providers);
  }

  g_free(path);
  return providers;
}

AiConversationEntry *ai_conversation_entry_new(const gchar *prompt, const gchar *dsl, const gchar *error) {
  AiConversationEntry *entry = g_new0(AiConversationEntry, 1);
  entry->prompt = g_strdup(prompt);
  entry->dsl = g_strdup(dsl);
  entry->error = g_strdup(error);
  return entry;
}

void ai_conversation_entry_free(AiConversationEntry *entry) {
  if (!entry) {
    return;
  }
  g_free(entry->prompt);
  g_free(entry->dsl);
  g_free(entry->error);
  g_free(entry);
}

AiSessionState *ai_session_state_new(void) {
  AiSessionState *state = g_new0(AiSessionState, 1);
  state->log = g_ptr_array_new_with_free_func((GDestroyNotify)ai_conversation_entry_free);
  return state;
}

void ai_session_state_set_provider(AiSessionState *state, const gchar *provider_id) {
  if (!state) {
    return;
  }
  g_free(state->provider_id);
  state->provider_id = g_strdup(provider_id);
}

const gchar *ai_session_state_get_provider(const AiSessionState *state) {
  return state ? state->provider_id : NULL;
}

void ai_session_state_append_entry(AiSessionState *state, AiConversationEntry *entry) {
  if (!state || !entry) {
    ai_conversation_entry_free(entry);
    return;
  }
  g_ptr_array_add(state->log, entry);
}

GPtrArray *ai_session_state_get_log(const AiSessionState *state) {
  return state ? state->log : NULL;
}

void ai_session_state_set_context_snapshot(AiSessionState *state, const gchar *snapshot) {
  if (!state) {
    return;
  }
  g_free(state->last_context_snapshot);
  state->last_context_snapshot = g_strdup(snapshot);
}

const gchar *ai_session_state_get_context_snapshot(const AiSessionState *state) {
  return state ? state->last_context_snapshot : NULL;
}

void ai_session_state_clear(AiSessionState *state) {
  if (!state) {
    return;
  }
  if (state->log) {
    while (state->log->len > 0) {
      g_ptr_array_remove_index(state->log, state->log->len - 1);
    }
  }
  g_clear_pointer(&state->last_context_snapshot, g_free);
}

void ai_session_state_free(AiSessionState *state) {
  if (!state) {
    return;
  }
  ai_session_state_clear(state);
  g_free(state->provider_id);
  if (state->log) {
    g_ptr_array_free(state->log, TRUE);
  }
  g_free(state);
}
