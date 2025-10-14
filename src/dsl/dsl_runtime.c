#include <gtk/gtk.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "canvas_core.h"
#include "element.h"
#include "connection.h"
#include "undo_manager.h"
#include "shape.h"
#include "canvas_drop.h"
#include "note.h"
#include "paper_note.h"
#include "animation.h"
#include "inline_text.h"

#include "dsl/dsl_runtime.h"
#include "dsl/dsl_commands.h"
#include "dsl/dsl_utils.h"

#include "canvas_presentation.h"

typedef struct _DSLRuntime DSLRuntime;

typedef struct {
  gchar *var_name;
  gboolean is_position;
} DSLBinding;

typedef struct {
  gboolean is_string;
  gchar *expected_str;
  double expected_value;
  gboolean triggered;
} DSLAutoAdvance;

typedef struct {
  gchar *block_source;
  DSLConditionType condition_type;
  double condition_value;
} DSLVariableHandler;

struct _DSLRuntime {
  GHashTable *variables;          // name -> DSLVariable*
  GHashTable *id_to_model;        // id -> ModelElement*
  GHashTable *model_to_id;        // ModelElement* -> id string
  GHashTable *click_handlers;     // id -> GPtrArray* of char* scripts
  GHashTable *variable_handlers;  // var name -> GPtrArray* of DSLVariableHandler*
  GHashTable *bindings;          // element id -> DSLBinding*
  GHashTable *auto_next;         // var name -> DSLAutoAdvance*
  GQueue *pending_notifications;  // queue of gchar* variable names
  int notification_depth;
};

static void dsl_binding_free(gpointer data) {
  DSLBinding *binding = (DSLBinding *)data;
  if (!binding) return;
  g_free(binding->var_name);
  g_free(binding);
}

static void dsl_auto_advance_free(gpointer data) {
  DSLAutoAdvance *entry = (DSLAutoAdvance *)data;
  if (!entry) return;
  g_free(entry->expected_str);
  g_free(entry);
}

static void dsl_variable_handler_free(gpointer data) {
  DSLVariableHandler *handler = (DSLVariableHandler *)data;
  if (!handler) return;
  g_free(handler->block_source);
  g_free(handler);
}

static void dsl_free_handler_array(gpointer data) {
  if (!data) return;
  GPtrArray *array = (GPtrArray *)data;
  g_ptr_array_free(array, TRUE);
}

static DSLBinding* dsl_runtime_lookup_binding(DSLRuntime *runtime, const gchar *element_id) {
  if (!runtime || !runtime->bindings || !element_id) return NULL;
  return (DSLBinding *)g_hash_table_lookup(runtime->bindings, element_id);
}

static void dsl_runtime_try_auto_next(CanvasData *data, const gchar *var_name);

gchar* dsl_unescape_text(const gchar *str) {
  if (!str) return g_strdup("");
  if (str[0] == '\0') return g_strdup("");

  GString *result = g_string_new(NULL);
  const gchar *p = str;

  while (*p) {
    if (*p == '\\') {
      p++;
      switch (*p) {
      case 'n':
        g_string_append_c(result, '\n');
        break;
      case 'r':
        g_string_append_c(result, '\r');
        break;
      case 't':
        g_string_append_c(result, '\t');
        break;
      case '"':
        g_string_append_c(result, '"');
        break;
      case '\\':
        g_string_append_c(result, '\\');
        break;
      default:
        // Unknown escape sequence, keep both characters
        g_string_append_c(result, '\\');
        g_string_append_c(result, *p);
        break;
      }
      if (*p != '\0') p++;
    } else {
      g_string_append_c(result, *p);
      p++;
    }
  }

  return g_string_free(result, FALSE);
}

static void dsl_free_block_array(gpointer data) {
  if (!data) return;
  GPtrArray *array = (GPtrArray *)data;
  g_ptr_array_free(array, TRUE);
}

static void dsl_variable_free(gpointer data) {
  DSLVariable *var = (DSLVariable *)data;
  if (!var) return;
  g_free(var->string_value);
  g_free(var->expression);
  g_free(var->array_values);
  g_free(var);
}

DSLRuntime* dsl_runtime_get(CanvasData *data) {
  if (!data) return NULL;
  if (!data->dsl_runtime) {
    data->dsl_runtime = g_new0(DSLRuntime, 1);
    data->dsl_runtime->variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, dsl_variable_free);
    data->dsl_runtime->id_to_model = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    data->dsl_runtime->model_to_id = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    data->dsl_runtime->click_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, dsl_free_block_array);
    data->dsl_runtime->variable_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, dsl_free_handler_array);
    data->dsl_runtime->bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, dsl_binding_free);
    data->dsl_runtime->auto_next = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, dsl_auto_advance_free);
    data->dsl_runtime->pending_notifications = g_queue_new();
  }
  return data->dsl_runtime;
}

