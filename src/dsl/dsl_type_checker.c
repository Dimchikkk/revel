#include <glib.h>
#include <string.h>

#include "dsl/dsl_utils.h"
#include "dsl/dsl_runtime.h"
#include "dsl/dsl_type_checker.h"

typedef struct {
  GHashTable *variables;  // name -> DSLVarType (stored as gpointer)
  GHashTable *elements;   // id -> gpointer
  GPtrArray *errors;      // array of gchar*
  const gchar *filename;  // filename for error messages (can be NULL)
} DSLTypeCheckerContext;

static void dsl_type_add_error(DSLTypeCheckerContext *ctx, int line, const gchar *fmt, ...) {
  if (!ctx || !ctx->errors || !fmt) return;

  va_list args;
  va_start(args, fmt);
  gchar *message = g_strdup_vprintf(fmt, args);
  va_end(args);

  // Emacs-compatible format: FILE:LINE:COLUMN: message
  // We use column 1 since we don't track column numbers
  gchar *full;
  if (ctx->filename) {
    full = g_strdup_printf("%s:%d:1: %s", ctx->filename, line, message);
  } else {
    full = g_strdup_printf("Line %d: %s", line, message);
  }
  g_free(message);
  g_ptr_array_add(ctx->errors, full);
}

static gboolean dsl_type_check_interpolation(const gchar *interp) {
  return (g_ascii_strcasecmp(interp, "immediate") == 0 ||
          g_ascii_strcasecmp(interp, "linear") == 0 ||
          g_ascii_strcasecmp(interp, "bezier") == 0 ||
          g_ascii_strcasecmp(interp, "ease-in") == 0 ||
          g_ascii_strcasecmp(interp, "ease-out") == 0 ||
          g_ascii_strcasecmp(interp, "bounce") == 0 ||
          g_ascii_strcasecmp(interp, "elastic") == 0 ||
          g_ascii_strcasecmp(interp, "back") == 0 ||
          g_ascii_strcasecmp(interp, "curve") == 0);
}

static void dsl_type_validate_interpolation(DSLTypeCheckerContext *ctx, const gchar *interp, int line, const gchar *command) {
  if (!dsl_type_check_interpolation(interp)) {
    dsl_type_add_error(ctx, line, "%s interpolation must be immediate, linear, bezier, ease-in, ease-out, bounce, elastic, or back", command);
  }
}

static gboolean dsl_type_register_variable(DSLTypeCheckerContext *ctx, const gchar *name, int line, DSLVarType type) {
  if (!ctx || !ctx->variables || !name) return FALSE;
  if (g_hash_table_contains(ctx->variables, name)) {
    dsl_type_add_error(ctx, line, "Variable '%s' already defined", name);
    return FALSE;
  }
  g_hash_table_insert(ctx->variables, g_strdup(name), GINT_TO_POINTER(type));
  return TRUE;
}

static gboolean dsl_type_register_element(DSLTypeCheckerContext *ctx, const gchar *name, int line) {
  if (!ctx || !ctx->elements || !name) return FALSE;
  if (g_hash_table_contains(ctx->elements, name)) {
    dsl_type_add_error(ctx, line, "Element '%s' already defined", name);
    return FALSE;
  }
  g_hash_table_insert(ctx->elements, g_strdup(name), GINT_TO_POINTER(1));
  return TRUE;
}

static DSLVarType dsl_type_lookup_variable_type(DSLTypeCheckerContext *ctx, const gchar *name) {
  if (!ctx || !ctx->variables || !name) return DSL_VAR_UNSET;
  gpointer value = g_hash_table_lookup(ctx->variables, name);
  return (DSLVarType)GPOINTER_TO_INT(value);
}

static gboolean dsl_type_require_variable(DSLTypeCheckerContext *ctx, const gchar *name, int line, const gchar *context) {
  if (!ctx || !ctx->variables || !name) return FALSE;
  if (!g_hash_table_contains(ctx->variables, name)) {
    dsl_type_add_error(ctx, line, "%s references unknown variable '%s'", context, name);
    return FALSE;
  }
  return TRUE;
}

static gboolean dsl_type_require_element(DSLTypeCheckerContext *ctx, const gchar *name, int line, const gchar *context) {
  if (!ctx || !ctx->elements || !name) return FALSE;
  if (!g_hash_table_contains(ctx->elements, name)) {
    dsl_type_add_error(ctx, line, "%s references unknown element '%s'", context, name);
    return FALSE;
  }
  return TRUE;
}

static void dsl_type_collect_identifiers(const gchar *expr, GPtrArray *out) {
  if (!expr || !out) return;
  const gchar *p = expr;
  while (*p) {
    if (g_ascii_isalpha(*p) || *p == '_') {
      const gchar *start = p;
      p++;
      while (*p && (g_ascii_isalnum(*p) || *p == '_')) {
        p++;
      }
      g_ptr_array_add(out, g_strndup(start, p - start));

      // Skip array access: var[...]
      if (*p == '[') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
          if (*p == '[') depth++;
          else if (*p == ']') depth--;
          p++;
        }
      }
    } else {
      p++;
    }
  }
}

static gboolean dsl_type_check_expression(DSLTypeCheckerContext *ctx, const gchar *expr, int line, const gchar *context) {
  if (!expr || *expr == '\0') return TRUE;
  GPtrArray *idents = g_ptr_array_new_with_free_func(g_free);
  dsl_type_collect_identifiers(expr, idents);
  GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  gboolean ok = TRUE;
  for (guint i = 0; i < idents->len; i++) {
    const gchar *identifier = g_ptr_array_index(idents, i);
    if (!g_hash_table_contains(seen, identifier)) {
      g_hash_table_insert(seen, g_strdup(identifier), GINT_TO_POINTER(1));
      if (!g_hash_table_contains(ctx->variables, identifier)) {
        dsl_type_add_error(ctx, line, "%s uses unknown variable '%s'", context, identifier);
        ok = FALSE;
      } else {
        DSLVarType ref_type = dsl_type_lookup_variable_type(ctx, identifier);
        if (ref_type == DSL_VAR_STRING) {
          dsl_type_add_error(ctx, line, "%s cannot use string variable '%s' in numeric expression", context, identifier);
          ok = FALSE;
        }
      }
    }
  }

  g_hash_table_destroy(seen);
  g_ptr_array_free(idents, TRUE);
  return ok;
}

