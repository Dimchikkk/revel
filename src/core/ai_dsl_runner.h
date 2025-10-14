#ifndef AI_DSL_RUNNER_H
#define AI_DSL_RUNNER_H

#include <glib.h>
#include "canvas.h"

typedef struct {
  const char *description;   // Undo description label, defaults to "AI Update"
  gboolean dry_run;          // Future hook for sandboxing
} AiDslRunnerOptions;

#define AI_DSL_RUNNER_DEFAULT_LABEL "AI Update"

char *ai_dsl_runner_validate(CanvasData *data, const char *dsl);

char *ai_dsl_runner_apply(CanvasData *data,
                          const char *dsl,
                          const AiDslRunnerOptions *options,
                          gboolean *out_applied);

#endif