void dsl_runtime_reset(CanvasData *data) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime) return;

  gboolean keep_globals = data && data->presentation_mode_active;
  if (runtime->variables) {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, runtime->variables);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      DSLVariable *var = (DSLVariable *)value;
      if (!var || !keep_globals || !var->is_global) {
        g_hash_table_iter_remove(&iter);
      } else {
        var->evaluating = FALSE;
      }
    }
  }
  g_hash_table_remove_all(runtime->id_to_model);
  g_hash_table_remove_all(runtime->model_to_id);
  g_hash_table_remove_all(runtime->click_handlers);
  g_hash_table_remove_all(runtime->variable_handlers);
  if (runtime->bindings) {
    g_hash_table_remove_all(runtime->bindings);
  }
  if (runtime->auto_next) {
    g_hash_table_remove_all(runtime->auto_next);
  }

  if (runtime->pending_notifications) {
    g_queue_clear_full(runtime->pending_notifications, g_free);
  }
  runtime->notification_depth = 0;
}

DSLVariable* dsl_runtime_lookup_variable(CanvasData *data, const gchar *name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !name) return NULL;
  return (DSLVariable *)g_hash_table_lookup(runtime->variables, name);
}

void dsl_runtime_seed_global_types(CanvasData *data, GHashTable *dest) {
  if (!data || !dest) return;
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !runtime->variables) return;

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, runtime->variables);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    const gchar *name = (const gchar *)key;
    DSLVariable *var = (DSLVariable *)value;
    if (!var || !var->is_global || var->type == DSL_VAR_UNSET) {
      continue;
    }
    if (!g_hash_table_contains(dest, name)) {
      g_hash_table_insert(dest, g_strdup(name), GINT_TO_POINTER(var->type));
    }
  }
}

DSLVariable* dsl_runtime_ensure_variable(CanvasData *data, const gchar *name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !name) return NULL;

  DSLVariable *var = (DSLVariable *)g_hash_table_lookup(runtime->variables, name);
  if (!var) {
    var = g_new0(DSLVariable, 1);
    var->type = DSL_VAR_UNSET;
    g_hash_table_insert(runtime->variables, g_strdup(name), var);
  }
  return var;
}

typedef struct {
  CanvasData *data;
  const gchar *input;
  const gchar *pos;
  gboolean error;
} ExprParser;

static void expr_skip_ws(ExprParser *parser) {
  while (*(parser->pos) && g_ascii_isspace(*(parser->pos))) {
    parser->pos++;
  }
}

static double expr_parse_expression(ExprParser *parser);

static double expr_parse_factor(ExprParser *parser) {
  expr_skip_ws(parser);
  if (parser->error || !*(parser->pos)) {
    return 0.0;
  }

  if (*(parser->pos) == '+') {
    parser->pos++;
    return expr_parse_factor(parser);
  }

  if (*(parser->pos) == '-') {
    parser->pos++;
    return -expr_parse_factor(parser);
  }

  if (*(parser->pos) == '(') {
    parser->pos++;
    double value = expr_parse_expression(parser);
    expr_skip_ws(parser);
    if (*(parser->pos) == ')') {
      parser->pos++;
    } else {
      parser->error = TRUE;
    }
    return value;
  }

  if (g_ascii_isdigit(*(parser->pos)) || *(parser->pos) == '.') {
    gchar *end_ptr = NULL;
    double value = g_ascii_strtod(parser->pos, &end_ptr);
    if (end_ptr == parser->pos) {
      parser->error = TRUE;
      return 0.0;
    }
    parser->pos = end_ptr;
    return value;
  }

  if (g_ascii_isalpha(*(parser->pos)) || *(parser->pos) == '_') {
    const gchar *start = parser->pos;
    parser->pos++;
    while (*(parser->pos) && (g_ascii_isalnum(*(parser->pos)) || *(parser->pos) == '_')) {
      parser->pos++;
    }
    gchar *name = g_strndup(start, parser->pos - start);

    // Check for array access: var[index]
    expr_skip_ws(parser);
    if (*(parser->pos) == '[') {
      parser->pos++; // Skip '['
      double index_val = expr_parse_expression(parser);
      expr_skip_ws(parser);
      if (*(parser->pos) == ']') {
        parser->pos++; // Skip ']'
      } else {
        parser->error = TRUE;
        g_free(name);
        return 0.0;
      }

      int index = (int)index_val;
      double value = dsl_runtime_get_array_element(parser->data, name, index);
      g_free(name);
      return value;
    }

    // Regular variable access
    DSLVariable *var = dsl_runtime_lookup_variable(parser->data, name);
    double value = 0.0;
    if (var) {
      if (var->type == DSL_VAR_INT || var->type == DSL_VAR_REAL || var->type == DSL_VAR_BOOL) {
        value = var->numeric_value;
      } else if (var->type == DSL_VAR_ARRAY) {
        g_print("DSL: Array '%s' requires index access, treating as 0\n", name);
        value = 0.0;
      } else {
        g_print("DSL: Variable '%s' is not numeric, treating as 0\n", name);
        value = 0.0;
      }
    } else {
      g_print("DSL: Unknown variable '%s', defaulting to 0\n", name);
      value = 0.0;
    }
    g_free(name);
    return value;
  }

  parser->error = TRUE;
  return 0.0;
}

