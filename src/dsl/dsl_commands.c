#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "canvas_core.h"
#include "element.h"
#include "connection.h"
#include "undo_manager.h"
#include "shape.h"
#include "canvas_drop.h"
#include "paper_note.h"
#include "animation.h"
#include "inline_text.h"

#include "dsl_executor.h"
#include "dsl/dsl_runtime.h"
#include "dsl/dsl_utils.h"
#include "dsl/dsl_commands.h"

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
    if (token_count < 1) {
      g_strfreev(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "add") == 0 && token_count >= 3) {
      const gchar *var_name = tokens[1];
      DSLVariable *var = dsl_runtime_lookup_variable(data, var_name);
      if (!var) {
        g_print("DSL: add references unknown variable '%s'\n", var_name);
        success = FALSE;
      } else if (var->type != DSL_VAR_INT && var->type != DSL_VAR_REAL && var->type != DSL_VAR_UNSET) {
        g_print("DSL: add only supports numeric variables (attempted on '%s')\n", var_name);
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
        double delta = 0.0;
        if (!dsl_evaluate_expression(data, expr, &delta)) {
          g_print("DSL: Failed to evaluate add expression '%s'\n", expr);
          success = FALSE;
        } else {
          double current = var->numeric_value;
          if (var->type == DSL_VAR_INT) {
            dsl_runtime_set_variable(data, var_name, current + delta, TRUE);
          } else {
            if (var->type == DSL_VAR_UNSET) {
              var->type = DSL_VAR_REAL;
              var->numeric_value = 0.0;
            }
            dsl_runtime_set_variable(data, var_name, current + delta, TRUE);
          }
          variables_changed = TRUE;
        }
        g_free(expr);
      }
    }
    else if (g_strcmp0(tokens[0], "animate_move") == 0 && token_count >= 4) {
      const gchar *elem_id = tokens[1];
      ModelElement *model_element = dsl_runtime_lookup_element(data, elem_id);
      if (!model_element) {
        g_print("DSL: animate_move target '%s' not found\n", elem_id);
        g_strfreev(tokens);
        continue;
      }

      int from_x = 0, from_y = 0, to_x = 0, to_y = 0;
      int cursor = 2;
      if (token_count >= 6 && tokens[2][0] == '(' && tokens[3][0] == '(') {
        if (!dsl_parse_point_token(data, tokens[2], &from_x, &from_y) ||
            !dsl_parse_point_token(data, tokens[3], &to_x, &to_y)) {
          g_print("DSL: Failed to parse animate_move positions\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 4;
      } else if (tokens[2][0] == '(') {
        if (!model_element->position) {
          g_print("DSL: animate_move missing element position data\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        from_x = model_element->position->x;
        from_y = model_element->position->y;
        if (!dsl_parse_point_token(data, tokens[2], &to_x, &to_y)) {
          g_print("DSL: Failed to parse animate_move target position\n");
          g_strfreev(tokens);
          success = FALSE;
          continue;
        }
        cursor = 3;
      } else {
        g_print("DSL: Invalid animate_move syntax\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      if ((cursor + 1) >= token_count) {
        g_print("DSL: animate_move missing timing arguments\n");
        g_strfreev(tokens);
        success = FALSE;
        continue;
      }

      double start_time = 0.0;
      double duration = 0.0;
      if (!dsl_parse_double_token(data, tokens[cursor], &start_time) ||
          !dsl_parse_double_token(data, tokens[cursor + 1], &duration)) {
        g_print("DSL: animate_move timing parse error\n");
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
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      dsl_runtime_add_move_animation(data, model_element, from_x, from_y, to_x, to_y,
                                     start_time, duration, interp);
      animations_scheduled = TRUE;
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
      }

      if (!animation_prepared) {
        dsl_runtime_prepare_animation_engine(data);
        animation_prepared = TRUE;
      }

      dsl_runtime_add_resize_animation(data, model_element, from_w, from_h, to_w, to_h,
                                       start_time, duration, interp);
      animations_scheduled = TRUE;
    }
    else if (g_strcmp0(tokens[0], "text_update") == 0 && token_count >= 3) {
      const gchar *elem_id = tokens[1];
      const gchar *text_token = tokens[2];
      gchar *clean_text = dsl_unescape_text(text_token);
      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);

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
    else {
      g_print("DSL: Unsupported command in event block: %s\n", tokens[0]);
      success = FALSE;
    }

    g_strfreev(tokens);
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