static void dsl_type_check_token_for_braces(DSLTypeCheckerContext *ctx, const gchar *token, int line, const gchar *context) {
  if (!token) return;
  const gchar *p = token;
  while (*p) {
    if (*p == '{') {
      const gchar *start = ++p;
      int depth = 1;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
      }
      if (depth != 0) {
        dsl_type_add_error(ctx, line, "%s has unmatched '{'", context);
        return;
      }
      size_t len = (size_t)((p - 1) - start);
      gchar *expr = g_strndup(start, len);
      dsl_type_check_expression(ctx, expr, line, context);
      g_free(expr);
    } else if (*p == '}') {
      dsl_type_add_error(ctx, line, "%s has unmatched '}'", context);
      return;
    } else {
      p++;
    }
  }
}

static void dsl_type_check_string_interpolations(DSLTypeCheckerContext *ctx, const gchar *text, int line, const gchar *context) {
  if (!text) return;
  const gchar *p = text;
  while ((p = strstr(p, "${")) != NULL) {
    p += 2; // Skip '${'
    const gchar *start = p;
    int depth = 1;
    while (*p && depth > 0) {
      if (*p == '{') depth++;
      else if (*p == '}') depth--;
      p++;
    }
    if (depth != 0) {
      dsl_type_add_error(ctx, line, "%s has unmatched '${'", context);
      return;
    }
    size_t len = (size_t)((p - 1) - start);
    gchar *expr = g_strndup(start, len);

    // For string interpolation, we need to check if it's a pure variable reference (string allowed)
    // or an expression (only numeric allowed)
    gchar *trimmed = g_strstrip(g_strdup(expr));
    gboolean is_pure_identifier = TRUE;
    if (trimmed && *trimmed) {
      const gchar *c = trimmed;
      if (g_ascii_isalpha(*c) || *c == '_') {
        c++;
        while (*c && (g_ascii_isalnum(*c) || *c == '_')) {
          c++;
        }
        if (*c != '\0') {
          is_pure_identifier = FALSE;
        }
      } else {
        is_pure_identifier = FALSE;
      }
    }

    if (is_pure_identifier) {
      // Pure identifier: can be string or numeric variable, just check it exists
      if (!g_hash_table_contains(ctx->variables, trimmed)) {
        dsl_type_add_error(ctx, line, "%s uses unknown variable '%s'", context, trimmed);
      }
    } else {
      // Expression: must be numeric only
      dsl_type_check_expression(ctx, expr, line, context);
    }

    g_free(trimmed);
    g_free(expr);
  }
}

static void dsl_type_check_set_expression(DSLTypeCheckerContext *ctx, gchar **tokens, int start_index, int token_count, int line) {
  if (start_index >= token_count) return;
  GString *expr = g_string_new(NULL);
  for (int t = start_index; t < token_count; t++) {
    if (expr->len > 0) g_string_append_c(expr, ' ');
    g_string_append(expr, tokens[t]);
  }
  dsl_type_check_expression(ctx, expr->str, line, "set expression");
  g_string_free(expr, TRUE);
}

gboolean dsl_type_is_number_literal(const gchar *token) {
  if (!token || *token == '\0') return FALSE;
  char *end_ptr = NULL;
  g_ascii_strtod(token, &end_ptr);
  return end_ptr != token && *end_ptr == '\0';
}

static gboolean dsl_type_check_numeric_component(DSLTypeCheckerContext *ctx, const gchar *component, int line, const gchar *context) {
  if (!component) return FALSE;
  gchar *trimmed = g_strstrip(g_strdup(component));
  gboolean ok = TRUE;

  size_t len = strlen(trimmed);
  if (len == 0) {
    dsl_type_add_error(ctx, line, "%s is missing numeric value", context);
    ok = FALSE;
  } else if (trimmed[0] == '{' && trimmed[len - 1] == '}') {
    gchar *expr = g_strndup(trimmed + 1, len - 2);
    dsl_type_check_expression(ctx, expr, line, context);
    g_free(expr);
  } else if (!dsl_type_is_number_literal(trimmed)) {
    dsl_type_add_error(ctx, line, "%s expects numeric value, got '%s'", context, trimmed);
    ok = FALSE;
  }

  g_free(trimmed);
  return ok;
}

static gboolean dsl_type_check_point_token(DSLTypeCheckerContext *ctx, const gchar *token, int line, const gchar *context) {
  if (!token || token[0] != '(') {
    dsl_type_add_error(ctx, line, "%s expects point literal like (x,y)", context);
    return FALSE;
  }

  size_t len = strlen(token);
  if (len < 5 || token[len - 1] != ')') {
    dsl_type_add_error(ctx, line, "%s expects point literal like (x,y)", context);
    return FALSE;
  }

  gchar *copy = g_strndup(token + 1, len - 2);
  int brace_depth = 0;
  int paren_depth = 0;
  gchar *comma = NULL;
  for (char *c = copy; *c; c++) {
    if (*c == '{') brace_depth++;
    else if (*c == '}') brace_depth--;
    else if (*c == '(') paren_depth++;
    else if (*c == ')') paren_depth--;
    else if (*c == ',' && brace_depth == 0 && paren_depth == 0) {
      comma = c;
      break;
    }
  }

  if (!comma) {
    dsl_type_add_error(ctx, line, "%s requires two comma-separated numeric values", context);
    g_free(copy);
    return FALSE;
  }

  *comma = '\0';
  gchar *first = copy;
  gchar *second = comma + 1;
  dsl_type_check_numeric_component(ctx, first, line, context);
  dsl_type_check_numeric_component(ctx, second, line, context);
  g_free(copy);
  return TRUE;
}

