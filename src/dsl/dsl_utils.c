#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "dsl/dsl_utils.h"

gchar* trim_whitespace(gchar *str) {
  while (isspace(*str)) str++;
  if (*str == 0) return str;

  gchar *end = str + strlen(str) - 1;
  while (end > str && isspace(*end)) end--;
  *(end + 1) = 0;

  return str;
}

gboolean parse_point(const gchar *str, int *x, int *y) {
  if (str[0] != '(') return FALSE;

  gchar *copy = g_strdup(str);
  gchar *comma = strchr(copy, ',');
  if (!comma) {
    g_free(copy);
    return FALSE;
  }

  *comma = '\0';
  gchar *end_ptr;

  // Parse x value (skip the opening parenthesis)
  *x = (int)strtol(copy + 1, &end_ptr, 10);
  if (*end_ptr != '\0') {
    g_free(copy);
    return FALSE;
  }

  gchar *y_str = comma + 1;
  while (isspace(*y_str)) y_str++;

  // Find the closing parenthesis
  gchar *close_paren = strchr(y_str, ')');
  if (!close_paren) {
    g_free(copy);
    return FALSE;
  }
  *close_paren = '\0';

  *y = (int)strtol(y_str, &end_ptr, 10);
  if (*end_ptr != '\0') {
    g_free(copy);
    return FALSE;
  }

  g_free(copy);
  return TRUE;
}

gboolean parse_float_point(const gchar *str, double *x, double *y) {
  if (str[0] != '(') return FALSE;

  gchar *copy = g_strdup(str);
  gchar *comma = strchr(copy, ',');
  if (!comma) {
    g_free(copy);
    return FALSE;
  }

  *comma = '\0';

  gchar *end_ptr;
  double parsed_x = g_ascii_strtod(copy + 1, &end_ptr);
  if (*end_ptr != '\0') {
    g_free(copy);
    return FALSE;
  }

  gchar *y_str = comma + 1;
  while (isspace(*y_str)) y_str++;

  gchar *close_paren = strchr(y_str, ')');
  if (!close_paren) {
    g_free(copy);
    return FALSE;
  }
  *close_paren = '\0';

  double parsed_y = g_ascii_strtod(y_str, &end_ptr);
  if (*end_ptr != '\0') {
    g_free(copy);
    return FALSE;
  }

  g_free(copy);

  *x = CLAMP(parsed_x, 0.0, 1.0);
  *y = CLAMP(parsed_y, 0.0, 1.0);
  return TRUE;
}

gboolean parse_shape_type(const gchar *str, int *shape_type) {
  if (g_strcmp0(str, "circle") == 0) {
    *shape_type = SHAPE_CIRCLE;
    return TRUE;
  } else if (g_strcmp0(str, "rectangle") == 0) {
    *shape_type = SHAPE_RECTANGLE;
    return TRUE;
  } else if (g_strcmp0(str, "triangle") == 0) {
    *shape_type = SHAPE_TRIANGLE;
    return TRUE;
  } else if (g_strcmp0(str, "diamond") == 0) {
    *shape_type = SHAPE_DIAMOND;
    return TRUE;
  } else if (g_strcmp0(str, "cylinder_vertical") == 0 || g_strcmp0(str, "vcylinder") == 0) {
    *shape_type = SHAPE_CYLINDER_VERTICAL;
    return TRUE;
  } else if (g_strcmp0(str, "cylinder_horizontal") == 0 || g_strcmp0(str, "hcylinder") == 0) {
    *shape_type = SHAPE_CYLINDER_HORIZONTAL;
    return TRUE;
  } else if (g_strcmp0(str, "rounded_rectangle") == 0 ||
             g_strcmp0(str, "rounded-rectangle") == 0 ||
             g_strcmp0(str, "roundedrect") == 0 ||
             g_strcmp0(str, "roundrect") == 0) {
    *shape_type = SHAPE_ROUNDED_RECTANGLE;
    return TRUE;
  } else if (g_strcmp0(str, "trapezoid") == 0) {
    *shape_type = SHAPE_TRAPEZOID;
    return TRUE;
  } else if (g_strcmp0(str, "line") == 0) {
    *shape_type = SHAPE_LINE;
    return TRUE;
  } else if (g_strcmp0(str, "arrow") == 0) {
    *shape_type = SHAPE_ARROW;
    return TRUE;
  } else if (g_strcmp0(str, "bezier") == 0 || g_strcmp0(str, "curve") == 0) {
    *shape_type = SHAPE_BEZIER;
    return TRUE;
  } else if (g_strcmp0(str, "cube") == 0) {
    *shape_type = SHAPE_CUBE;
    return TRUE;
  } else if (g_strcmp0(str, "plot") == 0 || g_strcmp0(str, "graph") == 0) {
    *shape_type = SHAPE_PLOT;
    return TRUE;
  }
  return FALSE;
}

