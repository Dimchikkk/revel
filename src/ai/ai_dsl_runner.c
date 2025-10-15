#include "ai_dsl_runner.h"

#include "dsl/dsl_type_checker.h"
#include "dsl/dsl_executor.h"
#include "undo_manager.h"

static char *validate_inputs(CanvasData *data, const char *dsl) {
  if (!data) {
    return g_strdup("Missing canvas context");
  }
  if (!dsl || !*dsl) {
    return g_strdup("AI response is empty");
  }
  return NULL;
}

char *ai_dsl_runner_validate(CanvasData *data, const char *dsl) {
  char *input_error = validate_inputs(data, dsl);
  if (input_error) {
    return input_error;
  }

  GPtrArray *type_errors = NULL;
  if (!dsl_type_check_script(data, dsl, NULL, &type_errors)) {
    GString *message = g_string_new("DSL type check failed");
    if (type_errors && type_errors->len > 0) {
      g_string_append(message, ": ");
      for (guint i = 0; i < type_errors->len; i++) {
        const gchar *error = g_ptr_array_index(type_errors, i);
        if (i > 0) {
          g_string_append(message, " | ");
        }
        g_string_append(message, error);
      }
    }
    if (type_errors) {
      g_ptr_array_free(type_errors, TRUE);
    }
    return g_string_free(message, FALSE);
  }
  if (type_errors) {
    g_ptr_array_free(type_errors, TRUE);
  }
  return NULL;
}

static void rollback_to_length(UndoManager *undo_manager, guint target_length) {
  if (!undo_manager) {
    return;
  }
  while (g_list_length(undo_manager->undo_stack) > target_length) {
    undo_manager_undo(undo_manager);
  }
}

char *ai_dsl_runner_apply(CanvasData *data,
                          const char *dsl,
                          const AiDslRunnerOptions *options,
                          gboolean *out_applied) {
  if (out_applied) {
    *out_applied = FALSE;
  }

  char *error = ai_dsl_runner_validate(data, dsl);
  if (error) {
    return error;
  }

  UndoManager *undo_manager = data ? data->undo_manager : NULL;
  guint undo_before = undo_manager ? g_list_length(undo_manager->undo_stack) : 0;

  // Don't reset DSL runtime - preserve element IDs across AI turns
  canvas_execute_script_internal(data, dsl, NULL, TRUE);

  guint undo_after = undo_manager ? g_list_length(undo_manager->undo_stack) : 0;

  if (undo_manager && undo_after < undo_before) {
    rollback_to_length(undo_manager, undo_before);
    return g_strdup("DSL execution failed; changes rolled back");
  }

  if (out_applied) {
    *out_applied = (undo_after > undo_before);
  }

  return NULL;
}