static double expr_parse_term(ExprParser *parser) {
  double value = expr_parse_factor(parser);
  while (!parser->error) {
    expr_skip_ws(parser);
    char op = *(parser->pos);
    if (op != '*' && op != '/') break;
    parser->pos++;
    double rhs = expr_parse_factor(parser);
    if (parser->error) break;
    if (op == '*') {
      value *= rhs;
    } else {
      if (fabs(rhs) < 1e-9) {
        g_print("DSL: Division by zero in expression '%s'\n", parser->input);
        parser->error = TRUE;
        return 0.0;
      }
      value /= rhs;
    }
  }
  return value;
}

static double expr_parse_additive(ExprParser *parser) {
  double value = expr_parse_term(parser);
  while (!parser->error) {
    expr_skip_ws(parser);
    char op = *(parser->pos);
    if (op != '+' && op != '-') break;
    parser->pos++;
    double rhs = expr_parse_term(parser);
    if (parser->error) break;
    if (op == '+') {
      value += rhs;
    } else {
      value -= rhs;
    }
  }
  return value;
}

static double expr_parse_comparison(ExprParser *parser) {
  double value = expr_parse_additive(parser);
  while (!parser->error) {
    expr_skip_ws(parser);
    const gchar *pos = parser->pos;

    if (pos[0] == '=' && pos[1] == '=') {
      parser->pos += 2;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (fabs(value - rhs) < 1e-9) ? 1.0 : 0.0;
    } else if (pos[0] == '!' && pos[1] == '=') {
      parser->pos += 2;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (fabs(value - rhs) >= 1e-9) ? 1.0 : 0.0;
    } else if (pos[0] == '<' && pos[1] == '=') {
      parser->pos += 2;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (value <= rhs) ? 1.0 : 0.0;
    } else if (pos[0] == '>' && pos[1] == '=') {
      parser->pos += 2;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (value >= rhs) ? 1.0 : 0.0;
    } else if (pos[0] == '<') {
      parser->pos += 1;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (value < rhs) ? 1.0 : 0.0;
    } else if (pos[0] == '>') {
      parser->pos += 1;
      double rhs = expr_parse_additive(parser);
      if (parser->error) break;
      value = (value > rhs) ? 1.0 : 0.0;
    } else {
      break;
    }
  }
  return value;
}

static double expr_parse_expression(ExprParser *parser) {
  return expr_parse_comparison(parser);
}

gboolean dsl_evaluate_expression(CanvasData *data, const gchar *expr, double *out_value) {
  if (!data || !expr || !out_value) return FALSE;
  ExprParser parser = {
    .data = data,
    .input = expr,
    .pos = expr,
    .error = FALSE,
  };

  double result = expr_parse_expression(&parser);
  expr_skip_ws(&parser);
  if (*(parser.pos) != '\0') {
    parser.error = TRUE;
  }

  if (parser.error) {
    g_print("DSL: Failed to evaluate expression '%s'\n", expr);
    return FALSE;
  }

  *out_value = result;
  return TRUE;
}

static void dsl_runtime_enqueue_notification(DSLRuntime *runtime, const gchar *var_name) {
  if (!runtime || !var_name) return;
  if (!runtime->pending_notifications) {
    runtime->pending_notifications = g_queue_new();
  }
  g_queue_push_tail(runtime->pending_notifications, g_strdup(var_name));
}

static void dsl_runtime_execute_variable_handlers(CanvasData *data, const gchar *var_name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !var_name) return;

  GPtrArray *handlers = (GPtrArray *)g_hash_table_lookup(runtime->variable_handlers, var_name);
  if (!handlers || handlers->len == 0) {
    dsl_runtime_try_auto_next(data, var_name);
    return;
  }

  // Get current variable value
  DSLVariable *var = dsl_runtime_lookup_variable(data, var_name);
  double current_value = var ? var->numeric_value : 0.0;

  // Take a defensive copy because executing a handler can reset the DSL runtime
  // and free the original handler array.
  GPtrArray *snapshot = g_ptr_array_new_with_free_func(dsl_variable_handler_free);
  for (guint i = 0; i < handlers->len; i++) {
    DSLVariableHandler *handler = (DSLVariableHandler *)g_ptr_array_index(handlers, i);
    if (handler && handler->block_source) {
      DSLVariableHandler *copy = g_new0(DSLVariableHandler, 1);
      copy->block_source = g_strdup(handler->block_source);
      copy->condition_type = handler->condition_type;
      copy->condition_value = handler->condition_value;
      g_ptr_array_add(snapshot, copy);
    }
  }

  for (guint i = 0; i < snapshot->len; i++) {
    DSLVariableHandler *handler = (DSLVariableHandler *)g_ptr_array_index(snapshot, i);
    if (handler && handler->block_source) {
      // Check condition if present
      gboolean condition_met = FALSE;
      switch (handler->condition_type) {
        case DSL_COND_NONE:
          condition_met = TRUE;
          break;
        case DSL_COND_EQUAL:
          condition_met = fabs(current_value - handler->condition_value) < 1e-9;
          break;
        case DSL_COND_NOT_EQUAL:
          condition_met = fabs(current_value - handler->condition_value) >= 1e-9;
          break;
        case DSL_COND_LESS_THAN:
          condition_met = current_value < handler->condition_value;
          break;
        case DSL_COND_LESS_EQUAL:
          condition_met = current_value <= handler->condition_value;
          break;
        case DSL_COND_GREATER_THAN:
          condition_met = current_value > handler->condition_value;
          break;
        case DSL_COND_GREATER_EQUAL:
          condition_met = current_value >= handler->condition_value;
          break;
      }

      if (condition_met) {
        dsl_execute_command_block(data, handler->block_source);
      }
    }
  }

  g_ptr_array_free(snapshot, TRUE);

  dsl_runtime_try_auto_next(data, var_name);
}