static gboolean dsl_type_check_color_token(DSLTypeCheckerContext *ctx, const gchar *token, int line, const gchar *context) {
  double r, g, b, a;
  if (!token || !parse_color_token(token, &r, &g, &b, &a)) {
    dsl_type_add_error(ctx, line, "%s expects color literal, got '%s'", context, token ? token : "(null)");
    return FALSE;
  }
  return TRUE;
}

static gboolean dsl_type_check_boolean_token(DSLTypeCheckerContext *ctx, const gchar *token, int line, const gchar *context) {
  if (!token) {
    dsl_type_add_error(ctx, line, "%s expects boolean value", context);
    return FALSE;
  }
  if (g_ascii_strcasecmp(token, "true") == 0 ||
      g_ascii_strcasecmp(token, "false") == 0 ||
      strcmp(token, "1") == 0 ||
      strcmp(token, "0") == 0 ||
      g_ascii_strcasecmp(token, "yes") == 0 ||
      g_ascii_strcasecmp(token, "no") == 0) {
    return TRUE;
  }
  dsl_type_add_error(ctx, line, "%s expects boolean literal, got '%s'", context, token);
  return FALSE;
}

static gboolean dsl_type_check_stroke_style(const gchar *token) {
  return token && (g_ascii_strcasecmp(token, "solid") == 0 ||
                   g_ascii_strcasecmp(token, "dashed") == 0 ||
                   g_ascii_strcasecmp(token, "dotted") == 0);
}

static gboolean dsl_type_check_fill_style(const gchar *token) {
  return token && (g_ascii_strcasecmp(token, "solid") == 0 ||
                   g_ascii_strcasecmp(token, "hachure") == 0 ||
                   g_ascii_strcasecmp(token, "hatch") == 0 ||
                   g_ascii_strcasecmp(token, "cross-hatch") == 0 ||
                   g_ascii_strcasecmp(token, "cross_hatch") == 0 ||
                   g_ascii_strcasecmp(token, "crosshatch") == 0 ||
                   g_ascii_strcasecmp(token, "cross") == 0);
}

static const gchar* dsl_type_extract_inline_value(const gchar *token) {
  const gchar *value = strchr(token, '=');
  if (!value) value = strchr(token, ':');
  if (!value || *(value + 1) == '\0') return NULL;
  return value + 1;
}

static void dsl_type_check_options(DSLTypeCheckerContext *ctx, gchar **tokens, int start_index, int token_count, int line, gboolean allow_shape_options) {
  gboolean expect_bg = FALSE;
  gboolean expect_text_color = FALSE;
  gboolean expect_font = FALSE;
  gboolean expect_filled = FALSE;
  gboolean expect_stroke = FALSE;
  gboolean expect_rotation = FALSE;
  gboolean expect_line_start = FALSE;
  gboolean expect_line_end = FALSE;
  gboolean expect_stroke_color = FALSE;
  gboolean expect_stroke_style = FALSE;
  gboolean expect_fill_style = FALSE;

  for (int t = start_index; t < token_count; t++) {
    const gchar *token = tokens[t];
    if (!token) continue;

    if (expect_bg) {
      dsl_type_check_color_token(ctx, token, line, "background color");
      expect_bg = FALSE;
      continue;
    }
    if (expect_text_color) {
      dsl_type_check_color_token(ctx, token, line, "text color");
      expect_text_color = FALSE;
      continue;
    }
    if (expect_font) {
      expect_font = FALSE;
      continue;
    }
    if (expect_filled) {
      dsl_type_check_boolean_token(ctx, token, line, "filled option");
      expect_filled = FALSE;
      continue;
    }
    if (expect_stroke) {
      if (!dsl_type_is_number_literal(token)) {
        dsl_type_add_error(ctx, line, "stroke width must be numeric");
      }
      expect_stroke = FALSE;
      continue;
    }
    if (expect_rotation) {
      dsl_type_check_numeric_component(ctx, token, line, "rotation option");
      expect_rotation = FALSE;
      continue;
    }
    if (expect_line_start) {
      dsl_type_check_point_token(ctx, token, line, "line_start option");
      expect_line_start = FALSE;
      continue;
    }
    if (expect_line_end) {
      dsl_type_check_point_token(ctx, token, line, "line_end option");
      expect_line_end = FALSE;
      continue;
    }
    if (expect_stroke_color) {
      dsl_type_check_color_token(ctx, token, line, "stroke_color option");
      expect_stroke_color = FALSE;
      continue;
    }
    if (expect_stroke_style) {
      if (!dsl_type_check_stroke_style(token)) {
        dsl_type_add_error(ctx, line, "stroke_style must be solid, dashed, or dotted");
      }
      expect_stroke_style = FALSE;
      continue;
    }
    if (expect_fill_style) {
      if (!dsl_type_check_fill_style(token)) {
        dsl_type_add_error(ctx, line, "fill_style must be solid, hachure, or crosshatch");
      }
      expect_fill_style = FALSE;
      continue;
    }

    if (g_strcmp0(token, "bg") == 0 || g_strcmp0(token, "background") == 0) {
      expect_bg = TRUE;
      continue;
    }
    if (g_strcmp0(token, "text_color") == 0 || g_strcmp0(token, "text") == 0 ||
        g_strcmp0(token, "font_color") == 0) {
      expect_text_color = TRUE;
      continue;
    }
    if (g_strcmp0(token, "font") == 0) {
      expect_font = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "filled") == 0) {
      expect_filled = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "stroke") == 0) {
      expect_stroke = TRUE;
      continue;
    }
    if (g_strcmp0(token, "rotation") == 0) {
      expect_rotation = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "line_start") == 0) {
      expect_line_start = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "line_end") == 0) {
      expect_line_end = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "stroke_color") == 0) {
      expect_stroke_color = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "stroke_style") == 0) {
      expect_stroke_style = TRUE;
      continue;
    }
    if (allow_shape_options && g_strcmp0(token, "fill_style") == 0) {
      expect_fill_style = TRUE;
      continue;
    }

    if (g_str_has_prefix(token, "bg=") || g_str_has_prefix(token, "bg:")) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (!value) dsl_type_add_error(ctx, line, "background option missing value");
      else dsl_type_check_color_token(ctx, value, line, "background option");
      continue;
    }
    if (g_str_has_prefix(token, "text_color=") || g_str_has_prefix(token, "text_color:")) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (!value) dsl_type_add_error(ctx, line, "text_color option missing value");
      else dsl_type_check_color_token(ctx, value, line, "text_color option");
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "filled=") || g_str_has_prefix(token, "filled:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (!value) dsl_type_add_error(ctx, line, "filled option missing value");
      else dsl_type_check_boolean_token(ctx, value, line, "filled option");
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "stroke=") || g_str_has_prefix(token, "stroke:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (!value || !dsl_type_is_number_literal(value)) {
        dsl_type_add_error(ctx, line, "stroke option expects numeric value");
      }
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "line_start=") || g_str_has_prefix(token, "line_start:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (value) dsl_type_check_point_token(ctx, value, line, "line_start option");
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "line_end=") || g_str_has_prefix(token, "line_end:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (value) dsl_type_check_point_token(ctx, value, line, "line_end option");
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "stroke_color=") || g_str_has_prefix(token, "stroke_color:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (value) dsl_type_check_color_token(ctx, value, line, "stroke_color option");
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "stroke_style=") || g_str_has_prefix(token, "stroke_style:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (value && !dsl_type_check_stroke_style(value)) {
        dsl_type_add_error(ctx, line, "stroke_style must be solid, dashed, or dotted");
      }
      continue;
    }
    if (allow_shape_options && (g_str_has_prefix(token, "fill_style=") || g_str_has_prefix(token, "fill_style:"))) {
      const gchar *value = dsl_type_extract_inline_value(token);
      if (value && !dsl_type_check_fill_style(value)) {
        dsl_type_add_error(ctx, line, "fill_style must be solid, hachure, or crosshatch");
      }
      continue;
    }

    dsl_type_check_token_for_braces(ctx, token, line, "option");
  }
}

