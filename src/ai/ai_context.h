#ifndef AI_CONTEXT_H
#define AI_CONTEXT_H

#include <glib.h>
#include "ai_provider.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  guint max_context_bytes;      // Maximum bytes for context DSL; 0 uses default (4096)
  gboolean include_grammar;     // Whether to include DSL grammar snippet
  guint history_limit;          // How many previous exchanges to include
} AiContextOptions;

#define AI_CONTEXT_DEFAULT_MAX_BYTES (4 * 1024)

char *ai_context_build_payload(CanvasData *data,
                               AiSessionState *session,
                               const char *prompt,
                               const char *retry_error,
                               const AiContextOptions *options,
                               char **out_snapshot,
                               gboolean *out_truncated,
                               GError **error);

char *ai_context_truncate_utf8(const char *text, guint max_bytes);

#endif