void dsl_runtime_flush_notifications(CanvasData *data) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !runtime->pending_notifications) return;

  runtime->notification_depth++;

  if (runtime->notification_depth > 5) {
    runtime->notification_depth--;
    return;
  }

  while (!g_queue_is_empty(runtime->pending_notifications)) {
    gchar *var_name = (gchar *)g_queue_pop_head(runtime->pending_notifications);
    if (var_name) {
      dsl_runtime_execute_variable_handlers(data, var_name);
      g_free(var_name);
    }
  }
  runtime->notification_depth--;
  if (runtime->notification_depth < 0) runtime->notification_depth = 0;
}

void dsl_runtime_notify_variable(CanvasData *data, const gchar *var_name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !var_name) return;
  dsl_runtime_enqueue_notification(runtime, var_name);
}

gboolean dsl_runtime_set_variable(CanvasData *data, const gchar *name, double value, gboolean trigger_watchers) {
  DSLVariable *var = dsl_runtime_lookup_variable(data, name);
  if (!var) {
    g_print("DSL: Attempted to set unknown variable '%s'\n", name ? name : "(null)");
    return FALSE;
  }

  double new_value = value;
  gboolean changed = FALSE;

  switch (var->type) {
    case DSL_VAR_INT: {
      new_value = round(value);
      if (fabs(var->numeric_value - new_value) >= 1e-6) {
        var->numeric_value = new_value;
        changed = TRUE;
      }
      break;
    }
    case DSL_VAR_REAL: {
      if (fabs(var->numeric_value - value) >= 1e-6) {
        var->numeric_value = value;
        changed = TRUE;
      }
      break;
    }
    case DSL_VAR_BOOL: {
      double coerced = (value != 0.0) ? 1.0 : 0.0;
      if (fabs(var->numeric_value - coerced) >= 1e-6) {
        var->numeric_value = coerced;
        changed = TRUE;
      }
      break;
    }
    case DSL_VAR_STRING: {
      g_print("DSL: Cannot assign numeric value to string variable '%s'\n", name);
      return FALSE;
    }
    case DSL_VAR_ARRAY: {
      g_print("DSL: Cannot assign single value to array variable '%s'\n", name);
      return FALSE;
    }
    case DSL_VAR_UNSET: {
      var->type = DSL_VAR_REAL;
      var->numeric_value = value;
      changed = TRUE;
      break;
    }
  }

  if (changed || trigger_watchers) {
    if (trigger_watchers) {
      dsl_runtime_notify_variable(data, name);
      DSLRuntime *runtime = dsl_runtime_get(data);
      if (runtime && runtime->notification_depth == 0) {
        dsl_runtime_flush_notifications(data);
      }
    } else {
      dsl_runtime_try_auto_next(data, name);
    }
  }
  return TRUE;
}

gboolean dsl_runtime_set_string_variable(CanvasData *data, const gchar *name, const gchar *value, gboolean trigger_watchers) {
  DSLVariable *var = dsl_runtime_lookup_variable(data, name);
  if (!var) {
    g_print("DSL: Attempted to set unknown variable '%s'\n", name ? name : "(null)");
    return FALSE;
  }

  if (var->type != DSL_VAR_STRING && var->type != DSL_VAR_UNSET) {
    g_print("DSL: Cannot assign string value to non-string variable '%s'\n", name);
    return FALSE;
  }

  if (var->type == DSL_VAR_UNSET) {
    var->type = DSL_VAR_STRING;
  }

  if (g_strcmp0(var->string_value, value) == 0) {
    return TRUE;
  }

  g_free(var->string_value);
  var->string_value = g_strdup(value);

  if (trigger_watchers) {
    dsl_runtime_notify_variable(data, name);
    DSLRuntime *runtime = dsl_runtime_get(data);
    if (runtime && runtime->notification_depth == 0) {
      dsl_runtime_flush_notifications(data);
    }
  } else {
    dsl_runtime_try_auto_next(data, name);
  }
  return TRUE;
}

gboolean dsl_runtime_set_array_element(CanvasData *data, const gchar *name, int index, double value, gboolean trigger_watchers) {
  DSLVariable *var = dsl_runtime_lookup_variable(data, name);
  if (!var) {
    g_print("DSL: Attempted to set element in unknown variable '%s'\n", name ? name : "(null)");
    return FALSE;
  }

  if (var->type != DSL_VAR_ARRAY) {
    g_print("DSL: Variable '%s' is not an array\n", name);
    return FALSE;
  }

  if (index < 0 || index >= var->array_size) {
    g_print("DSL: Array index %d out of bounds for '%s' (size %d)\n", index, name, var->array_size);
    return FALSE;
  }

  var->array_values[index] = value;

  if (trigger_watchers) {
    dsl_runtime_notify_variable(data, name);
    DSLRuntime *runtime = dsl_runtime_get(data);
    if (runtime && runtime->notification_depth == 0) {
      dsl_runtime_flush_notifications(data);
    }
  }
  return TRUE;
}