static void dsl_type_check_event_command(DSLTypeCheckerContext *ctx, gchar **tokens, int token_count, int line);

static void dsl_type_check_loop_body_command(DSLTypeCheckerContext *ctx, gchar **tokens, int token_count, int line) {
  if (token_count < 1) return;
  const gchar *command = tokens[0];

  // Allow variable declarations inside for loops
  gboolean is_global_decl = g_strcmp0(command, "global") == 0;
  int type_token_index = is_global_decl ? 1 : 0;
  const gchar *type_token = tokens[type_token_index];

  if ((g_strcmp0(type_token, "int") == 0 ||
       g_strcmp0(type_token, "real") == 0 ||
       g_strcmp0(type_token, "bool") == 0 ||
       g_strcmp0(type_token, "string") == 0) && token_count >= (type_token_index + 2)) {
    DSLVarType var_type = DSL_VAR_REAL;
    if (g_strcmp0(type_token, "int") == 0) var_type = DSL_VAR_INT;
    else if (g_strcmp0(type_token, "real") == 0) var_type = DSL_VAR_REAL;
    else if (g_strcmp0(type_token, "bool") == 0) var_type = DSL_VAR_BOOL;
    else if (g_strcmp0(type_token, "string") == 0) var_type = DSL_VAR_STRING;

    const gchar *var_name_token = tokens[type_token_index + 1];
    gchar *var_name = NULL;

    // Check for array declaration: name[size]
    const gchar *bracket = strchr(var_name_token, '[');
    if (bracket) {
      var_name = g_strndup(var_name_token, bracket - var_name_token);
      var_type = DSL_VAR_ARRAY;
    } else {
      var_name = g_strdup(var_name_token);
    }

    gboolean already_defined = g_hash_table_contains(ctx->variables, var_name);
    if (!already_defined) {
      dsl_type_register_variable(ctx, var_name, line, var_type);
    }
    g_free(var_name);

    int expr_start = type_token_index + 2;
    if (var_type != DSL_VAR_STRING && token_count > expr_start) {
      GString *expr = g_string_new(NULL);
      for (int t = expr_start; t < token_count; t++) {
        if (expr->len > 0) g_string_append_c(expr, ' ');
        g_string_append(expr, tokens[t]);
      }
      if (var_type == DSL_VAR_BOOL) {
        const gchar *literal = tokens[expr_start];
        if (!(g_ascii_strcasecmp(literal, "true") == 0 ||
              g_ascii_strcasecmp(literal, "false") == 0 ||
              g_ascii_strcasecmp(literal, "yes") == 0 ||
              g_ascii_strcasecmp(literal, "no") == 0 ||
              strcmp(literal, "1") == 0 ||
              strcmp(literal, "0") == 0)) {
          dsl_type_check_expression(ctx, expr->str, line, "bool assignment");
        }
      } else {
        dsl_type_check_expression(ctx, expr->str, line, "variable assignment");
      }
      g_string_free(expr, TRUE);
    }
    return;
  }

  // Fall back to regular event command checking
  dsl_type_check_event_command(ctx, tokens, token_count, line);
}

