#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "canvas/canvas_core.h"
#include "elements/element.h"
#include "elements/connection.h"
#include "undo_manager.h"
#include "elements/shape.h"
#include "canvas/canvas_drop.h"
#include "elements/paper_note.h"
#include "animation.h"
#include "elements/inline_text.h"

#include "dsl/dsl_executor.h"
#include "dsl/dsl_runtime.h"
#include "dsl/dsl_utils.h"
#include "dsl/dsl_commands.h"

// Forward declaration for recursive script execution
extern void canvas_execute_script_internal(CanvasData *data, const gchar *script, const gchar *filename, gboolean skip_type_check);

static gchar *dsl_copy_paren_token(const gchar *start, const gchar **after_token) {
  if (!start || *start != '(') {
    return NULL;
  }

  int depth = 1;
  const gchar *p = start + 1;
  while (*p && depth > 0) {
    if (*p == '(') depth++;
    else if (*p == ')') depth--;
    p++;
  }

  if (depth != 0) {
    return NULL;
  }

  gchar *token = g_strndup(start, p - start);
  if (after_token) {
    *after_token = p;
  }
  return token;
}

gboolean dsl_execute_command_block(CanvasData *data, const gchar *block_source) {
  if (!data || !block_source) return FALSE;

  gchar **lines = g_strsplit(block_source, "\n", 0);
  gboolean success = TRUE;
  gboolean variables_changed = FALSE;
  gboolean animation_prepared = FALSE;
  gboolean animations_scheduled = FALSE;

  for (int i = 0; lines[i] != NULL; i++) {
    gchar *line = trim_whitespace(lines[i]);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }

    int token_count = 0;
    gchar **tokens = tokenize_line(line, &token_count);
    if (token_count < 0) {
      success = FALSE;
      g_strfreev(tokens);
      break;
    }
    if (token_count < 1) {
      g_strfreev(tokens);
      continue;
    }


    if (g_strcmp0(tokens[0], "set") == 0 && token_count >= 3) {
      const gchar *var_token = tokens[1];
      gchar *var_name = NULL;
      int array_index = -1;

      // Check for array access: var[index]
      const gchar *bracket = strchr(var_token, '[');
      if (bracket) {
        size_t name_len = bracket - var_token;
        var_name = g_strndup(var_token, name_len);
        const gchar *index_start = bracket + 1;
        const gchar *bracket_end = strchr(index_start, ']');
        if (bracket_end) {
          gchar *index_expr = g_strndup(index_start, bracket_end - index_start);
          double index_val = 0.0;
          if (dsl_evaluate_expression(data, index_expr, &index_val)) {
            array_index = (int)index_val;
          }
          g_free(index_expr);
        }
      } else {
        var_name = g_strdup(var_token);
      }

      DSLVariable *var = dsl_runtime_lookup_variable(data, var_name);
      if (!var) {
        g_print("DSL: set references unknown variable '%s'\n", var_name);
        g_free(var_name);
        success = FALSE;
      } else if (array_index >= 0) {
        // Array element assignment
        if (var->type != DSL_VAR_ARRAY) {
          g_print("DSL: Variable '%s' is not an array\n", var_name);
          g_free(var_name);
          success = FALSE;
        } else {
          GString *expr_builder = g_string_new(NULL);
          for (int t = 2; t < token_count; t++) {
            if (expr_builder->len > 0) {
              g_string_append_c(expr_builder, ' ');
            }
            g_string_append(expr_builder, tokens[t]);
          }

          gchar *expr = g_string_free(expr_builder, FALSE);
          gchar *clean_expr = expr;
          gsize expr_len = strlen(expr);
          if (expr_len >= 2 && expr[0] == '{' && expr[expr_len - 1] == '}') {
            clean_expr = g_strndup(expr + 1, expr_len - 2);
            g_free(expr);
            expr = clean_expr;
          }

          double value = 0.0;
          if (!dsl_evaluate_expression(data, expr, &value)) {
            g_print("DSL: Failed to evaluate set expression '%s'\n", expr);
            success = FALSE;
          } else {
            dsl_runtime_set_array_element(data, var_name, array_index, value, TRUE);
            variables_changed = TRUE;
          }
          g_free(expr);
          g_free(var_name);
        }
      } else if (var->type != DSL_VAR_INT && var->type != DSL_VAR_REAL && var->type != DSL_VAR_UNSET) {
        g_print("DSL: set only supports numeric variables (attempted on '%s')\n", var_name);
        g_free(var_name);
        success = FALSE;
      } else {
        GString *expr_builder = g_string_new(NULL);
        for (int t = 2; t < token_count; t++) {
          if (expr_builder->len > 0) {
            g_string_append_c(expr_builder, ' ');
          }
          g_string_append(expr_builder, tokens[t]);
        }

        gchar *expr = g_string_free(expr_builder, FALSE);

        // Strip braces if present
        gchar *clean_expr = expr;
        gsize expr_len = strlen(expr);
        if (expr_len >= 2 && expr[0] == '{' && expr[expr_len - 1] == '}') {
          clean_expr = g_strndup(expr + 1, expr_len - 2);
          g_free(expr);
          expr = clean_expr;
        }

        double value = 0.0;
        if (!dsl_evaluate_expression(data, expr, &value)) {
          g_print("DSL: Failed to evaluate set expression '%s'\n", expr);
          success = FALSE;
        } else {
          if (var->type == DSL_VAR_UNSET) {
            var->type = DSL_VAR_REAL;
          }
          dsl_runtime_set_variable(data, var_name, value, TRUE);
          variables_changed = TRUE;
        }
        g_free(expr);
        g_free(var_name);
      }
    }
    else if (g_strcmp0(tokens[0], "animate_move") == 0) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_move target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      int from_x = 0, from_y = 0, to_x = 0, to_y = 0;
      int cursor = 2;
      const gchar *from_token = NULL;
      const gchar *to_token = NULL;
      const gchar *after_to_ptr = NULL;
      gboolean from_token_owned = FALSE;
      gboolean to_token_owned = FALSE;

      if (token_count >= 6 && tokens[2][0] == '(' && tokens[3][0] == '(') {
        from_token = tokens[2];
        to_token = tokens[3];
        cursor = 4;
      } else if (token_count >= 3 && tokens[2][0] == '(') {
        from_token = NULL;
        to_token = tokens[2];
        cursor = 3;
      } else {
        const gchar *raw_line = lines[i];
        const gchar *id_pos = strstr(raw_line, elem_id);
        if (id_pos) {
          const gchar *scan = id_pos + strlen(elem_id);
          while (*scan && g_ascii_isspace(*scan)) scan++;

          if (*scan == '(') {
            const gchar *after_first = NULL;
            gchar *first = dsl_copy_paren_token(scan, &after_first);
            if (first) {
              from_token = first;
              from_token_owned = TRUE;
              g_print("DSL: animate_move inferred from point %s\n", from_token);
              scan = after_first;
              while (*scan && g_ascii_isspace(*scan)) scan++;
              if (*scan == '(') {
                const gchar *after_second = NULL;
                gchar *second = dsl_copy_paren_token(scan, &after_second);
                if (second) {
                  to_token = second;
                  to_token_owned = TRUE;
                  after_to_ptr = after_second;
                  cursor = 4;
                  g_print("DSL: animate_move inferred to point %s\n", to_token);
                }
              }
            }
          }
        }
      }

      if (from_token) {
        if (!dsl_parse_point_token(data, from_token, &from_x, &from_y)) {
          g_print("DSL: Failed to parse animate_move positions\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
      } else {
        if (!model_element->position) {
          g_print("DSL: animate_move missing element position data\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        from_x = model_element->position->x;
        from_y = model_element->position->y;
      }

      if (!to_token) {
        if (token_count > cursor && tokens[cursor][0] == '(') {
          to_token = tokens[cursor];
          cursor++;
        }
      }

      if (!to_token) {
        g_print("DSL: animate_move missing destination point for '%s'\n", elem_id);
        if (from_token_owned) g_free((gchar *)from_token);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      if (!dsl_parse_point_token(data, to_token, &to_x, &to_y)) {
        g_print("DSL: Failed to parse animate_move target position\n");
        if (from_token_owned) g_free((gchar *)from_token);
        if (to_token_owned) g_free((gchar *)to_token);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;

      gboolean timing_parsed = FALSE;
      if (token_count >= cursor + 2 && tokens[cursor] && tokens[cursor + 1]) {
        if (dsl_parse_double_token(data, tokens[cursor], &start_time) &&
            dsl_parse_double_token(data, tokens[cursor + 1], &duration)) {
          timing_parsed = TRUE;
        }
      }

      if (!timing_parsed && after_to_ptr) {
        const gchar *scan = after_to_ptr;
        while (*scan && g_ascii_isspace(*scan)) scan++;
        gchar *end_ptr = NULL;
        start_time = g_ascii_strtod(scan, &end_ptr);
        if (end_ptr && end_ptr != scan) {
          scan = end_ptr;
          while (*scan && g_ascii_isspace(*scan)) scan++;
          duration = g_ascii_strtod(scan, &end_ptr);
          if (end_ptr && end_ptr != scan) {
            timing_parsed = TRUE;
            after_to_ptr = end_ptr;
          }
        }
      }

      if (!timing_parsed) {
        g_print("DSL: animate_move missing or invalid timing arguments for '%s'\n", elem_id);
        if (from_token_owned) g_free((gchar *)from_token);
        if (to_token_owned) g_free((gchar *)to_token);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      gboolean interp_parsed = FALSE;
      if (token_count > cursor + 2) {
        const gchar *type_token = tokens[cursor + 2];
        if (type_token && *type_token) {
          interp_parsed = TRUE;
          if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
          else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
          else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
          else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
          else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
          else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
          else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
          else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
          else interp_parsed = FALSE;
        }
      }

      if (!interp_parsed && after_to_ptr) {
        const gchar *scan = after_to_ptr;
        while (*scan && g_ascii_isspace(*scan)) scan++;
        if (*scan) {
          if (g_ascii_strncasecmp(scan, "immediate", 9) == 0) interp = ANIM_INTERP_IMMEDIATE;
          else if (g_ascii_strncasecmp(scan, "linear", 6) == 0) interp = ANIM_INTERP_LINEAR;
          else if (g_ascii_strncasecmp(scan, "bezier", 6) == 0 || g_ascii_strncasecmp(scan, "curve", 5) == 0) interp = ANIM_INTERP_BEZIER;
          else if (g_ascii_strncasecmp(scan, "ease-in", 7) == 0 || g_ascii_strncasecmp(scan, "easein", 6) == 0) interp = ANIM_INTERP_EASE_IN;
          else if (g_ascii_strncasecmp(scan, "ease-out", 8) == 0 || g_ascii_strncasecmp(scan, "easeout", 7) == 0) interp = ANIM_INTERP_EASE_OUT;
          else if (g_ascii_strncasecmp(scan, "bounce", 6) == 0) interp = ANIM_INTERP_BOUNCE;
          else if (g_ascii_strncasecmp(scan, "elastic", 7) == 0) interp = ANIM_INTERP_ELASTIC;
          else if (g_ascii_strncasecmp(scan, "back", 4) == 0) interp = ANIM_INTERP_BACK;
        }
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      dsl_runtime_add_move_animation(data, model_element, from_x, from_y, to_x, to_y,
                                     start_time, duration, interp);
      g_print("DSL: animate_move parsed -> from (%d,%d) to (%d,%d) start %.3f duration %.3f\n",
              from_x, from_y, to_x, to_y, start_time, duration);
      animations_scheduled = TRUE;
      if (from_token_owned) g_free((gchar *)from_token);
      if (to_token_owned) g_free((gchar *)to_token);
    }
    else if (g_strcmp0(tokens[0], "animate_resize") == 0 && token_count >= 4) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_resize target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      int from_w = 0, from_h = 0, to_w = 0, to_h = 0;
      int cursor = 2;
      if (token_count >= 6 && tokens[2][0] == '(' && tokens[3][0] == '(') {
        if (!dsl_parse_point_token(data, tokens[2], &from_w, &from_h) ||
            !dsl_parse_point_token(data, tokens[3], &to_w, &to_h)) {
          g_print("DSL: Failed to parse animate_resize sizes\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 4;
      } else if (tokens[2][0] == '(') {
        if (!model_element->size) {
          g_print("DSL: animate_resize missing element size data\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        from_w = model_element->size->width;
        from_h = model_element->size->height;
        if (!dsl_parse_point_token(data, tokens[2], &to_w, &to_h)) {
          g_print("DSL: Failed to parse animate_resize target size\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 3;
      } else {
        g_print("DSL: Invalid animate_resize syntax\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      if ((cursor + 1) >= token_count) {
        g_print("DSL: animate_resize missing timing arguments\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[cursor], &start_time) ||
          !dsl_parse_double_token(data, tokens[cursor + 1], &duration)) {
        g_print("DSL: animate_resize timing parse error\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      if ((cursor + 2) < token_count) {
        const gchar *type_token = tokens[cursor + 2];
        if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
        else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
        else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
        else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
        else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
        else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
        else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
        else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      dsl_runtime_add_resize_animation(data, model_element, from_w, from_h, to_w, to_h,
                                       start_time, duration, interp);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "animate_rotate") == 0 && token_count >= 4) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_rotate target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      double from_rotation = 0.0, to_rotation = 0.0;
      int cursor = 2;

      // Count ALL numeric tokens to determine syntax
      // animate_rotate ELEMENT TO START DURATION [TYPE]      (3 numeric)
      // animate_rotate ELEMENT FROM TO START DURATION [TYPE] (4 numeric)
      int numeric_count = 0;
      int temp_idx = 2;
      while (temp_idx < token_count && tokens[temp_idx][0] != '(') {
        double dummy;
        if (dsl_parse_double_token(data, tokens[temp_idx], &dummy)) {
          numeric_count++;
          temp_idx++;
        } else {
          break;
        }
      }

      // Determine how many rotation params based on total count
      int rotation_param_count = (numeric_count >= 4) ? 2 : 1;

      if (rotation_param_count == 2) {
        // Two rotation values: from and to
        if (!dsl_parse_double_token(data, tokens[2], &from_rotation) ||
            !dsl_parse_double_token(data, tokens[3], &to_rotation)) {
          g_print("DSL: Failed to parse animate_rotate angles\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 4;
      } else {
        // Single rotation value - animate from current rotation
        if (!model_element->visual_element) {
          g_print("DSL: animate_rotate missing element rotation data\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        from_rotation = model_element->visual_element->rotation_degrees;
        if (!dsl_parse_double_token(data, tokens[2], &to_rotation)) {
          g_print("DSL: Failed to parse animate_rotate target angle\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 3;
      }

      if ((cursor + 1) >= token_count) {
        g_print("DSL: animate_rotate missing timing arguments\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[cursor], &start_time) ||
          !dsl_parse_double_token(data, tokens[cursor + 1], &duration)) {
        g_print("DSL: animate_rotate timing parse error\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      if ((cursor + 2) < token_count) {
        const gchar *type_token = tokens[cursor + 2];
        if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
        else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
        else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
        else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
        else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
        else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
        else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
        else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      dsl_runtime_add_rotate_animation(data, model_element, from_rotation, to_rotation,
                                       start_time, duration, interp);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "animate_color") == 0 && token_count >= 5) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_color target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      const gchar *from_color = tokens[2];
      const gchar *to_color = tokens[3];

      if ((4 + 1) >= token_count) {
        g_print("DSL: animate_color missing timing arguments\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[4], &start_time) ||
          !dsl_parse_double_token(data, tokens[5], &duration)) {
        g_print("DSL: animate_color timing parse error\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      if (6 < token_count) {
        const gchar *type_token = tokens[6];
        if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
        else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
        else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
        else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
        else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
        else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
        else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
        else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      animation_add_color(data->anim_engine, model_element->uuid,
                         start_time, duration, interp,
                         from_color, to_color);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "animate_appear") == 0 && token_count >= 4) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_appear target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[2], &start_time) ||
          !dsl_parse_double_token(data, tokens[3], &duration)) {
        g_print("DSL: animate_appear timing parse error\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      if (4 < token_count) {
        const gchar *type_token = tokens[4];
        if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
        else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
        else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
        else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
        else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
        else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
        else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
        else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      animation_add_create(data->anim_engine, model_element->uuid,
                          start_time, duration, interp);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "animate_disappear") == 0 && token_count >= 4) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_disappear target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[2], &start_time) ||
          !dsl_parse_double_token(data, tokens[3], &duration)) {
        g_print("DSL: animate_disappear timing parse error\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      AnimInterpolationType interp = ANIM_INTERP_LINEAR;
      if (4 < token_count) {
        const gchar *type_token = tokens[4];
        if (g_strcmp0(type_token, "immediate") == 0) interp = ANIM_INTERP_IMMEDIATE;
        else if (g_strcmp0(type_token, "linear") == 0) interp = ANIM_INTERP_LINEAR;
        else if (g_strcmp0(type_token, "bezier") == 0 || g_strcmp0(type_token, "curve") == 0) interp = ANIM_INTERP_BEZIER;
        else if (g_strcmp0(type_token, "ease-in") == 0 || g_strcmp0(type_token, "easein") == 0) interp = ANIM_INTERP_EASE_IN;
        else if (g_strcmp0(type_token, "ease-out") == 0 || g_strcmp0(type_token, "easeout") == 0) interp = ANIM_INTERP_EASE_OUT;
        else if (g_strcmp0(type_token, "bounce") == 0) interp = ANIM_INTERP_BOUNCE;
        else if (g_strcmp0(type_token, "elastic") == 0) interp = ANIM_INTERP_ELASTIC;
        else if (g_strcmp0(type_token, "back") == 0) interp = ANIM_INTERP_BACK;
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      animation_add_delete(data->anim_engine, model_element->uuid,
                          start_time, duration, interp);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "text_update") == 0) {
      if (token_count < 2) {
        g_print("DSL: text_update missing element id\n");
        g_strfreev(tokens);
        continue;
      }

      const gchar *elem_id = tokens[1];
      const gchar *text_token = NULL;
      gboolean fallback_alloc = FALSE;

      if (token_count >= 3) {
        text_token = tokens[2];
      } else {
        const gchar *raw_line = lines[i];
        const gchar *id_pos = strstr(raw_line, elem_id);
        if (id_pos) {
          const gchar *after_id = id_pos + strlen(elem_id);
          while (*after_id && g_ascii_isspace(*after_id)) {
            after_id++;
          }
          if (*after_id != '\0') {
            text_token = g_strdup(after_id);
            fallback_alloc = TRUE;
          }
        }
      }

      if (!text_token || *text_token == '\0') {
        g_print("DSL: text_update missing text payload after '%s'\n", elem_id);
        if (fallback_alloc) {
          g_free((gpointer)text_token);
        }
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      gchar *clean_text = dsl_unescape_text(text_token);
      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);
      if (fallback_alloc) {
        g_free((gpointer)text_token);
      }

      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: text_update target '%s' not found\n", elem_id);
      } else {
        dsl_runtime_text_update(data, model_element, interpolated);
      }

      g_free(interpolated);
    }
    else if (g_strcmp0(tokens[0], "text_bind") == 0 && token_count >= 3) {
      const gchar *element_id = tokens[1];
      const gchar *var_name = tokens[2];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: text_bind references unknown variable '%s'\n", var_name);
        success = FALSE;
      } else {
        dsl_runtime_register_text_binding(data, element_id, var_name);
      }
    }
    else if (g_strcmp0(tokens[0], "position_bind") == 0 && token_count >= 3) {
      const gchar *element_id = tokens[1];
      const gchar *var_name = tokens[2];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: position_bind references unknown variable '%s'\n", var_name);
        success = FALSE;
      } else {
        dsl_runtime_register_position_binding(data, element_id, var_name);
      }
    }
    else if (g_strcmp0(tokens[0], "presentation_next") == 0) {
      canvas_presentation_next_slide(data);
    }
    else if (g_strcmp0(tokens[0], "presentation_auto_next_if") == 0 && token_count >= 3) {
      const gchar *var_name = tokens[1];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: presentation_auto_next_if references unknown variable '%s'\n", var_name);
        success = FALSE;
      } else {
        gboolean is_string = TRUE;
        double expected_value = 0.0;
        const gchar *value_token = tokens[2];
        if (dsl_parse_double_token(data, value_token, &expected_value)) {
          is_string = FALSE;
        }
        dsl_runtime_register_auto_next(data, var_name, is_string, is_string ? value_token : NULL, expected_value);
      }
    }
    else if (g_strcmp0(tokens[0], "shape_create") == 0 && token_count >= 6) {
      // shape_create ID SHAPE_TYPE "Text" (x,y) (width,height) [options...]
      const gchar *id = tokens[1];
      const gchar *shape_type_str = tokens[2];
      const gchar *text_token = tokens[3];

      int shape_type;
      if (!parse_shape_type(shape_type_str, &shape_type)) {
        g_print("DSL: Invalid shape type '%s'\n", shape_type_str);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      // Parse text with interpolation
      gchar *clean_text = NULL;
      if (text_token[0] == '"' && text_token[strlen(text_token)-1] == '"') {
        gchar *quoted = g_strndup(text_token + 1, strlen(text_token) - 2);
        clean_text = dsl_unescape_text(quoted);
        g_free(quoted);
      } else {
        clean_text = dsl_unescape_text(text_token);
      }
      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);

      int x, y, width, height;
      if (!dsl_parse_point_token(data, tokens[4], &x, &y) ||
          !dsl_parse_point_token(data, tokens[5], &width, &height)) {
        g_print("DSL: Failed to parse position/size for shape_create\n");
        g_free(interpolated);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      // Defaults
      double bg_r = 0.95, bg_g = 0.95, bg_b = 0.98, bg_a = 1.0;
      double text_r = 0.1, text_g = 0.1, text_b = 0.1, text_a = 1.0;
      double stroke_r = 0.95, stroke_g = 0.95, stroke_b = 0.98, stroke_a = 1.0;
      int stroke_width = 2;
      gboolean filled = FALSE;
      gchar *font_override = NULL;
      double rotation_degrees = 0.0;

      // Parse optional parameters
      for (int t = 6; t < token_count; t++) {
        if (g_strcmp0(tokens[t], "bg") == 0 && (t+1) < token_count) {
          gchar *resolved = dsl_resolve_numeric_token(data, tokens[++t]);
          parse_color_token(resolved, &bg_r, &bg_g, &bg_b, &bg_a);
          g_free(resolved);
        } else if (g_strcmp0(tokens[t], "text_color") == 0 && (t+1) < token_count) {
          gchar *resolved = dsl_resolve_numeric_token(data, tokens[++t]);
          parse_color_token(resolved, &text_r, &text_g, &text_b, &text_a);
          g_free(resolved);
        } else if (g_strcmp0(tokens[t], "stroke") == 0 && (t+1) < token_count) {
          parse_int_value(tokens[++t], &stroke_width);
        } else if (g_strcmp0(tokens[t], "filled") == 0 && (t+1) < token_count) {
          parse_bool_value(tokens[++t], &filled);
        } else if (g_strcmp0(tokens[t], "font") == 0 && (t+1) < token_count) {
          parse_font_value(tokens[++t], &font_override);
        } else if (g_strcmp0(tokens[t], "rotation") == 0 && (t+1) < token_count) {
          parse_double_value(tokens[++t], &rotation_degrees);
        }
      }

      // Create element
      ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
      ElementColor bg_color = { .r = bg_r, .g = bg_g, .b = bg_b, .a = bg_a };
      ElementColor text_color = { .r = text_r, .g = text_g, .b = text_b, .a = text_a };
      ElementSize size = { .width = width, .height = height };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };
      ElementConnection connection = {
        .from_element_uuid = NULL, .to_element_uuid = NULL,
        .from_point = -1, .to_point = -1,
      };
      ElementDrawing drawing = {
        .drawing_points = NULL,
        .stroke_width = stroke_width,
      };

      ElementText text_elem = {
        .text = interpolated,
        .text_color = text_color,
        .font_description = font_override ? font_override : g_strdup("Ubuntu Bold 14"),
        .alignment = NULL,
      };
      ElementShape shape_elem = {
        .shape_type = shape_type,
        .stroke_width = stroke_width,
        .filled = filled,
        .stroke_style = STROKE_STYLE_SOLID,
        .fill_style = FILL_STYLE_SOLID,
        .stroke_color = { .r = stroke_r, .g = stroke_g, .b = stroke_b, .a = stroke_a },
      };

      ElementConfig config = {
        .type = ELEMENT_SHAPE,
        .bg_color = bg_color,
        .position = position,
        .size = size,
        .media = media,
        .drawing = drawing,
        .connection = connection,
        .text = text_elem,
        .shape = shape_elem,
      };

      ModelElement *model_element = model_create_element(data->model, config);
      if (model_element) {
        if (rotation_degrees != 0.0) {
          model_element->rotation_degrees = rotation_degrees;
        }
        dsl_runtime_register_element(data, id, model_element);
      } else {
        success = FALSE;
      }

      g_free(text_elem.font_description);
      g_free(interpolated);
    }
    else if (g_strcmp0(tokens[0], "element_delete") == 0 && token_count >= 2) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: element_delete target '%s' not found\n", elem_id);
      } else {
        // Push undo action before deletion
        if (data->undo_manager) {
          undo_manager_push_delete_action(data->undo_manager, model_element);
        }
        model_delete_element(data->model, model_element);
        g_print("DSL: element_delete removed '%s'\n", elem_id);
      }
    }
    else if (g_strcmp0(tokens[0], "for") == 0 && token_count >= 4) {
      // For loop: for var start end
      const gchar *loop_var = tokens[1];
      double start_val = 0.0, end_val = 0.0;

      if (!dsl_evaluate_expression(data, tokens[2], &start_val) ||
          !dsl_evaluate_expression(data, tokens[3], &end_val)) {
        g_print("DSL: Failed to evaluate for loop bounds in event block\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      // Create loop variable if it doesn't exist
      DSLVariable *loop_variable = dsl_runtime_ensure_variable(data, loop_var);
      if (!loop_variable) {
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }
      if (loop_variable->type == DSL_VAR_UNSET) {
        loop_variable->type = DSL_VAR_INT;
      }

      // Collect loop body with nesting depth tracking
      GString *loop_body = g_string_new(NULL);
      gboolean found_end = FALSE;
      int nesting_depth = 0;

      for (int j = i + 1; lines[j] != NULL; j++) {
        gchar *body_line = trim_whitespace(lines[j]);
        if (body_line[0] == '#' || body_line[0] == '\0') {
          continue;
        }

        // Check for nested for loops
        int nested_count = 0;
        gchar **nested_tokens = tokenize_line(body_line, &nested_count);
        if (nested_count < 0) {
          success = FALSE;
          g_strfreev(nested_tokens);
          break;
        }
        if (nested_tokens && nested_tokens[0]) {
          if (g_strcmp0(nested_tokens[0], "for") == 0) {
            nesting_depth++;
          } else if (g_strcmp0(nested_tokens[0], "end") == 0) {
            if (nesting_depth > 0) {
              nesting_depth--;
            } else {
              // This is the end for our loop
              g_strfreev(nested_tokens);
              i = j;  // Skip to end marker
              found_end = TRUE;
              break;
            }
          }
        }
        g_strfreev(nested_tokens);

        if (loop_body->len > 0) {
          g_string_append_c(loop_body, '\n');
        }
        g_string_append(loop_body, body_line);
      }

      if (!success) {
        g_string_free(loop_body, TRUE);
        g_strfreev(tokens);
        break;
      }

      if (!found_end) {
        g_print("DSL: Missing 'end' for for loop in event block\n");
        g_string_free(loop_body, TRUE);
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      gchar *body_source = g_string_free(loop_body, FALSE);

      // Execute loop
      int start_int = (int)start_val;
      int end_int = (int)end_val;
      for (int loop_i = start_int; loop_i <= end_int; loop_i++) {
        dsl_runtime_set_variable(data, loop_var, (double)loop_i, FALSE);
        dsl_execute_command_block(data, body_source);
      }

      g_free(body_source);
    }
    else {
      g_print("DSL: Unsupported command in event block: %s\n", tokens[0]);
      success = FALSE;
    }

    g_strfreev(tokens);
    if (!success) {
      break;
    }
  }

  if (variables_changed) {
    dsl_runtime_recompute_expressions(data);
  }

  dsl_runtime_flush_notifications(data);

  if (animation_prepared && animations_scheduled && data->anim_engine) {
    animation_engine_start(data->anim_engine, data->drawing_area, data);
  }

  g_strfreev(lines);
  return success;
}
// Helper function to determine optimal connection points based on relative positions


// Helper function to trim whitespace from a string

// Helper function to parse a point string like "(50, 50)"
// Helper function to parse a point string like "(50, 50)"




// Helper function to parse shape type string

// Helper function to parse a color string like "(.1,.1,.1,.1)"

// Accepts raw '(r,g,b,a)', 'color(...)', 'color=(...)', 'rgba(...)', or '#RRGGBB[AA]'
// Custom tokenizer that handles quotes and parentheses