double dsl_runtime_get_array_element(CanvasData *data, const gchar *name, int index) {
  DSLVariable *var = dsl_runtime_lookup_variable(data, name);
  if (!var) {
    g_print("DSL: Attempted to access unknown variable '%s'\n", name ? name : "(null)");
    return 0.0;
  }

  if (var->type != DSL_VAR_ARRAY) {
    g_print("DSL: Variable '%s' is not an array\n", name);
    return 0.0;
  }

  if (index < 0 || index >= var->array_size) {
    g_print("DSL: Array index %d out of bounds for '%s' (size %d)\n", index, name, var->array_size);
    return 0.0;
  }

  return var->array_values[index];
}

gboolean dsl_runtime_recompute_expressions(CanvasData *data) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime) return FALSE;

  GHashTableIter iter;
  gpointer key, value;
  gboolean success = TRUE;

  g_hash_table_iter_init(&iter, runtime->variables);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    const gchar *name = (const gchar *)key;
    DSLVariable *var = (DSLVariable *)value;
    if (!var || !var->expression) continue;
    if (var->type == DSL_VAR_STRING) continue;
    if (var->evaluating) continue; // Prevent recursion

    var->evaluating = TRUE;
    double new_value = var->numeric_value;
    gboolean ok = dsl_evaluate_expression(data, var->expression, &new_value);
    var->evaluating = FALSE;
    if (!ok) {
      success = FALSE;
      continue;
    }

    if (var->type == DSL_VAR_BOOL) {
      dsl_runtime_set_variable(data, name, new_value != 0.0 ? 1.0 : 0.0, TRUE);
    } else {
      dsl_runtime_set_variable(data, name, new_value, TRUE);
    }
  }

  return success;
}

gchar* dsl_resolve_numeric_token(CanvasData *data, const gchar *token) {
  if (!token) return NULL;
  if (!strchr(token, '{')) {
    return g_strdup(token);
  }

  GString *resolved = g_string_new(NULL);
  const gchar *p = token;

  while (*p) {
    if (*p == '{') {
      p++;
      int depth = 1;
      const gchar *start = p;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        if (depth > 0) p++;
      }

      if (depth != 0) {
        g_string_free(resolved, TRUE);
        return g_strdup(token);
      }

      size_t len = (size_t)(p - start);
      gchar *expr = g_strndup(start, len);

      // Check if expression contains commas (tuple notation)
      if (strchr(expr, ',')) {
        // Handle tuple: {expr1,expr2,expr3,...}
        // Split by commas and evaluate each part
        g_string_append_c(resolved, '(');
        gchar **parts = g_strsplit(expr, ",", -1);
        for (int j = 0; parts[j] != NULL; j++) {
          if (j > 0) g_string_append_c(resolved, ',');

          gchar *trimmed = g_strstrip(g_strdup(parts[j]));
          double value = 0.0;
          if (!dsl_evaluate_expression(data, trimmed, &value)) {
            value = 0.0;
          }
          g_free(trimmed);

          // Format as decimal for floats, integer otherwise
          if (fabs(value - round(value)) < 1e-9) {
            g_string_append_printf(resolved, "%.0f", value);
          } else {
            g_string_append_printf(resolved, "%.2f", value);
          }
        }
        g_strfreev(parts);
        g_string_append_c(resolved, ')');
      } else {
        // Single expression: {expr}
        double value = 0.0;
        if (!dsl_evaluate_expression(data, expr, &value)) {
          value = 0.0;
        }
        if (fabs(value - round(value)) < 1e-9) {
          g_string_append_printf(resolved, "%.0f", value);
        } else {
          g_string_append_printf(resolved, "%.6f", value);
        }
      }
      g_free(expr);

      if (*p == '}') p++;
    } else {
      g_string_append_c(resolved, *p);
      p++;
    }
  }

  return g_string_free(resolved, FALSE);
}

gboolean dsl_parse_point_token(CanvasData *data, const gchar *token, int *out_x, int *out_y) {
  if (!token || !out_x || !out_y) return FALSE;
  gchar *resolved = dsl_resolve_numeric_token(data, token);
  gboolean ok = FALSE;
  if (resolved) {
    ok = parse_point(resolved, out_x, out_y);
  }
  g_free(resolved);
  return ok;
}