static void dsl_type_check_event_command(DSLTypeCheckerContext *ctx, gchar **tokens, int token_count, int line) {
  if (token_count < 1) return;
  const gchar *command = tokens[0];

  // Check for variable declarations (event handlers now support full DSL)
  gboolean is_global_decl = g_strcmp0(command, "global") == 0;
  int type_token_index = is_global_decl ? 1 : 0;
  const gchar *type_token = tokens[type_token_index];

  if ((g_strcmp0(type_token, "int") == 0 ||
       g_strcmp0(type_token, "real") == 0 ||
       g_strcmp0(type_token, "bool") == 0 ||
       g_strcmp0(type_token, "string") == 0) && token_count >= (type_token_index + 2)) {
    // Variable declaration in event handler - allowed since handlers use full script processor
    DSLVarType var_type = DSL_VAR_REAL;
    if (g_strcmp0(type_token, "int") == 0) var_type = DSL_VAR_INT;
    else if (g_strcmp0(type_token, "real") == 0) var_type = DSL_VAR_REAL;
    else if (g_strcmp0(type_token, "bool") == 0) var_type = DSL_VAR_BOOL;
    else if (g_strcmp0(type_token, "string") == 0) var_type = DSL_VAR_STRING;

    const gchar *var_name_token = tokens[type_token_index + 1];
    gchar *var_name = NULL;
    const gchar *bracket = strchr(var_name_token, '[');
    if (bracket) {
      var_name = g_strndup(var_name_token, bracket - var_name_token);
      var_type = DSL_VAR_ARRAY;
    } else {
      var_name = g_strdup(var_name_token);
    }

    gboolean already_defined = g_hash_table_contains(ctx->variables, var_name);
    if (!already_defined) {
      dsl_type_register_variable(ctx, var_name, line, var_type);
    }
    g_free(var_name);
    return;
  }

  if (g_strcmp0(command, "set") == 0) {
    if (token_count < 3) {
      dsl_type_add_error(ctx, line, "set requires a variable and a value");
      return;
    }
    // Extract variable name (handle array access)
    const gchar *var_token = tokens[1];
    gchar *var_name = NULL;
    const gchar *bracket = strchr(var_token, '[');
    if (bracket) {
      var_name = g_strndup(var_token, bracket - var_token);
    } else {
      var_name = g_strdup(var_token);
    }

    if (dsl_type_require_variable(ctx, var_name, line, "set")) {
      DSLVarType var_type = dsl_type_lookup_variable_type(ctx, var_name);
      if (var_type != DSL_VAR_INT && var_type != DSL_VAR_REAL && var_type != DSL_VAR_ARRAY) {
        dsl_type_add_error(ctx, line, "set only supports numeric variables (found '%s')", var_name);
      }
    }
    g_free(var_name);
    dsl_type_check_set_expression(ctx, tokens, 2, token_count, line);
  } else if (g_strcmp0(command, "animate_move") == 0 ||
             g_strcmp0(command, "animate_resize") == 0 ||
             g_strcmp0(command, "animate_color") == 0 ||
             g_strcmp0(command, "animate_rotate") == 0 ||
             g_strcmp0(command, "animate_appear") == 0 ||
             g_strcmp0(command, "animate_disappear") == 0 ||
             g_strcmp0(command, "animate_create") == 0 ||
             g_strcmp0(command, "animate_delete") == 0) {
    if (token_count < 2) {
      dsl_type_add_error(ctx, line, "%s requires an element id", command);
      return;
    }
    dsl_type_require_element(ctx, tokens[1], line, command);
    int idx = 2;
    int point_count = 0;
    while (idx < token_count && tokens[idx] && tokens[idx][0] == '(' && point_count < 2) {
      dsl_type_check_point_token(ctx, tokens[idx], line, command);
      idx++;
      point_count++;
    }
    if (g_strcmp0(command, "animate_color") == 0) {
      if (idx < token_count) dsl_type_check_token_for_braces(ctx, tokens[idx++], line, command);
      if (idx < token_count) dsl_type_check_token_for_braces(ctx, tokens[idx++], line, command);
    }
    if (g_strcmp0(command, "animate_move") == 0 || g_strcmp0(command, "animate_resize") == 0) {
      if (idx >= token_count) {
        dsl_type_add_error(ctx, line, "%s missing start time", command);
        return;
      }
      dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      if (idx >= token_count) {
        dsl_type_add_error(ctx, line, "%s missing duration", command);
        return;
      }
      dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      if (idx < token_count) {
        dsl_type_validate_interpolation(ctx, tokens[idx++], line, command);
      }
    } else if (g_strcmp0(command, "animate_rotate") == 0) {
      // animate_rotate ELEMENT TO_DEGREES START DURATION [TYPE]   (4-5 args after element)
      // animate_rotate ELEMENT FROM TO START DURATION [TYPE]      (5-6 args after element)
      // We need to count numeric args to determine which syntax

      int numeric_count = 0;
      int temp_idx = idx;
      while (temp_idx < token_count) {
        const gchar *token = tokens[temp_idx];
        if (token[0] == '(' || g_ascii_isalpha(token[0])) break;
        if (dsl_type_is_number_literal(token)) {
          numeric_count++;
          temp_idx++;
        } else {
          break;
        }
      }

      // Determine how many rotation params based on total numeric count
      // If 4+ numeric values, first two are from/to rotation, next two are start/duration
      // If 3+ numeric values, first is to rotation, next two are start/duration
      int rotation_params = (numeric_count >= 4) ? 2 : 1;

      // Validate rotation parameters
      for (int i = 0; i < rotation_params && idx < token_count; i++) {
        dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      }

      // Validate timing parameters
      if (idx < token_count) dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      if (idx < token_count) dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);

      // Validate interpolation
      if (idx < token_count) {
        dsl_type_validate_interpolation(ctx, tokens[idx++], line, command);
      }
    } else if (g_strcmp0(command, "animate_color") == 0 ||
               g_strcmp0(command, "animate_appear") == 0 ||
               g_strcmp0(command, "animate_disappear") == 0 ||
               g_strcmp0(command, "animate_create") == 0 ||
               g_strcmp0(command, "animate_delete") == 0) {
      if (idx < token_count) dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      if (idx < token_count) dsl_type_check_numeric_component(ctx, tokens[idx++], line, command);
      if (idx < token_count) {
        dsl_type_validate_interpolation(ctx, tokens[idx++], line, command);
      }
    }
    for (; idx < token_count; idx++) {
      dsl_type_check_token_for_braces(ctx, tokens[idx], line, command);
    }
  } else if (g_strcmp0(command, "text_update") == 0) {
    if (token_count < 3) {
      dsl_type_add_error(ctx, line, "text_update requires an element id and text");
      return;
    }
    dsl_type_require_element(ctx, tokens[1], line, "text_update");
    dsl_type_check_string_interpolations(ctx, tokens[2], line, "text_update");
  } else if (g_strcmp0(command, "presentation_auto_next_if") == 0) {
    if (token_count < 3) {
      dsl_type_add_error(ctx, line, "presentation_auto_next_if requires a variable and value");
      return;
    }
    dsl_type_require_variable(ctx, tokens[1], line, "presentation_auto_next_if");
  } else if (g_strcmp0(command, "canvas_background") == 0 ||
             g_strcmp0(command, "animation_mode") == 0) {
    // No additional checks needed inside event
  } else if (g_strcmp0(command, "shape_create") == 0 ||
             g_strcmp0(command, "note_create") == 0 ||
             g_strcmp0(command, "text_create") == 0 ||
             g_strcmp0(command, "paper_note_create") == 0) {
    // Allow shape/note/text creation in event handlers
    // Basic validation: needs ID, text/type, position, size
    if (token_count < 5) {
      dsl_type_add_error(ctx, line, "%s requires at least id, type/text, position, and size", command);
      return;
    }
    // Register the element ID being created
    const gchar *element_id = tokens[1];
    if (element_id && element_id[0] != '\0') {
      dsl_type_register_element(ctx, element_id, line);
    }
  } else if (g_strcmp0(command, "element_delete") == 0) {
    if (token_count < 2) {
      dsl_type_add_error(ctx, line, "element_delete requires an element id");
      return;
    }
    dsl_type_require_element(ctx, tokens[1], line, "element_delete");
  } else if (g_strcmp0(command, "for") == 0) {
    // For loops are allowed in event blocks
    if (token_count < 4) {
      dsl_type_add_error(ctx, line, "for loop requires variable, start, and end values");
      return;
    }
    const gchar *loop_var = tokens[1];
    // Register loop variable as int type if not exists
    if (!g_hash_table_contains(ctx->variables, loop_var)) {
      dsl_type_register_variable(ctx, loop_var, line, DSL_VAR_INT);
    }
    // Check loop bounds are valid expressions
    dsl_type_check_expression(ctx, tokens[2], line, "for loop start");
    dsl_type_check_expression(ctx, tokens[3], line, "for loop end");
    // Note: nested for loop body validation would require recursion here
    // For now we just validate the loop header
  } else {
    // Unrecognized command inside block - flag but continue
    dsl_type_add_error(ctx, line, "Unknown command '%s' inside event block", command);
  }
}

