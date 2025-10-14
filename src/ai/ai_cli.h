#ifndef AI_CLI_H
#define AI_CLI_H

#include <glib.h>
#include <gio/gio.h>
#include "ai_provider.h"

gboolean ai_cli_generate(const AiProvider *provider, const gchar *payload, GCancellable *cancellable, gchar **out_dsl, gchar **out_error);
gboolean ai_cli_generate_with_timeout(const AiProvider *provider, const gchar *payload, guint timeout_ms, GCancellable *cancellable, gchar **out_dsl, gchar **out_error);

#endif