void dsl_runtime_register_text_binding(CanvasData *data, const gchar *element_id, const gchar *var_name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element_id || !var_name) return;
  if (!g_hash_table_lookup(runtime->variables, var_name)) {
    g_print("DSL: Cannot bind unknown variable '%s'\n", var_name);
    return;
  }
  DSLBinding *binding = dsl_runtime_lookup_binding(runtime, element_id);
  if (!binding) {
    binding = g_new0(DSLBinding, 1);
  }
  g_free(binding->var_name);
  binding->var_name = g_strdup(var_name);
  binding->is_position = FALSE;
  g_hash_table_replace(runtime->bindings, g_strdup(element_id), binding);

  ModelElement *element = dsl_runtime_lookup_element(data, element_id);
  if (element && element->visual_element && element->visual_element->type == ELEMENT_INLINE_TEXT) {
    DSLVariable *var = dsl_runtime_lookup_variable(data, var_name);
    const char *value = (var && var->type == DSL_VAR_STRING && var->string_value) ? var->string_value : "";
    dsl_runtime_text_update(data, element, value);
  }
}

void dsl_runtime_register_position_binding(CanvasData *data, const gchar *element_id, const gchar *var_name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element_id || !var_name) return;
  if (!g_hash_table_lookup(runtime->variables, var_name)) {
    g_print("DSL: Cannot bind position to unknown variable '%s'\n", var_name);
    return;
  }
  DSLBinding *binding = dsl_runtime_lookup_binding(runtime, element_id);
  if (!binding) {
    binding = g_new0(DSLBinding, 1);
  }
  g_free(binding->var_name);
  binding->var_name = g_strdup(var_name);
  binding->is_position = TRUE;
  g_hash_table_replace(runtime->bindings, g_strdup(element_id), binding);

  ModelElement *element = dsl_runtime_lookup_element(data, element_id);
  if (element && element->position) {
    gchar *value = g_strdup_printf("%d,%d", element->position->x, element->position->y);
    dsl_runtime_set_string_variable(data, var_name, value, FALSE);
    g_free(value);
  }

  dsl_runtime_try_auto_next(data, var_name);
}

void dsl_runtime_element_moved(CanvasData *data, ModelElement *model_element) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !model_element || !model_element->uuid) return;

  const gchar *element_id = dsl_runtime_lookup_element_id(data, model_element);
  if (!element_id) return;

  DSLBinding *binding = dsl_runtime_lookup_binding(runtime, element_id);
  if (!binding || !binding->is_position || !binding->var_name) return;

  if (model_element->position) {
    gchar *value = g_strdup_printf("%d,%d", model_element->position->x, model_element->position->y);
    dsl_runtime_set_string_variable(data, binding->var_name, value, TRUE);
    g_free(value);
  }
}

void dsl_runtime_inline_text_updated(CanvasData *data, Element *element, const gchar *text) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element || !text) return;
  ModelElement *model_element = model_get_by_visual(data->model, element);
  if (!model_element || !model_element->uuid) return;

  const gchar *element_id = dsl_runtime_lookup_element_id(data, model_element);
  if (!element_id) return;

  DSLBinding *binding = dsl_runtime_lookup_binding(runtime, element_id);
  if (!binding || binding->is_position || !binding->var_name) return;

  gchar *sanitized = g_strdup(text);
  if (sanitized) {
    g_strstrip(sanitized);
    dsl_runtime_set_string_variable(data, binding->var_name, sanitized, TRUE);
    g_free(sanitized);
  }
}

void dsl_runtime_register_auto_next(CanvasData *data, const gchar *var_name, gboolean is_string, const gchar *expected_str, double expected_value) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !var_name) return;
  if (!g_hash_table_lookup(runtime->variables, var_name)) {
    g_print("DSL: presentation_auto_next_if references unknown variable '%s'\n", var_name);
    return;
  }

  DSLAutoAdvance *entry = (DSLAutoAdvance *)g_hash_table_lookup(runtime->auto_next, var_name);
  if (!entry) {
    entry = g_new0(DSLAutoAdvance, 1);
    g_hash_table_insert(runtime->auto_next, g_strdup(var_name), entry);
  } else {
    g_free(entry->expected_str);
  }

  entry->is_string = is_string;
  entry->expected_str = is_string ? g_strdup(expected_str ? expected_str : "") : NULL;
  entry->expected_value = expected_value;
  entry->triggered = FALSE;

  dsl_runtime_try_auto_next(data, var_name);
}

static void dsl_runtime_try_auto_next(CanvasData *data, const gchar *var_name) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !var_name || !runtime->auto_next) return;

  DSLAutoAdvance *entry = (DSLAutoAdvance *)g_hash_table_lookup(runtime->auto_next, var_name);
  if (!entry || entry->triggered) return;

  DSLVariable *var = dsl_runtime_lookup_variable(data, var_name);
  if (!var) return;

  gboolean matched = FALSE;
  if (entry->is_string) {
    const gchar *current = (var->type == DSL_VAR_STRING && var->string_value) ? var->string_value : "";
    matched = g_strcmp0(current, entry->expected_str ? entry->expected_str : "") == 0;
  } else {
    double value = var->numeric_value;
    matched = fabs(value - entry->expected_value) < 1e-6;
  }

  if (matched) {
    entry->triggered = TRUE;
    canvas_presentation_request_auto_next(data);
  }
}