static void dsl_type_check_event_block(DSLTypeCheckerContext *ctx, gchar **lines, int *index_ptr, const gchar *event_type, const gchar *target, int start_line) {
  int i = *index_ptr;
  gboolean found_end = FALSE;

  for (int j = i + 1; lines[j] != NULL; j++) {
    gchar *raw = trim_whitespace(lines[j]);
    if (raw[0] == '\0' || raw[0] == '#') {
      continue;
    }
    if (g_strcmp0(raw, "end") == 0) {
      found_end = TRUE;
      *index_ptr = j;
      break;
    }

    int token_count = 0;
    gchar **tokens = tokenize_line(raw, &token_count);
    if (token_count < 0) {
      dsl_type_add_error(ctx, j + 1, "Syntax error in event block");
      g_strfreev(tokens);
      break;
    }
    if (token_count > 0) {
      dsl_type_check_event_command(ctx, tokens, token_count, j + 1);
    }
    g_strfreev(tokens);
  }

  if (!found_end) {
    dsl_type_add_error(ctx, start_line, "Event '%s %s' missing matching 'end'", event_type, target);
  }
}

gboolean dsl_type_check_script(CanvasData *data,
                               const gchar *script,
                               const gchar *filename,
                               GPtrArray **out_errors) {
  (void)data;
  DSLTypeCheckerContext ctx = {
    .variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
    .elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
    .errors = g_ptr_array_new_with_free_func(g_free),
    .filename = filename,
  };

  if (out_errors) {
    *out_errors = NULL;
  }

  if (data && data->model && data->model->elements) {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, data->model->elements);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      if (key) {
        g_hash_table_insert(ctx.elements, g_strdup((const gchar *)key), GINT_TO_POINTER(1));
      }
    }
  }

  // Also seed elements from DSL runtime (for AI-generated aliases like "circle_5")
  dsl_runtime_seed_element_ids(data, ctx.elements);

  if (data && data->presentation_mode_active) {
    dsl_runtime_seed_global_types(data, ctx.variables);
  }

  gchar **lines = g_strsplit(script, "\n", 0);

  for (int i = 0; lines[i] != NULL; i++) {
    gchar *raw_line = lines[i];
    gchar *line = trim_whitespace(raw_line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }

    int token_count = 0;
    int line_no = i + 1;
    gchar **tokens = tokenize_line(line, &token_count);
    if (token_count < 0) {
      dsl_type_add_error(&ctx, line_no, "Syntax error");
      g_strfreev(tokens);
      continue;
    }
    if (token_count < 1) {
      g_strfreev(tokens);
      continue;
    }

    const gchar *cmd = tokens[0];

    gboolean is_global_decl = g_strcmp0(cmd, "global") == 0;
    int type_token_index = is_global_decl ? 1 : 0;

    if (is_global_decl && token_count < 3) {
      dsl_type_add_error(&ctx, line_no, "global declaration requires a type and variable name");
      g_strfreev(tokens);
      continue;
    }

    const gchar *type_token = tokens[type_token_index];

    if ((g_strcmp0(type_token, "int") == 0 ||
         g_strcmp0(type_token, "real") == 0 ||
         g_strcmp0(type_token, "bool") == 0 ||
         g_strcmp0(type_token, "string") == 0) && token_count >= (type_token_index + 2)) {
      DSLVarType var_type = DSL_VAR_REAL;
      if (g_strcmp0(type_token, "int") == 0) var_type = DSL_VAR_INT;
      else if (g_strcmp0(type_token, "real") == 0) var_type = DSL_VAR_REAL;
      else if (g_strcmp0(type_token, "bool") == 0) var_type = DSL_VAR_BOOL;
      else if (g_strcmp0(type_token, "string") == 0) var_type = DSL_VAR_STRING;

      const gchar *var_name_token = tokens[type_token_index + 1];
      gchar *var_name = NULL;

      // Check for array declaration: name[size]
      const gchar *bracket = strchr(var_name_token, '[');
      if (bracket) {
        var_name = g_strndup(var_name_token, bracket - var_name_token);
        var_type = DSL_VAR_ARRAY;  // Override to array type
      } else {
        var_name = g_strdup(var_name_token);
      }

      gboolean already_defined = g_hash_table_contains(ctx.variables, var_name);
      if (!already_defined) {
        dsl_type_register_variable(&ctx, var_name, line_no, var_type);
      } else if (!is_global_decl) {
        dsl_type_add_error(&ctx, line_no, "Variable '%s' already defined", var_name);
      }
      g_free(var_name);

      int expr_start = type_token_index + 2;

      if (var_type == DSL_VAR_STRING) {
        // Strings accept literal tokens without additional checks
      } else if (token_count > expr_start) {
        GString *expr = g_string_new(NULL);
        for (int t = expr_start; t < token_count; t++) {
          if (expr->len > 0) g_string_append_c(expr, ' ');
          g_string_append(expr, tokens[t]);
        }
        if (var_type == DSL_VAR_BOOL) {
          const gchar *literal = tokens[expr_start];
          if (!(g_ascii_strcasecmp(literal, "true") == 0 ||
                g_ascii_strcasecmp(literal, "false") == 0 ||
                g_ascii_strcasecmp(literal, "yes") == 0 ||
                g_ascii_strcasecmp(literal, "no") == 0 ||
                strcmp(literal, "1") == 0 ||
                strcmp(literal, "0") == 0)) {
            dsl_type_check_expression(&ctx, expr->str, line_no, "bool assignment");
          }
        } else {
          dsl_type_check_expression(&ctx, expr->str, line_no, "variable assignment");
        }
        g_string_free(expr, TRUE);
      }
    } else if ((g_strcmp0(cmd, "note_create") == 0 ||
                g_strcmp0(cmd, "paper_note_create") == 0 ||
                g_strcmp0(cmd, "text_create") == 0) && token_count >= 5) {
      const gchar *elem_id = tokens[1];
      dsl_type_register_element(&ctx, elem_id, line_no);
      const gchar *text = tokens[2];
      dsl_type_check_string_interpolations(&ctx, text, line_no, cmd);
      dsl_type_check_point_token(&ctx, tokens[3], line_no, cmd);
      dsl_type_check_point_token(&ctx, tokens[4], line_no, cmd);
      dsl_type_check_options(&ctx, tokens, 5, token_count, line_no, FALSE);
    } else if (g_strcmp0(cmd, "shape_create") == 0 && token_count >= 6) {
      const gchar *elem_id = tokens[1];
      dsl_type_register_element(&ctx, elem_id, line_no);
      const gchar *shape_text = tokens[3];
      dsl_type_check_string_interpolations(&ctx, shape_text, line_no, cmd);
      dsl_type_check_point_token(&ctx, tokens[4], line_no, cmd);
      dsl_type_check_point_token(&ctx, tokens[5], line_no, cmd);
      dsl_type_check_options(&ctx, tokens, 6, token_count, line_no, TRUE);
    } else if ((g_strcmp0(cmd, "image_create") == 0 ||
                g_strcmp0(cmd, "video_create") == 0 ||
                g_strcmp0(cmd, "audio_create") == 0 ||
                g_strcmp0(cmd, "space_create") == 0) && token_count >= 5) {
      const gchar *elem_id = tokens[1];
      dsl_type_register_element(&ctx, elem_id, line_no);
      dsl_type_check_point_token(&ctx, tokens[3], line_no, cmd);
      dsl_type_check_point_token(&ctx, tokens[4], line_no, cmd);
      dsl_type_check_options(&ctx, tokens, 5, token_count, line_no, FALSE);
    } else if (g_strcmp0(cmd, "canvas_background") == 0) {
      if (token_count >= 2) {
        dsl_type_check_color_token(&ctx, tokens[1], line_no, "canvas_background color");
      }
      if (token_count >= 4) {
        dsl_type_check_color_token(&ctx, tokens[3], line_no, "canvas_background grid color");
      }
    } else if (g_strcmp0(cmd, "connect") == 0 && token_count >= 3) {
      dsl_type_require_element(&ctx, tokens[1], line_no, "connect");
      dsl_type_require_element(&ctx, tokens[2], line_no, "connect");
    } else if (g_strcmp0(cmd, "on") == 0 && token_count >= 3) {
      const gchar *event_type = tokens[1];
      const gchar *target = tokens[2];
      if (g_ascii_strcasecmp(event_type, "click") == 0) {
        dsl_type_require_element(&ctx, target, line_no, "on click");
      } else if (g_ascii_strcasecmp(event_type, "variable") == 0) {
        dsl_type_require_variable(&ctx, target, line_no, "on variable");
      }
      dsl_type_check_event_block(&ctx, lines, &i, event_type, target, line_no);
    } else if (g_strcmp0(cmd, "for") == 0 && token_count >= 4) {
      const gchar *loop_var = tokens[1];
      // Register loop variable as int type if not exists
      if (!g_hash_table_contains(ctx.variables, loop_var)) {
        dsl_type_register_variable(&ctx, loop_var, line_no, DSL_VAR_INT);
      }
      // Check loop bounds are valid expressions
      dsl_type_check_expression(&ctx, tokens[2], line_no, "for loop start");
      dsl_type_check_expression(&ctx, tokens[3], line_no, "for loop end");

      // Validate loop body commands (similar to event block validation)
      gboolean found_end = FALSE;
      int nesting_depth = 0;

      for (int j = i + 1; lines[j] != NULL; j++) {
        gchar *check_line = trim_whitespace(lines[j]);
        if (check_line[0] == '\0' || check_line[0] == '#') {
          continue;
        }

        int body_token_count = 0;
        gchar **body_tokens = tokenize_line(check_line, &body_token_count);

        if (body_token_count < 0) {
          dsl_type_add_error(&ctx, j + 1, "Syntax error in for loop");
          g_strfreev(body_tokens);
          break;
        }

        if (body_token_count > 0) {
          if (g_strcmp0(body_tokens[0], "for") == 0) {
            nesting_depth++;
            // Validate nested for loop header
            if (body_token_count >= 4) {
              const gchar *nested_var = body_tokens[1];
              if (!g_hash_table_contains(ctx.variables, nested_var)) {
                dsl_type_register_variable(&ctx, nested_var, j + 1, DSL_VAR_INT);
              }
              dsl_type_check_expression(&ctx, body_tokens[2], j + 1, "for loop start");
              dsl_type_check_expression(&ctx, body_tokens[3], j + 1, "for loop end");
            }
          } else if (g_strcmp0(body_tokens[0], "end") == 0) {
            if (nesting_depth > 0) {
              nesting_depth--;
            } else {
              i = j;
              found_end = TRUE;
              g_strfreev(body_tokens);
              break;
            }
          } else {
            // Validate command in loop body (allows variable declarations)
            dsl_type_check_loop_body_command(&ctx, body_tokens, body_token_count, j + 1);
          }
        }

        g_strfreev(body_tokens);
      }

      if (!found_end) {
        dsl_type_add_error(&ctx, line_no, "for loop missing matching 'end'");
      }
    } else if (g_strcmp0(cmd, "animation_next_slide") == 0 ||
               g_strcmp0(cmd, "animation_mode") == 0 ||
               g_strcmp0(cmd, "animation_mode") == 0) {
      // Nothing to validate
    } else if (g_strcmp0(cmd, "presentation_next") == 0) {
      // Nothing to validate
    } else if (g_strcmp0(cmd, "presentation_auto_next_if") == 0 && token_count >= 3) {
      dsl_type_require_variable(&ctx, tokens[1], line_no, "presentation_auto_next_if");
    } else if (g_strcmp0(cmd, "text_bind") == 0 && token_count >= 3) {
      dsl_type_require_element(&ctx, tokens[1], line_no, "text_bind");
      dsl_type_require_variable(&ctx, tokens[2], line_no, "text_bind");
    } else if (g_strcmp0(cmd, "position_bind") == 0 && token_count >= 3) {
      dsl_type_require_element(&ctx, tokens[1], line_no, "position_bind");
      dsl_type_require_variable(&ctx, tokens[2], line_no, "position_bind");
    } else if (g_strcmp0(cmd, "element_delete") == 0 && token_count >= 2) {
      dsl_type_require_element(&ctx, tokens[1], line_no, "element_delete");
    } else {
      for (int t = 1; t < token_count; t++) {
        dsl_type_check_token_for_braces(&ctx, tokens[t], line_no, cmd);
      }
    }

    g_strfreev(tokens);
  }

  gboolean ok = (ctx.errors->len == 0);
  if (!ok) {
    if (out_errors) {
      GPtrArray *collected = g_ptr_array_new_with_free_func(g_free);
      for (guint i = 0; i < ctx.errors->len; i++) {
        const gchar *msg = g_ptr_array_index(ctx.errors, i);
        g_ptr_array_add(collected, g_strdup(msg));
      }
      *out_errors = collected;
    } else {
      g_print("DSL type check found %u issue(s):\n", ctx.errors->len);
      for (guint i = 0; i < ctx.errors->len; i++) {
        const gchar *msg = g_ptr_array_index(ctx.errors, i);
        g_print("%s\n", msg);
      }
    }
  }

  g_ptr_array_free(ctx.errors, TRUE);
  g_hash_table_destroy(ctx.variables);
  g_hash_table_destroy(ctx.elements);
  g_strfreev(lines);

  return ok;
}