gboolean parse_color(const gchar *str, double *r, double *g, double *b, double *a) {
  if (str[0] != '(') return FALSE;

  gchar *copy = g_strdup(str + 1); // Skip '('
  gchar *values[4];
  gchar *token = strtok(copy, ",");
  int i = 0;

  while (token && i < 4) {
    values[i++] = token;
    token = strtok(NULL, ",");
  }

  if (i != 4) {
    g_free(copy);
    return FALSE;
  }

  // Remove trailing ')' from the last value
  gchar *last = values[3];
  gchar *close_paren = strchr(last, ')');
  if (!close_paren) {
    g_free(copy);
    return FALSE;
  }
  *close_paren = '\0';

  *r = g_ascii_strtod(values[0], NULL);
  *g = g_ascii_strtod(values[1], NULL);
  *b = g_ascii_strtod(values[2], NULL);
  *a = g_ascii_strtod(values[3], NULL);

  g_free(copy);
  return TRUE;
}

gboolean parse_color_token(const gchar *token,
                                  double *r, double *g, double *b, double *a) {
  if (!token) return FALSE;

  // Handle hex notation
  if (token[0] == '#') {
    size_t len = strlen(token + 1);
    if (len != 6 && len != 8) {
      return FALSE;
    }

    char component[3] = {0};
    component[0] = token[1];
    component[1] = token[2];
    int r_int = (int)strtol(component, NULL, 16);
    component[0] = token[3];
    component[1] = token[4];
    int g_int = (int)strtol(component, NULL, 16);
    component[0] = token[5];
    component[1] = token[6];
    int b_int = (int)strtol(component, NULL, 16);
    int a_int = 255;
    if (len == 8) {
      component[0] = token[7];
      component[1] = token[8];
      a_int = (int)strtol(component, NULL, 16);
    }

    *r = r_int / 255.0;
    *g = g_int / 255.0;
    *b = b_int / 255.0;
    *a = a_int / 255.0;
    return TRUE;
  }

  // Handle prefixes like color(...), color=(...), rgba(...)
  const gchar *start = NULL;
  if (g_str_has_prefix(token, "color")) {
    start = strchr(token, '(');
  } else if (g_str_has_prefix(token, "rgba")) {
    start = strchr(token, '(');
  } else {
    start = token;
  }

  if (!start) {
    if (g_str_has_prefix(token, "color")) {
      const gchar *equals = strchr(token, '=');
      if (equals && *(equals + 1) != '\0') {
        return parse_color_token(equals + 1, r, g, b, a);
      }
    }
    return FALSE;
  }
  if (*start != '(') {
    // Allow forms like color=(...)
    if (*start == '=') {
      start++;
    }
  }

  if (*start != '(') return FALSE;

  return parse_color(start, r, g, b, a);
}

gboolean parse_font_value(const gchar *value, gchar **out_font) {
  if (!value || !out_font) return FALSE;

  const gchar *start = value;
  size_t len = strlen(value);

  if (len == 0) return FALSE;

  if (start[0] == '"' && len >= 2 && start[len - 1] == '"') {
    start++;
    len -= 2;
  }

  *out_font = g_strndup(start, len);
  return TRUE;
}