gboolean dsl_parse_double_token(CanvasData *data, const gchar *token, double *out_value) {
  if (!token || !out_value) return FALSE;

  size_t len = strlen(token);
  if (len >= 2 && token[0] == '{' && token[len - 1] == '}') {
    gchar *expr = g_strndup(token + 1, len - 2);
    gboolean ok = dsl_evaluate_expression(data, expr, out_value);
    g_free(expr);
    return ok;
  }

  return parse_double_value(token, out_value);
}

gchar* dsl_interpolate_text(CanvasData *data, const gchar *input) {
  if (!input) return g_strdup("");
  const gchar *marker = strstr(input, "${");
  if (!marker) return g_strdup(input);

  GString *result = g_string_new(NULL);
  const gchar *p = input;

  while (*p) {
    if (*p == '$' && *(p + 1) == '{') {
      p += 2; // Skip '${'
      int depth = 1;
      const gchar *start = p;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        if (depth > 0) p++;
      }

      if (depth != 0) {
        g_string_free(result, TRUE);
        return g_strdup(input);
      }

      size_t len = (size_t)(p - start);
      gchar *expr = g_strndup(start, len);
      double value = 0.0;
      if (!dsl_evaluate_expression(data, expr, &value)) {
        value = 0.0;
      }
      g_free(expr);

      if (*p == '}') p++;

      double rounded = round(value);
      if (fabs(value - rounded) < 1e-6) {
        g_string_append_printf(result, "%.0f", rounded);
      } else {
        char buffer[G_ASCII_DTOSTR_BUF_SIZE];
        g_ascii_dtostr(buffer, sizeof(buffer), value);
        g_string_append(result, buffer);
      }
    } else {
      g_string_append_c(result, *p);
      p++;
    }
  }

  return g_string_free(result, FALSE);
}

void dsl_runtime_register_element(CanvasData *data, const gchar *id, ModelElement *element) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !id || !element) return;

  g_hash_table_replace(runtime->id_to_model, g_strdup(id), element);
  g_hash_table_replace(runtime->model_to_id, element, g_strdup(id));

  if (data && data->dsl_aliases && element->uuid && *element->uuid &&
      g_strcmp0(id, element->uuid) != 0) {
    g_hash_table_insert(data->dsl_aliases, g_strdup(id), g_strdup(element->uuid));
  }
}

ModelElement* dsl_runtime_lookup_element(CanvasData *data, const gchar *id) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !id) return NULL;
  ModelElement *element = (ModelElement *)g_hash_table_lookup(runtime->id_to_model, id);
  if (element) {
    return element;
  }

  if (data && data->model && data->model->elements) {
    element = g_hash_table_lookup(data->model->elements, id);
    if (element) {
      dsl_runtime_register_element(data, id, element);
    }
  }
  return element;
}

const gchar* dsl_runtime_lookup_element_id(CanvasData *data, ModelElement *element) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element) return NULL;
  return (const gchar *)g_hash_table_lookup(runtime->model_to_id, element);
}

void dsl_runtime_add_click_handler(CanvasData *data, const gchar *element_id, gchar *block_source) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element_id || !block_source) return;

  GPtrArray *handlers = (GPtrArray *)g_hash_table_lookup(runtime->click_handlers, element_id);
  if (!handlers) {
    handlers = g_ptr_array_new_with_free_func(g_free);
    g_hash_table_insert(runtime->click_handlers, g_strdup(element_id), handlers);
  }
  g_ptr_array_add(handlers, block_source);
}

void dsl_runtime_add_variable_handler(CanvasData *data, const gchar *var_name, gchar *block_source) {
  dsl_runtime_add_variable_handler_conditional(data, var_name, block_source, DSL_COND_NONE, 0.0);
}

void dsl_runtime_add_variable_handler_conditional(CanvasData *data, const gchar *var_name, gchar *block_source, DSLConditionType condition_type, double condition_value) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !var_name || !block_source) return;

  GPtrArray *handlers = (GPtrArray *)g_hash_table_lookup(runtime->variable_handlers, var_name);
  if (!handlers) {
    handlers = g_ptr_array_new_with_free_func(dsl_variable_handler_free);
    g_hash_table_insert(runtime->variable_handlers, g_strdup(var_name), handlers);
  }

  DSLVariableHandler *handler = g_new0(DSLVariableHandler, 1);
  handler->block_source = block_source;
  handler->condition_type = condition_type;
  handler->condition_value = condition_value;
  g_ptr_array_add(handlers, handler);
}

gboolean dsl_runtime_handle_click(CanvasData *data, const gchar *element_id) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime || !element_id) return FALSE;

  GPtrArray *handlers = (GPtrArray *)g_hash_table_lookup(runtime->click_handlers, element_id);
  if (!handlers || handlers->len == 0) {
    return FALSE;
  }

  // Copy handler blocks because callbacks can advance slides and reset the runtime.
  GPtrArray *snapshot = g_ptr_array_new_with_free_func(g_free);
  for (guint i = 0; i < handlers->len; i++) {
    const gchar *block = (const gchar *)g_ptr_array_index(handlers, i);
    if (block) {
      g_ptr_array_add(snapshot, g_strdup(block));
    }
  }

  gboolean handled = FALSE;
  for (guint i = 0; i < snapshot->len; i++) {
    const gchar *block = (const gchar *)g_ptr_array_index(snapshot, i);
    if (block) {
      dsl_execute_command_block(data, block);
      handled = TRUE;
    }
  }

  g_ptr_array_free(snapshot, TRUE);
  return handled;
}