gboolean parse_bool_value(const gchar *token, gboolean *out_value) {
  if (!token || !out_value) return FALSE;

  if (g_ascii_strcasecmp(token, "true") == 0 ||
      g_ascii_strcasecmp(token, "yes") == 0 ||
      strcmp(token, "1") == 0) {
    *out_value = TRUE;
    return TRUE;
  }

  if (g_ascii_strcasecmp(token, "false") == 0 ||
      g_ascii_strcasecmp(token, "no") == 0 ||
      strcmp(token, "0") == 0) {
    *out_value = FALSE;
    return TRUE;
  }

  return FALSE;
}

gboolean parse_stroke_style_value(const gchar *token, StrokeStyle *out_style) {
  if (!token || !out_style) return FALSE;

  if (g_ascii_strcasecmp(token, "solid") == 0) {
    *out_style = STROKE_STYLE_SOLID;
    return TRUE;
  }
  if (g_ascii_strcasecmp(token, "dashed") == 0) {
    *out_style = STROKE_STYLE_DASHED;
    return TRUE;
  }
  if (g_ascii_strcasecmp(token, "dotted") == 0) {
    *out_style = STROKE_STYLE_DOTTED;
    return TRUE;
  }
  return FALSE;
}

gboolean parse_fill_style_value(const gchar *token, FillStyle *out_style) {
  if (!token || !out_style) return FALSE;

  if (g_ascii_strcasecmp(token, "solid") == 0) {
    *out_style = FILL_STYLE_SOLID;
    return TRUE;
  }
  if (g_ascii_strcasecmp(token, "hachure") == 0 || g_ascii_strcasecmp(token, "hatch") == 0) {
    *out_style = FILL_STYLE_HACHURE;
    return TRUE;
  }
  if (g_ascii_strcasecmp(token, "cross-hatch") == 0 || g_ascii_strcasecmp(token, "cross_hatch") == 0 ||
      g_ascii_strcasecmp(token, "crosshatch") == 0 || g_ascii_strcasecmp(token, "cross") == 0) {
    *out_style = FILL_STYLE_CROSS_HATCH;
    return TRUE;
  }
  return FALSE;
}

gboolean parse_int_value(const gchar *token, int *out_value) {
  if (!token || !out_value) return FALSE;

  char *end_ptr = NULL;
  long value = strtol(token, &end_ptr, 10);
  if (end_ptr == token || *end_ptr != '\0') {
    return FALSE;
  }

  *out_value = (int)value;
  return TRUE;
}

gboolean parse_double_value(const gchar *token, double *out_value) {
  if (!token || !out_value) return FALSE;

  char *end_ptr = NULL;
  double value = g_ascii_strtod(token, &end_ptr);
  if (end_ptr == token || *end_ptr != '\0') {
    return FALSE;
  }

  *out_value = value;
  return TRUE;
}

gchar** tokenize_line(const gchar *line, int *token_count) {
  GArray *tokens = g_array_new(FALSE, FALSE, sizeof(gchar*));
  const gchar *p = line;

  while (*p) {
    // Skip whitespace
    while (*p && isspace(*p)) p++;
    if (!*p) break;

    if (*p == '"') {
      // Quoted string
      const gchar *start = p + 1;
      p++;
      while (*p && *p != '"') p++;
      if (*p == '"') {
        gchar *token = g_strndup(start, p - start);
        g_array_append_val(tokens, token);
        p++;
      }
    } else if (*p == '(') {
      // Parenthesized expression
      const gchar *start = p;
      int depth = 0;
      do {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
      } while (*p && depth > 0);

      gchar *token = g_strndup(start, p - start);
      g_array_append_val(tokens, token);
    } else if (*p == '{') {
      const gchar *start = p;
      int depth = 0;
      do {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
      } while (*p && depth > 0);

      gchar *token = g_strndup(start, p - start);
      g_array_append_val(tokens, token);
    } else {
      // Regular token
      const gchar *start = p;
      while (*p && !isspace(*p)) p++;
      gchar *token = g_strndup(start, p - start);
      g_array_append_val(tokens, token);
    }
  }

  *token_count = tokens->len;
  gchar *null_token = NULL;
  g_array_append_val(tokens, null_token);
  return (gchar**)g_array_free(tokens, FALSE);
}