GHashTable* dsl_runtime_get_click_handlers(CanvasData *data) {
  DSLRuntime *runtime = dsl_runtime_get(data);
  if (!runtime) return NULL;
  return runtime->click_handlers;
}

void dsl_runtime_prepare_animation_engine(CanvasData *data) {
  if (!data) return;

  if (!data->anim_engine) {
    data->anim_engine = g_malloc0(sizeof(AnimationEngine));
  } else {
    animation_engine_stop(data->anim_engine);
    animation_engine_cleanup(data->anim_engine);
  }
  animation_engine_init(data->anim_engine, FALSE);
}

void dsl_runtime_add_move_animation(CanvasData *data, ModelElement *model_element,
                                           int from_x, int from_y, int to_x, int to_y,
                                           double start_time, double duration,
                                           AnimInterpolationType interp) {
  if (!data || !model_element || !model_element->uuid || !data->anim_engine) return;

  if (data->undo_manager && (from_x != to_x || from_y != to_y)) {
    undo_manager_push_move_action(data->undo_manager,
                                  model_element,
                                  from_x, from_y,
                                  to_x, to_y);
  }

  animation_add_move(data->anim_engine, model_element->uuid,
                     start_time, duration, interp,
                     from_x, from_y, to_x, to_y);
}

void dsl_runtime_add_resize_animation(CanvasData *data, ModelElement *model_element,
                                             int from_w, int from_h, int to_w, int to_h,
                                             double start_time, double duration,
                                             AnimInterpolationType interp) {
  if (!data || !model_element || !model_element->uuid || !data->anim_engine) return;

  animation_add_resize(data->anim_engine, model_element->uuid,
                       start_time, duration, interp,
                       from_w, from_h, to_w, to_h);

  if (model_element->size) {
    model_element->size->width = to_w;
    model_element->size->height = to_h;
  }

  if (model_element->visual_element) {
    element_update_size(model_element->visual_element, to_w, to_h);
  }
}

void dsl_runtime_add_rotate_animation(CanvasData *data, ModelElement *model_element,
                                             double from_rotation, double to_rotation,
                                             double start_time, double duration,
                                             AnimInterpolationType interp) {
  if (!data || !model_element || !model_element->uuid || !data->anim_engine) return;

  animation_add_rotate(data->anim_engine, model_element->uuid,
                       start_time, duration, interp,
                       from_rotation, to_rotation);
}

void dsl_runtime_text_update(CanvasData *data, ModelElement *model_element, const gchar *new_text) {
  if (!data || !model_element || !new_text) return;

  const gchar *old_text_ptr = (model_element->text && model_element->text->text)
                                ? model_element->text->text
                                : "";
  gboolean changed = g_strcmp0(old_text_ptr, new_text) != 0;
  gchar *old_text_copy = g_strdup(old_text_ptr);
  g_message("DSL text_update target=%s old='%s' new='%s'", model_element->uuid, old_text_ptr, new_text);

  Element *element = model_element->visual_element;
  gboolean element_changed = FALSE;

  if (element) {
    switch (element->type) {
      case ELEMENT_INLINE_TEXT: {
        InlineText *inline_text = (InlineText *)element;
        if (g_strcmp0(inline_text->text, new_text) != 0) {
          g_free(inline_text->text);
          inline_text->text = g_strdup(new_text);
          inline_text_update_layout(inline_text);
          element_changed = TRUE;
        }
        break;
      }
      case ELEMENT_NOTE: {
        Note *note = (Note *)element;
        if (g_strcmp0(note->text, new_text) != 0) {
          g_free(note->text);
          note->text = g_strdup(new_text);
          element_changed = TRUE;
        }
        break;
      }
      case ELEMENT_PAPER_NOTE: {
        PaperNote *paper = (PaperNote *)element;
        if (g_strcmp0(paper->text, new_text) != 0) {
          g_free(paper->text);
          paper->text = g_strdup(new_text);
          element_changed = TRUE;
        }
        break;
      }
      case ELEMENT_SHAPE: {
        Shape *shape = (Shape *)element;
        if (g_strcmp0(shape->text, new_text) != 0) {
          g_free(shape->text);
          shape->text = g_strdup(new_text);
          element_changed = TRUE;
        }
        break;
      }
      default:
        break;
    }
  }

  int model_result = model_update_text(data->model, model_element, new_text);
  if (model_result > 0) {
    changed = TRUE;
  }

  if (element_changed) {
    changed = TRUE;
  }

  if (changed && data->undo_manager) {
    undo_manager_push_text_action(data->undo_manager,
                                  model_element,
                                  old_text_copy ? old_text_copy : "",
                                  new_text);
  }

  if (changed) {
    g_message("DSL text_update applied; model text now '%s'", model_element->text && model_element->text->text ? model_element->text->text : "<null>");
    canvas_sync_with_model(data);
    gtk_widget_queue_draw(data->drawing_area);
  }

  g_free(old_text_copy);
}
