#include <gtk/gtk.h>
#include "canvas.h"
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "canvas_core.h"
#include <stdlib.h>
#include "element.h"
#include "connection.h"
#include "undo_manager.h"
#include "dsl_executor.h"
#include "shape.h"
#include "canvas_drop.h"
#include "paper_note.h"
#include "animation.h"
#include "inline_text.h"
#include <stdarg.h>

#include "dsl/dsl_runtime.h"
#include "dsl/dsl_utils.h"
#include "dsl/dsl_commands.h"
#include "dsl/dsl_type_checker.h"

static void determine_optimal_connection_points(ModelElement *from, ModelElement *to,
                                                int *from_point, int *to_point) {
  if (!from || !to || !from_point || !to_point) return;

  if (!from->position || !from->size || !to->position || !to->size) {
    *from_point = 1;
    *to_point = 3;
    return;
  }

  ConnectionRect from_rect = {
    .x = from->position->x,
    .y = from->position->y,
    .width = from->size->width,
    .height = from->size->height,
  };
  ConnectionRect to_rect = {
    .x = to->position->x,
    .y = to->position->y,
    .width = to->size->width,
    .height = to->size->height,
  };

  connection_determine_optimal_points(from_rect, to_rect, from_point, to_point);
}

static void canvas_execute_script_internal(CanvasData *data, const gchar *script, const gchar *filename) {
  if (!data || !script) {
    g_print("Error: No data or script provided\n");
    return;
  }

  // Check if this is a presentation script (has animation_next_slide commands)
  gboolean is_presentation = (strstr(script, "animation_next_slide") != NULL);

  if (!dsl_type_check_script(data, script, filename)) {
    extern void canvas_show_notification(CanvasData *data, const char *message);
    canvas_show_notification(data, "DSL type check failed");
    return;
  }

  if (is_presentation) {
    // Clean up previous presentation state
    if (data->presentation_dsl_slides) {
      g_strfreev(data->presentation_dsl_slides);
      data->presentation_dsl_slides = NULL;
    }
    if (data->presentation_original_script) {
      g_free(data->presentation_original_script);
      data->presentation_original_script = NULL;
    }

    // Store original script
    data->presentation_original_script = g_strdup(script);

    // Split script by animation_next_slide
    GPtrArray *slides = g_ptr_array_new();
    GString *current_slide = g_string_new(NULL);

    gchar **lines = g_strsplit(script, "\n", 0);
    for (int i = 0; lines[i] != NULL; i++) {
      gchar *line = trim_whitespace(lines[i]);

      if (g_strcmp0(line, "animation_next_slide") == 0) {
        // Finish current slide
        if (current_slide->len > 0) {
          g_ptr_array_add(slides, g_string_free(current_slide, FALSE));
          current_slide = g_string_new(NULL);
        }
      } else {
        // Add line to current slide
        if (current_slide->len > 0) {
          g_string_append_c(current_slide, '\n');
        }
        g_string_append(current_slide, lines[i]);
      }
    }

    // Add final slide
    if (current_slide->len > 0) {
      g_ptr_array_add(slides, g_string_free(current_slide, FALSE));
    } else {
      g_string_free(current_slide, TRUE);
    }

    g_strfreev(lines);

    // Convert to NULL-terminated array
    g_ptr_array_add(slides, NULL);
    data->presentation_dsl_slides = (gchar **)g_ptr_array_free(slides, FALSE);
    data->presentation_slide_count = g_strv_length(data->presentation_dsl_slides);
    data->presentation_current_slide = 0;
    data->presentation_mode_active = TRUE;

    g_print("Presentation mode: %d slides detected\n", data->presentation_slide_count);

    // Execute first slide
    if (data->presentation_dsl_slides[0]) {
      canvas_execute_script(data, data->presentation_dsl_slides[0]);
    }
    return;
  }

  // Check if this is an animation script
  gboolean is_animation_mode = FALSE;
  gboolean is_cycled = FALSE;

  // Check each line to see if it contains animation_mode (ignoring comments)
  gchar **check_lines = g_strsplit(script, "\n", 0);
  for (int i = 0; check_lines[i] != NULL && !is_animation_mode; i++) {
    gchar *check_line = trim_whitespace(check_lines[i]);
    if (check_line[0] != '#' && check_line[0] != '\0') {
      if (g_str_has_prefix(check_line, "animation_mode")) {
        is_animation_mode = TRUE;
        if (strstr(check_line, "cycled") || strstr(check_line, "cycle")) {
          is_cycled = TRUE;
        }
        break;
      }
    }
  }
  g_strfreev(check_lines);

  // Initialize animation engine if in animation mode
  if (is_animation_mode) {
    if (!data->anim_engine) {
      data->anim_engine = g_malloc0(sizeof(AnimationEngine));
    }
    animation_engine_cleanup(data->anim_engine);
    animation_engine_init(data->anim_engine, is_cycled);
    g_print("Animation mode: %s\n", is_cycled ? "cycled" : "single");
  }

  // Reset DSL runtime state for fresh execution
  dsl_runtime_reset(data);

  GHashTable *element_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  GList *connections = NULL;
  GList *new_elements = NULL;
  GList *connection_elements = NULL;

  gchar **lines = g_strsplit(script, "\n", 0);

  // Count total lines for progress reporting
  int total_lines = 0;
  for (int i = 0; lines[i] != NULL; i++) {
    total_lines++;
  }
  g_print("DSL: Processing %d lines...\n", total_lines);

  int progress_interval = total_lines / 10; // Report every 10%
  if (progress_interval < 1000) progress_interval = 1000;

  for (int i = 0; lines[i] != NULL; i++) {
    if (i > 0 && i % progress_interval == 0) {
      g_print("DSL: Processed %d/%d lines (%.1f%%)...\n", i, total_lines, (i * 100.0) / total_lines);
    }
    gchar *line = trim_whitespace(lines[i]);
    if (line[0] == '#' || line[0] == '\0') {
      continue; // Skip comments and empty lines
    }

    int token_count = 0;
    gchar **tokens = tokenize_line(line, &token_count);

    if (token_count < 1) {
      g_strfreev(tokens);
      continue;
    }

    const gchar *cmd = tokens[0];

    gboolean is_global_decl = g_strcmp0(cmd, "global") == 0;
    int type_token_index = is_global_decl ? 1 : 0;

    if (is_global_decl && token_count < 3) {
      g_print("DSL: global declarations require a type and variable name\n");
      g_strfreev(tokens);
      continue;
    }

    const gchar *type_token = tokens[type_token_index];

    if ((g_strcmp0(type_token, "int") == 0 ||
         g_strcmp0(type_token, "real") == 0 ||
         g_strcmp0(type_token, "bool") == 0 ||
         g_strcmp0(type_token, "string") == 0) && token_count >= (type_token_index + 2)) {
      const gchar *var_name = tokens[type_token_index + 1];

      DSLVariable *var = dsl_runtime_ensure_variable(data, var_name);
      if (!var) {
        g_strfreev(tokens);
        continue;
      }

      if (is_global_decl) {
        var->is_global = TRUE;
      }

      DSLVarType target_type = DSL_VAR_REAL;
      if (g_strcmp0(type_token, "int") == 0) target_type = DSL_VAR_INT;
      else if (g_strcmp0(type_token, "real") == 0) target_type = DSL_VAR_REAL;
      else if (g_strcmp0(type_token, "bool") == 0) target_type = DSL_VAR_BOOL;
      else if (g_strcmp0(type_token, "string") == 0) target_type = DSL_VAR_STRING;

      gboolean already_initialized = (var->type != DSL_VAR_UNSET);
      if (already_initialized && var->type != target_type) {
        g_print("DSL: Variable '%s' redeclared with different type\n", var_name);
      }

      gboolean should_assign = TRUE;
      if (is_global_decl && already_initialized && var->type == target_type) {
        should_assign = FALSE;
      }

      if (!already_initialized) {
        var->type = target_type;
      }

      if (!should_assign) {
        for (int j = 0; j < token_count; j++)
          g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      var->type = target_type;
      g_free(var->expression);
      var->expression = NULL;
      g_free(var->string_value);
      var->string_value = NULL;
      var->evaluating = FALSE;

      int expr_start = type_token_index + 2;

      if (target_type == DSL_VAR_STRING) {
        const gchar *literal = (token_count > expr_start) ? tokens[expr_start] : "";
        gchar *clean_text = dsl_unescape_text(literal);
        dsl_runtime_set_string_variable(data, var_name, clean_text ? clean_text : "", FALSE);
        g_free(clean_text);
      } else if (target_type == DSL_VAR_BOOL) {
        if (token_count <= expr_start) {
          dsl_runtime_set_variable(data, var_name, 0.0, FALSE);
        } else {
          GString *expr_builder = g_string_new(NULL);
          for (int t = expr_start; t < token_count; t++) {
            if (expr_builder->len > 0) g_string_append_c(expr_builder, ' ');
            g_string_append(expr_builder, tokens[t]);
          }
          gchar *expr_full = g_string_free(expr_builder, FALSE);
          gchar *trimmed = expr_full ? g_strstrip(expr_full) : NULL;
          if (trimmed && (*trimmed == '{') && trimmed[strlen(trimmed) - 1] == '}') {
            gchar *expression_body = g_strndup(trimmed + 1, strlen(trimmed) - 2);
            var->expression = expression_body;
            double value = 0.0;
            if (!dsl_evaluate_expression(data, expression_body, &value)) {
              value = 0.0;
            }
            dsl_runtime_set_variable(data, var_name, value != 0.0 ? 1.0 : 0.0, FALSE);
          } else if (trimmed && (g_ascii_strcasecmp(trimmed, "true") == 0 ||
                                 g_ascii_strcasecmp(trimmed, "false") == 0 ||
                                 g_ascii_strcasecmp(trimmed, "yes") == 0 ||
                                 g_ascii_strcasecmp(trimmed, "no") == 0 ||
                                 strcmp(trimmed, "1") == 0 ||
                                 strcmp(trimmed, "0") == 0)) {
            gboolean bool_value = (g_ascii_strcasecmp(trimmed, "true") == 0 ||
                                   g_ascii_strcasecmp(trimmed, "yes") == 0 ||
                                   strcmp(trimmed, "1") == 0);
            dsl_runtime_set_variable(data, var_name, bool_value ? 1.0 : 0.0, FALSE);
          } else if (trimmed && dsl_type_is_number_literal(trimmed)) {
            double numeric = g_ascii_strtod(trimmed, NULL);
            dsl_runtime_set_variable(data, var_name, numeric != 0.0 ? 1.0 : 0.0, FALSE);
          } else {
            gchar *expression_body = trimmed ? g_strdup(trimmed) : NULL;
            var->expression = expression_body;
            double value = 0.0;
            if (!dsl_evaluate_expression(data, expression_body, &value)) {
              value = 0.0;
            }
            dsl_runtime_set_variable(data, var_name, value != 0.0 ? 1.0 : 0.0, FALSE);
          }
          g_free(expr_full);
        }
      } else { // int or real
        if (token_count <= expr_start) {
          dsl_runtime_set_variable(data, var_name, 0.0, FALSE);
        } else {
          GString *expr_builder = g_string_new(NULL);
          for (int t = expr_start; t < token_count; t++) {
            if (expr_builder->len > 0) g_string_append_c(expr_builder, ' ');
            g_string_append(expr_builder, tokens[t]);
          }
          gchar *expr_full = g_string_free(expr_builder, FALSE);
          gchar *trimmed = expr_full ? g_strstrip(expr_full) : NULL;
          gboolean is_expression = FALSE;
          double value = 0.0;

          if (trimmed && (*trimmed == '{') && trimmed[strlen(trimmed) - 1] == '}') {
            gchar *expression_body = g_strndup(trimmed + 1, strlen(trimmed) - 2);
            var->expression = expression_body;
            is_expression = TRUE;
            if (!dsl_evaluate_expression(data, expression_body, &value)) {
              value = 0.0;
            }
          } else if (trimmed && dsl_type_is_number_literal(trimmed)) {
            value = g_ascii_strtod(trimmed, NULL);
          } else {
            gchar *expression_body = trimmed ? g_strdup(trimmed) : NULL;
            if (expression_body && *expression_body != '\0') {
              var->expression = expression_body;
              is_expression = TRUE;
              if (!dsl_evaluate_expression(data, expression_body, &value)) {
                value = 0.0;
              }
            } else {
              value = 0.0;
            }
          }

          if (!is_expression) {
            g_free(var->expression);
            var->expression = NULL;
          }

          dsl_runtime_set_variable(data, var_name, value, FALSE);
          g_free(expr_full);
        }
      }

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "text_bind") == 0 && token_count >= 3) {
      const gchar *element_id = tokens[1];
      const gchar *var_name = tokens[2];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: text_bind references unknown variable '%s'\n", var_name);
      } else {
        dsl_runtime_register_text_binding(data, element_id, var_name);
      }

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "position_bind") == 0 && token_count >= 3) {
      const gchar *element_id = tokens[1];
      const gchar *var_name = tokens[2];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: position_bind references unknown variable '%s'\n", var_name);
      } else {
        dsl_runtime_register_position_binding(data, element_id, var_name);
      }

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "presentation_next") == 0) {
      canvas_presentation_next_slide(data);

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "presentation_auto_next_if") == 0 && token_count >= 3) {
      const gchar *var_name = tokens[1];
      if (!dsl_runtime_lookup_variable(data, var_name)) {
        g_print("DSL: presentation_auto_next_if references unknown variable '%s'\n", var_name);
      } else {
        gboolean is_string = TRUE;
        double expected_value = 0.0;
        const gchar *value_token = tokens[2];
        if (dsl_parse_double_token(data, value_token, &expected_value)) {
          is_string = FALSE;
        }
        dsl_runtime_register_auto_next(data, var_name, is_string, is_string ? value_token : NULL, expected_value);
      }

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    if (g_strcmp0(tokens[0], "on") == 0 && token_count >= 3) {
      const gchar *event_type = tokens[1];
      const gchar *target = tokens[2];

      GString *block = g_string_new(NULL);
      gboolean found_end = FALSE;

      for (int j = i + 1; lines[j] != NULL; j++) {
        gchar *block_line = trim_whitespace(lines[j]);
        if (block_line[0] == '#' || block_line[0] == '\0') {
          continue;
        }

        if (g_strcmp0(block_line, "end") == 0) {
          i = j;
          found_end = TRUE;
          break;
        }

        if (block->len > 0) {
          g_string_append_c(block, '\n');
        }
        g_string_append(block, block_line);
      }

      if (!found_end) {
        g_print("DSL: Missing 'end' for on %s %s block\n", event_type, target);
        g_string_free(block, TRUE);
      } else {
        gchar *block_source = g_string_free(block, FALSE);
        if (g_ascii_strcasecmp(event_type, "click") == 0) {
          dsl_runtime_add_click_handler(data, target, block_source);
        } else if (g_ascii_strcasecmp(event_type, "variable") == 0) {
          dsl_runtime_add_variable_handler(data, target, block_source);
        } else {
          g_print("DSL: Unknown event type '%s'\n", event_type);
          g_free(block_source);
        }
      }

      for (int j = 0; j < token_count; j++)
        g_free(tokens[j]);
      g_free(tokens);
      continue;
    }

    // Canvas background settings
    if (g_strcmp0(tokens[0], "canvas_background") == 0 && token_count >= 3) {
      // canvas_background (bg_r,bg_g,bg_b,bg_a) SHOW_GRID (grid_r,grid_g,grid_b,grid_a)
      double bg_r, bg_g, bg_b, bg_a;
      if (parse_color(tokens[1], &bg_r, &bg_g, &bg_b, &bg_a)) {
        gboolean show_grid = FALSE;
        if (g_strcmp0(tokens[2], "true") == 0 || g_strcmp0(tokens[2], "1") == 0) {
          show_grid = TRUE;
        }

        double grid_r = 0.8, grid_g = 0.8, grid_b = 0.8, grid_a = 1.0;
        if (token_count >= 4) {
          if (!parse_color(tokens[3], &grid_r, &grid_g, &grid_b, &grid_a)) {
            g_print("Error: Failed to parse grid color: %s\n", tokens[3]);
          }
        }

        // Convert colors to strings
        gchar *bg_color_str = g_strdup_printf("rgba(%.0f,%.0f,%.0f,%.2f)",
                                              bg_r * 255, bg_g * 255, bg_b * 255, bg_a);
        gchar *grid_color_str = g_strdup_printf("rgba(%.0f,%.0f,%.0f,%.2f)",
                                                grid_r * 255, grid_g * 255, grid_b * 255, grid_a);

        model_set_space_background_color(data->model, data->model->current_space_uuid, bg_color_str);
        model_set_space_grid_settings(data->model, data->model->current_space_uuid, show_grid, grid_color_str);

        g_free(bg_color_str);
        g_free(grid_color_str);

        gtk_widget_queue_draw(data->drawing_area);
      } else {
        g_print("Error: Failed to parse background color: %s\n", tokens[1]);
      }
    }
    else

    // Common note creation function
    if ((g_strcmp0(tokens[0], "note_create") == 0 ||
         g_strcmp0(tokens[0], "paper_note_create") == 0) && token_count >= 5) {

      ElementType element_type = (g_strcmp0(tokens[0], "paper_note_create") == 0) ?
        ELEMENT_PAPER_NOTE : ELEMENT_NOTE;

      const gchar *id = tokens[1];
      const gchar *text_token = tokens[2];

      gchar *clean_text = NULL;
      if (text_token[0] == '"' && text_token[strlen(text_token)-1] == '"') {
        gchar *quoted_text = g_strndup(text_token + 1, strlen(text_token) - 2);
        clean_text = dsl_unescape_text(quoted_text);
        g_free(quoted_text);
      } else {
        clean_text = dsl_unescape_text(text_token);
      }

      int x, y, width, height;
      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);
      clean_text = interpolated;
      if (!dsl_parse_point_token(data, tokens[3], &x, &y) ||
          !dsl_parse_point_token(data, tokens[4], &width, &height)) {
        g_print("Failed to parse position/size for %s\n", tokens[0]);
        g_free(clean_text);
        for (int j = 0; j < token_count; j++)
          g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      gboolean is_paper = (element_type == ELEMENT_PAPER_NOTE);
      double bg_r = is_paper ? 1.0 : 1.0;
      double bg_g = is_paper ? 1.0 : 1.0;
      double bg_b = is_paper ? 0.8 : 1.0;
      double bg_a = 1.0;

      double text_r = 0.2, text_g = 0.2, text_b = 0.2, text_a = 1.0;
      const gchar *default_font = is_paper ? PAPER_NOTE_DEFAULT_FONT : "Ubuntu 16";
      gchar *font_override = NULL;

      gboolean bg_set = FALSE;
      gboolean text_color_set = FALSE;
      gboolean font_set = FALSE;
      gboolean expect_bg = FALSE;
      gboolean expect_text_color = FALSE;
      gboolean expect_font = FALSE;
      double rotation_degrees = 0.0;
      gboolean rotation_set = FALSE;
      gboolean expect_rotation = FALSE;
      gboolean locked = FALSE;
      gboolean locked_set = FALSE;
      gchar *alignment = NULL;
      gboolean alignment_set = FALSE;
      gboolean expect_alignment = FALSE;

      for (int t = 5; t < token_count; t++) {
        const gchar *token = tokens[t];

        if (expect_rotation) {
          if (!parse_double_value(token, &rotation_degrees)) {
            g_print("Failed to parse rotation angle: %s\n", token);
          } else {
            rotation_set = TRUE;
          }
          expect_rotation = FALSE;
          continue;
        }

        if (expect_bg) {
          if (!parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
            g_print("Failed to parse background color: %s\n", token);
          } else {
            bg_set = TRUE;
          }
          expect_bg = FALSE;
          continue;
        }

        if (expect_text_color) {
          if (!parse_color_token(token, &text_r, &text_g, &text_b, &text_a)) {
            g_print("Failed to parse text color: %s\n", token);
          } else {
            text_color_set = TRUE;
          }
          expect_text_color = FALSE;
          continue;
        }

        if (expect_font) {
          if (!parse_font_value(token, &font_override)) {
            g_print("Failed to parse font description: %s\n", token);
          } else {
            font_set = TRUE;
          }
          expect_font = FALSE;
          continue;
        }

        if (!bg_set && (g_strcmp0(token, "bg") == 0 || g_strcmp0(token, "background") == 0)) {
          expect_bg = TRUE;
          continue;
        }
        if (!bg_set && (g_str_has_prefix(token, "bg=") || g_str_has_prefix(token, "background=") ||
                        g_str_has_prefix(token, "bg:") || g_str_has_prefix(token, "background:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_color_token(value + 1, &bg_r, &bg_g, &bg_b, &bg_a)) {
              bg_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse background color: %s\n", token);
          continue;
        }
        if (!bg_set && parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
          bg_set = TRUE;
          continue;
        }

        if (!text_color_set && (g_strcmp0(token, "text_color") == 0 ||
                                 g_strcmp0(token, "text") == 0 ||
                                 g_strcmp0(token, "font_color") == 0)) {
          expect_text_color = TRUE;
          continue;
        }
        if (!text_color_set && (g_str_has_prefix(token, "text_color=") ||
                                 g_str_has_prefix(token, "text=") ||
                                 g_str_has_prefix(token, "font_color="))) {
          const gchar *value = strchr(token, '=');
          if (value && *(value + 1) != '\0' &&
              parse_color_token(value + 1, &text_r, &text_g, &text_b, &text_a)) {
            text_color_set = TRUE;
            continue;
          }
          g_print("Failed to parse text color: %s\n", token);
          continue;
        }

        if (!font_set && g_strcmp0(token, "font") == 0) {
          expect_font = TRUE;
          continue;
        }
        if (!font_set && (g_str_has_prefix(token, "font=") || g_str_has_prefix(token, "font:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_font_value(value + 1, &font_override)) {
              font_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse font description: %s\n", token);
          continue;
        }
        if (!font_set && token[0] == '"') {
          if (parse_font_value(token, &font_override)) {
            font_set = TRUE;
            continue;
          }
        }

        if (!rotation_set && g_strcmp0(token, "rotation") == 0) {
          expect_rotation = TRUE;
          continue;
        }
        if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_double_value(value + 1, &rotation_degrees)) {
              rotation_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse rotation angle: %s\n", token);
          continue;
        }

        if (!locked_set && (g_strcmp0(token, "locked") == 0 || g_strcmp0(token, "lock") == 0)) {
          locked = TRUE;
          locked_set = TRUE;
          continue;
        }
        if (!locked_set && (g_str_has_prefix(token, "locked=") || g_str_has_prefix(token, "locked:") ||
                            g_str_has_prefix(token, "lock=") || g_str_has_prefix(token, "lock:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (g_strcmp0(value + 1, "true") == 0 || g_strcmp0(value + 1, "1") == 0 || g_strcmp0(value + 1, "yes") == 0) {
              locked = TRUE;
              locked_set = TRUE;
            } else if (g_strcmp0(value + 1, "false") == 0 || g_strcmp0(value + 1, "0") == 0 || g_strcmp0(value + 1, "no") == 0) {
              locked = FALSE;
              locked_set = TRUE;
            } else {
              g_print("Failed to parse locked value: %s (expected true/false, 1/0, yes/no)\n", value + 1);
            }
          }
          continue;
        }

        if (expect_alignment) {
          alignment = g_strdup(token);
          alignment_set = TRUE;
          expect_alignment = FALSE;
          continue;
        }

        if (!alignment_set && (g_strcmp0(token, "align") == 0 || g_strcmp0(token, "alignment") == 0)) {
          expect_alignment = TRUE;
          continue;
        }
        if (!alignment_set && (g_str_has_prefix(token, "align=") || g_str_has_prefix(token, "alignment=") ||
                               g_str_has_prefix(token, "align:") || g_str_has_prefix(token, "alignment:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            alignment = g_strdup(value + 1);
            alignment_set = TRUE;
            continue;
          }
          g_print("Failed to parse alignment: %s\n", token);
          continue;
        }

        g_print("Warning: Unrecognized token in %s: %s\n", tokens[0], token);
      }

      ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
      ElementColor bg_color = { .r = bg_r, .g = bg_g, .b = bg_b, .a = bg_a };
      ElementColor text_color = { .r = text_r, .g = text_g, .b = text_b, .a = text_a };
      ElementSize size = { .width = width, .height = height };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };
      ElementConnection connection = {
        .from_element_uuid = NULL,
        .to_element_uuid = NULL,
        .from_point = -1,
        .to_point = -1,
      };
      ElementDrawing drawing = {
        .drawing_points = NULL,
        .stroke_width = 0,
      };

      const gchar *font_to_use = font_override ? font_override : default_font;
      ElementText note_text = {
        .text = clean_text,
        .text_color = text_color,
        .font_description = g_strdup(font_to_use),
        .alignment = alignment,
      };
      ElementShape shape = {
        .shape_type = -1,
        .stroke_width = 0,
        .filled = FALSE,
        .stroke_style = STROKE_STYLE_SOLID,
        .fill_style = FILL_STYLE_SOLID,
        .stroke_color = { .r = bg_color.r, .g = bg_color.g, .b = bg_color.b, .a = 1.0 },
      };

      ElementConfig config = {
        .type = element_type,
        .bg_color = bg_color,
        .position = position,
        .size = size,
        .media = media,
        .drawing = drawing,
        .connection = connection,
        .text = note_text,
        .shape = shape,
      };

      ModelElement *model_element = model_create_element(data->model, config);

      if (model_element) {
        if (rotation_set && rotation_degrees != 0.0) {
          model_element->rotation_degrees = rotation_degrees;
        }
        if (locked_set) {
          model_element->locked = locked;
        }
        g_hash_table_insert(element_map, g_strdup(id), model_element);
        dsl_runtime_register_element(data, id, model_element);
        new_elements = g_list_prepend(new_elements, model_element);
      }

      g_free(note_text.font_description);
      g_free(clean_text);
      g_free(font_override);
      // Note: alignment is owned by note_text, don't free here
    }
    else if (g_strcmp0(tokens[0], "text_create") == 0 && token_count >= 5) {
      // text_create ID "Text" (x,y) (width,height)
      const gchar *id = tokens[1];
      const gchar *text_token = tokens[2];

      gchar *clean_text = NULL;
      if (text_token[0] == '"' && text_token[strlen(text_token)-1] == '"') {
        gchar *quoted_text = g_strndup(text_token + 1, strlen(text_token) - 2);
        clean_text = dsl_unescape_text(quoted_text);
        g_free(quoted_text);
      } else {
        clean_text = dsl_unescape_text(text_token);
      }

      int x, y, width, height;
      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);
      clean_text = interpolated;
      if (!dsl_parse_point_token(data, tokens[3], &x, &y) ||
          !dsl_parse_point_token(data, tokens[4], &width, &height)) {
        g_print("Failed to parse position/size for text_create\n");
        g_free(clean_text);
        for (int j = 0; j < token_count; j++)
          g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      double bg_r = 0.0, bg_g = 0.0, bg_b = 0.0, bg_a = 0.0;
      double text_r = 0.6, text_g = 0.6, text_b = 0.6, text_a = 1.0;
      const gchar *default_font = "Ubuntu Mono 14";
      gchar *font_override = NULL;

      gboolean bg_set = FALSE;
      gboolean text_color_set = FALSE;
      gboolean font_set = FALSE;
      gboolean expect_bg = FALSE;
      gboolean expect_text_color = FALSE;
      gboolean expect_font = FALSE;
      double rotation_degrees = 0.0;
      gboolean rotation_set = FALSE;
      gboolean expect_rotation = FALSE;
      gboolean locked = FALSE;
      gboolean locked_set = FALSE;
      gchar *alignment = NULL;
      gboolean alignment_set = FALSE;
      gboolean expect_alignment = FALSE;

      for (int t = 5; t < token_count; t++) {
        const gchar *token = tokens[t];

        if (expect_rotation) {
          if (!parse_double_value(token, &rotation_degrees)) {
            g_print("Failed to parse rotation angle: %s\n", token);
          } else {
            rotation_set = TRUE;
          }
          expect_rotation = FALSE;
          continue;
        }

        if (expect_bg) {
          if (!parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
            g_print("Failed to parse background color: %s\n", token);
          } else {
            bg_set = TRUE;
          }
          expect_bg = FALSE;
          continue;
        }

        if (expect_text_color) {
          if (!parse_color_token(token, &text_r, &text_g, &text_b, &text_a)) {
            g_print("Failed to parse text color: %s\n", token);
          } else {
            text_color_set = TRUE;
          }
          expect_text_color = FALSE;
          continue;
        }

        if (expect_font) {
          if (!parse_font_value(token, &font_override)) {
            g_print("Failed to parse font description: %s\n", token);
          } else {
            font_set = TRUE;
          }
          expect_font = FALSE;
          continue;
        }

        if (!bg_set && (g_strcmp0(token, "bg") == 0 || g_strcmp0(token, "background") == 0)) {
          expect_bg = TRUE;
          continue;
        }
        if (!bg_set && (g_str_has_prefix(token, "bg=") || g_str_has_prefix(token, "background=") ||
                        g_str_has_prefix(token, "bg:") || g_str_has_prefix(token, "background:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_color_token(value + 1, &bg_r, &bg_g, &bg_b, &bg_a)) {
              bg_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse background color: %s\n", token);
          continue;
        }
        if (!bg_set && parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
          bg_set = TRUE;
          continue;
        }

        if (!text_color_set && (g_strcmp0(token, "text_color") == 0 ||
                                 g_strcmp0(token, "text") == 0 ||
                                 g_strcmp0(token, "font_color") == 0)) {
          expect_text_color = TRUE;
          continue;
        }
        if (!text_color_set && (g_str_has_prefix(token, "text_color=") ||
                                 g_str_has_prefix(token, "text=") ||
                                 g_str_has_prefix(token, "font_color="))) {
          const gchar *value = strchr(token, '=');
          if (value && *(value + 1) != '\0' &&
              parse_color_token(value + 1, &text_r, &text_g, &text_b, &text_a)) {
            text_color_set = TRUE;
            continue;
          }
          g_print("Failed to parse text color: %s\n", token);
          continue;
        }

        if (!font_set && g_strcmp0(token, "font") == 0) {
          expect_font = TRUE;
          continue;
        }
        if (!font_set && (g_str_has_prefix(token, "font=") || g_str_has_prefix(token, "font:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_font_value(value + 1, &font_override)) {
              font_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse font description: %s\n", token);
          continue;
        }
        if (!font_set && token[0] == '"') {
          if (parse_font_value(token, &font_override)) {
            font_set = TRUE;
            continue;
          }
        }

        if (!rotation_set && g_strcmp0(token, "rotation") == 0) {
          expect_rotation = TRUE;
          continue;
        }
        if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_double_value(value + 1, &rotation_degrees)) {
              rotation_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse rotation angle: %s\n", token);
          continue;
        }

        if (!locked_set && (g_strcmp0(token, "locked") == 0 || g_strcmp0(token, "lock") == 0)) {
          locked = TRUE;
          locked_set = TRUE;
          continue;
        }
        if (!locked_set && (g_str_has_prefix(token, "locked=") || g_str_has_prefix(token, "locked:") ||
                            g_str_has_prefix(token, "lock=") || g_str_has_prefix(token, "lock:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (g_strcmp0(value + 1, "true") == 0 || g_strcmp0(value + 1, "1") == 0 || g_strcmp0(value + 1, "yes") == 0) {
              locked = TRUE;
              locked_set = TRUE;
            } else if (g_strcmp0(value + 1, "false") == 0 || g_strcmp0(value + 1, "0") == 0 || g_strcmp0(value + 1, "no") == 0) {
              locked = FALSE;
              locked_set = TRUE;
            } else {
              g_print("Failed to parse locked value: %s (expected true/false, 1/0, yes/no)\n", value + 1);
            }
          }
          continue;
        }

        if (expect_alignment) {
          alignment = g_strdup(token);
          alignment_set = TRUE;
          expect_alignment = FALSE;
          continue;
        }

        if (!alignment_set && (g_strcmp0(token, "align") == 0 || g_strcmp0(token, "alignment") == 0)) {
          expect_alignment = TRUE;
          continue;
        }
        if (!alignment_set && (g_str_has_prefix(token, "align=") || g_str_has_prefix(token, "alignment=") ||
                               g_str_has_prefix(token, "align:") || g_str_has_prefix(token, "alignment:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            alignment = g_strdup(value + 1);
            alignment_set = TRUE;
            continue;
          }
          g_print("Failed to parse alignment: %s\n", token);
          continue;
        }

        g_print("Warning: Unrecognized token in text_create: %s\n", token);
      }

      ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
      ElementColor bg_color = { .r = bg_r, .g = bg_g, .b = bg_b, .a = bg_a };
      ElementColor text_color = { .r = text_r, .g = text_g, .b = text_b, .a = text_a };
      ElementSize size = { .width = width, .height = height };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };
      ElementConnection connection = {
        .from_element_uuid = NULL,
        .to_element_uuid = NULL,
        .from_point = -1,
        .to_point = -1,
      };
      ElementDrawing drawing = {
        .drawing_points = NULL,
        .stroke_width = 0,
      };

      const gchar *font_to_use = font_override ? font_override : default_font;
      ElementText inline_text = {
        .text = clean_text,
        .text_color = text_color,
        .font_description = g_strdup(font_to_use),
        .alignment = alignment,
      };
      ElementShape shape = {
        .shape_type = -1,
        .stroke_width = 0,
        .filled = FALSE,
        .stroke_style = STROKE_STYLE_SOLID,
        .fill_style = FILL_STYLE_SOLID,
        .stroke_color = { .r = bg_color.r, .g = bg_color.g, .b = bg_color.b, .a = 1.0 },
      };

      ElementConfig config = {
        .type = ELEMENT_INLINE_TEXT,
        .bg_color = bg_color,
        .position = position,
        .size = size,
        .media = media,
        .drawing = drawing,
        .connection = connection,
        .text = inline_text,
        .shape = shape,
      };

      ModelElement *model_element = model_create_element(data->model, config);

      if (model_element) {
        if (rotation_set && rotation_degrees != 0.0) {
          model_element->rotation_degrees = rotation_degrees;
        }
        if (locked_set) {
          model_element->locked = locked;
        }
        g_hash_table_insert(element_map, g_strdup(id), model_element);
        dsl_runtime_register_element(data, id, model_element);
        new_elements = g_list_prepend(new_elements, model_element);
      }

      g_free(inline_text.font_description);
      g_free(clean_text);
      g_free(font_override);
      // Note: alignment is owned by inline_text, don't free here
    }
    else if (g_strcmp0(tokens[0], "image_create") == 0 && token_count >= 5) {
      // image_create ID PATH (x,y) (width,height) [rotation DEGREES]
      const gchar *id = tokens[1];
      const gchar *path = tokens[2];
      int x, y, width, height;

      if (dsl_parse_point_token(data, tokens[3], &x, &y) &&
          dsl_parse_point_token(data, tokens[4], &width, &height)) {

        double rotation_degrees = 0.0;
        gboolean rotation_set = FALSE;

        for (int t = 5; t < token_count; t++) {
          const gchar *token = tokens[t];
          if (!rotation_set && g_strcmp0(token, "rotation") == 0 && (t + 1) < token_count) {
            if (parse_double_value(tokens[t + 1], &rotation_degrees)) {
              rotation_set = TRUE;
              t++;
            }
          } else if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
            const gchar *value = strchr(token, '=');
            if (!value) value = strchr(token, ':');
            if (value && *(value + 1) != '\0') {
              if (parse_double_value(value + 1, &rotation_degrees)) {
                rotation_set = TRUE;
              }
            }
          }
        }

        // Load image from file
        GError *error = NULL;
        gchar *contents = NULL;
        gsize length = 0;

        if (!g_file_get_contents(path, &contents, &length, &error)) {
          g_print("Failed to load image from %s: %s\n", path, error ? error->message : "unknown error");
          if (error) g_error_free(error);
        } else {
          ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
          ElementColor bg_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 };
          ElementColor text_color = { .r = 0.1, .g = 0.1, .b = 0.1, .a = 1.0 };
          ElementSize size = { .width = width, .height = height };
          ElementMedia media = {
            .type = MEDIA_TYPE_IMAGE,
            .image_data = (unsigned char*)contents,
            .image_size = length,
            .video_data = NULL,
            .video_size = 0,
            .duration = 0
          };
          ElementConnection connection = {
            .from_element_uuid = NULL,
            .to_element_uuid = NULL,
            .from_point = -1,
            .to_point = -1,
          };
          ElementDrawing drawing = {
            .drawing_points = NULL,
            .stroke_width = 0,
          };
          ElementText text = {
            .text = g_strdup(""),
            .text_color = text_color,
            .font_description = g_strdup(PAPER_NOTE_DEFAULT_FONT),
            .alignment = g_strdup("bottom-right"),
          };
          ElementShape shape = {
            .shape_type = -1,
            .stroke_width = 0,
            .filled = FALSE,
            .stroke_style = STROKE_STYLE_SOLID,
            .fill_style = FILL_STYLE_SOLID,
            .stroke_color = { .r = bg_color.r, .g = bg_color.g, .b = bg_color.b, .a = 1.0 },
          };

          ElementConfig config = {
            .type = ELEMENT_MEDIA_FILE,
            .bg_color = bg_color,
            .position = position,
            .size = size,
            .media = media,
            .drawing = drawing,
            .connection = connection,
            .text = text,
            .shape = shape,
          };

          ModelElement *model_element = model_create_element(data->model, config);

          if (model_element) {
            if (rotation_set && rotation_degrees != 0.0) {
              model_element->rotation_degrees = rotation_degrees;
            }
            g_hash_table_insert(element_map, g_strdup(id), model_element);
            dsl_runtime_register_element(data, id, model_element);
            new_elements = g_list_prepend(new_elements, model_element);
          }
        }
      } else {
        g_print("Failed to parse parameters for image_create\n");
      }
    }
    else if (g_strcmp0(tokens[0], "video_create") == 0 && token_count >= 5) {
      // video_create ID PATH (x,y) (width,height) [rotation DEGREES]
      const gchar *id = tokens[1];
      const gchar *path = tokens[2];
      int x, y, width, height;

      if (dsl_parse_point_token(data, tokens[3], &x, &y) &&
          dsl_parse_point_token(data, tokens[4], &width, &height)) {

        double rotation_degrees = 0.0;
        gboolean rotation_set = FALSE;

        for (int t = 5; t < token_count; t++) {
          const gchar *token = tokens[t];
          if (!rotation_set && g_strcmp0(token, "rotation") == 0 && (t + 1) < token_count) {
            if (parse_double_value(tokens[t + 1], &rotation_degrees)) {
              rotation_set = TRUE;
              t++;
            }
          } else if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
            const gchar *value = strchr(token, '=');
            if (!value) value = strchr(token, ':');
            if (value && *(value + 1) != '\0') {
              if (parse_double_value(value + 1, &rotation_degrees)) {
                rotation_set = TRUE;
              }
            }
          }
        }

        // Load video from file
        GError *error = NULL;
        gchar *contents = NULL;
        gsize length = 0;

        if (!g_file_get_contents(path, &contents, &length, &error)) {
          g_print("Failed to load video from %s: %s\n", path, error ? error->message : "unknown error");
          if (error) g_error_free(error);
        } else {
          // Get video duration
          gint64 duration_seconds = get_mp4_duration(path);
          if (duration_seconds <= 0) {
            g_print("Failed to get duration for video %s\n", path);
            g_free(contents);
            for (int j = 0; j < token_count; j++)
              g_free(tokens[j]);
            g_free(tokens);
            continue;
          }

          // Generate thumbnail
          GstSample *thumbnail_sample = generate_video_thumbnail(path);
          if (!thumbnail_sample) {
            g_print("Failed to generate thumbnail for video %s\n", path);
            g_free(contents);
            for (int j = 0; j < token_count; j++)
              g_free(tokens[j]);
            g_free(tokens);
            continue;
          }

          GdkPixbuf *thumbnail = sample_to_pixbuf(thumbnail_sample);
          gst_sample_unref(thumbnail_sample);

          if (!thumbnail) {
            g_print("Failed to convert thumbnail to pixbuf for video %s\n", path);
            g_free(contents);
            for (int j = 0; j < token_count; j++)
              g_free(tokens[j]);
            g_free(tokens);
            continue;
          }

          // Convert pixbuf to PNG data for storage
          gchar *thumb_buffer = NULL;
          gsize thumb_size = 0;
          GError *thumb_error = NULL;
          if (!gdk_pixbuf_save_to_buffer(thumbnail, &thumb_buffer, &thumb_size, "png", &thumb_error, NULL)) {
            g_print("Failed to save thumbnail to buffer: %s\n", thumb_error ? thumb_error->message : "unknown");
            if (thumb_error) g_error_free(thumb_error);
            g_object_unref(thumbnail);
            g_free(contents);
            for (int j = 0; j < token_count; j++)
              g_free(tokens[j]);
            g_free(tokens);
            continue;
          }
          g_object_unref(thumbnail);

          ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
          ElementColor bg_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 };
          ElementColor text_color = { .r = 0.1, .g = 0.1, .b = 0.1, .a = 1.0 };
          ElementSize size = { .width = width, .height = height };
          ElementMedia media = {
            .type = MEDIA_TYPE_VIDEO,
            .image_data = (unsigned char*)thumb_buffer,
            .image_size = thumb_size,
            .video_data = (unsigned char*)contents,
            .video_size = length,
            .duration = (int)duration_seconds
          };
          ElementConnection connection = {
            .from_element_uuid = NULL,
            .to_element_uuid = NULL,
            .from_point = -1,
            .to_point = -1,
          };
          ElementDrawing drawing = {
            .drawing_points = NULL,
            .stroke_width = 0,
          };
          ElementText text = {
            .text = g_strdup(""),
            .text_color = text_color,
            .font_description = g_strdup(PAPER_NOTE_DEFAULT_FONT),
            .alignment = g_strdup("bottom-right"),
          };
          ElementShape shape = {
            .shape_type = -1,
            .stroke_width = 0,
            .filled = FALSE,
            .stroke_style = STROKE_STYLE_SOLID,
            .fill_style = FILL_STYLE_SOLID,
            .stroke_color = { .r = bg_color.r, .g = bg_color.g, .b = bg_color.b, .a = 1.0 },
          };

          ElementConfig config = {
            .type = ELEMENT_MEDIA_FILE,
            .bg_color = bg_color,
            .position = position,
            .size = size,
            .media = media,
            .drawing = drawing,
            .connection = connection,
            .text = text,
            .shape = shape,
          };

          ModelElement *model_element = model_create_element(data->model, config);

          if (model_element) {
            if (rotation_set && rotation_degrees != 0.0) {
              model_element->rotation_degrees = rotation_degrees;
            }
            g_hash_table_insert(element_map, g_strdup(id), model_element);
            dsl_runtime_register_element(data, id, model_element);
            new_elements = g_list_prepend(new_elements, model_element);
          }
        }
      } else {
        g_print("Failed to parse parameters for video_create\n");
      }
    }
    else if (g_strcmp0(tokens[0], "space_create") == 0 && token_count >= 5) {
      // space_create ID "Text" (x,y) (width,height)
      const gchar *id = tokens[1];
      const gchar *text = tokens[2];

      gchar *clean_text = NULL;
      if (text[0] == '"' && text[strlen(text)-1] == '"') {
        gchar *quoted_text = g_strndup(text + 1, strlen(text) - 2);
        clean_text = dsl_unescape_text(quoted_text);
        g_free(quoted_text);
      } else {
        clean_text = dsl_unescape_text(text);
      }

      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);
      clean_text = interpolated;

      int x, y, width, height;

      if (dsl_parse_point_token(data, tokens[3], &x, &y) &&
          dsl_parse_point_token(data, tokens[4], &width, &height)) {

        double rotation_degrees = 0.0;
        gboolean rotation_set = FALSE;

        for (int t = 5; t < token_count; t++) {
          const gchar *token = tokens[t];
          if (!rotation_set && g_strcmp0(token, "rotation") == 0 && (t + 1) < token_count) {
            if (parse_double_value(tokens[t + 1], &rotation_degrees)) {
              rotation_set = TRUE;
              t++;
            }
          } else if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
            const gchar *value = strchr(token, '=');
            if (!value) value = strchr(token, ':');
            if (value && *(value + 1) != '\0') {
              if (parse_double_value(value + 1, &rotation_degrees)) {
                rotation_set = TRUE;
              }
            }
          }
        }

        ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
        ElementColor bg_color = { .r = 0.9, .g = 0.9, .b = 0.95, .a = 1.0 };
        ElementColor text_color = { .r = 0.1, .g = 0.1, .b = 0.1, .a = 1.0 };
        ElementSize size = { .width = width, .height = height };
        ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                               .video_data = NULL, .video_size = 0, .duration = 0 };
        ElementConnection connection = {
          .from_element_uuid = NULL,
          .to_element_uuid = NULL,
          .from_point = -1,
          .to_point = -1,
        };
        ElementDrawing drawing = {
          .drawing_points = NULL,
          .stroke_width = 0,
        };
        ElementText text_elem = {
          .text = clean_text,
          .text_color = text_color,
          .font_description = PAPER_NOTE_DEFAULT_FONT,
        };
        ElementShape shape = {
          .shape_type = -1,
          .stroke_width = 0,
          .filled = FALSE,
          .stroke_style = STROKE_STYLE_SOLID,
          .fill_style = FILL_STYLE_SOLID,
          .stroke_color = { .r = bg_color.r, .g = bg_color.g, .b = bg_color.b, .a = 1.0 },
        };

        ElementConfig config = {
          .type = ELEMENT_SPACE,
          .bg_color = bg_color,
          .position = position,
          .size = size,
          .media = media,
          .drawing = drawing,
          .connection = connection,
          .text = text_elem,
          .shape = shape,
        };

        ModelElement *model_element = model_create_element(data->model, config);

        if (model_element) {
          if (rotation_set && rotation_degrees != 0.0) {
            model_element->rotation_degrees = rotation_degrees;
          }
          g_hash_table_insert(element_map, g_strdup(id), model_element);
          dsl_runtime_register_element(data, id, model_element);
          new_elements = g_list_prepend(new_elements, model_element);
        }
      } else {
        g_print("Failed to parse parameters for space_create\n");
      }

      g_free(clean_text);
    }
    else if (g_strcmp0(tokens[0], "shape_create") == 0 && token_count >= 6) {
      // shape_create ID SHAPE_TYPE "Text" (x,y) (width,height)
      // Optional: bg, stroke, filled, font, text_color (in any order, with or without key=value)
      const gchar *id = tokens[1];
      const gchar *shape_type_str = tokens[2];
      const gchar *text_token = tokens[3];

      int shape_type;
      if (!parse_shape_type(shape_type_str, &shape_type)) {
        g_print("Invalid shape type: %s\n", shape_type_str);
        for (int j = 0; j < token_count; j++)
          g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      gchar *clean_text = NULL;
      if (text_token[0] == '"' && text_token[strlen(text_token)-1] == '"') {
        gchar *quoted_text = g_strndup(text_token + 1, strlen(text_token) - 2);
        clean_text = dsl_unescape_text(quoted_text);
        g_free(quoted_text);
      } else {
        clean_text = dsl_unescape_text(text_token);
      }

      gchar *interpolated = dsl_interpolate_text(data, clean_text);
      g_free(clean_text);
      clean_text = interpolated;

      int x, y, width, height;
      if (!dsl_parse_point_token(data, tokens[4], &x, &y) ||
          !dsl_parse_point_token(data, tokens[5], &width, &height)) {
        g_print("Failed to parse position/size for shape_create\n");
        g_free(clean_text);
        for (int j = 0; j < token_count; j++)
          g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      double bg_r = 0.95, bg_g = 0.95, bg_b = 0.98, bg_a = 1.0;
      double text_r = 0.1, text_g = 0.1, text_b = 0.1, text_a = 1.0;
      double stroke_r = 0.95, stroke_g = 0.95, stroke_b = 0.98, stroke_a = 1.0;
      int stroke_width = 2;
      gboolean filled = FALSE;
      const gchar *default_font = "Ubuntu Bold 14";
      gchar *font_override = NULL;

      gboolean bg_set = FALSE;
      gboolean text_color_set = FALSE;
      gboolean stroke_color_set = FALSE;
      gboolean font_set = FALSE;
      gboolean stroke_set = FALSE;
      gboolean filled_set = FALSE;
      gboolean expect_line_start = FALSE;
      gboolean expect_line_end = FALSE;
      double line_start_u = 0.0, line_start_v = 0.0;
      double line_end_u = 1.0, line_end_v = 1.0;
      gboolean line_start_defined = FALSE;
      gboolean line_end_defined = FALSE;

      StrokeStyle stroke_style_value = STROKE_STYLE_SOLID;
      FillStyle fill_style_value = FILL_STYLE_SOLID;

      gboolean expect_bg = FALSE;
      gboolean expect_text_color = FALSE;
      gboolean expect_stroke_color = FALSE;
      gboolean expect_font = FALSE;
      gboolean expect_stroke = FALSE;
      gboolean expect_filled = FALSE;
      gboolean expect_stroke_style = FALSE;
      gboolean expect_fill_style = FALSE;
      double rotation_degrees = 0.0;
      gboolean rotation_set = FALSE;
      gboolean expect_rotation = FALSE;
      gboolean locked = FALSE;
      gboolean locked_set = FALSE;
      gchar *alignment = NULL;
      gboolean alignment_set = FALSE;
      gboolean expect_alignment = FALSE;

      // Bezier curve support
      gboolean expect_bezier_p0 = FALSE;
      gboolean expect_bezier_p1 = FALSE;
      gboolean expect_bezier_p2 = FALSE;
      gboolean expect_bezier_p3 = FALSE;
      double bezier_p0_u = 0.0, bezier_p0_v = 0.0;
      double bezier_p1_u = 0.25, bezier_p1_v = 0.0;
      double bezier_p2_u = 0.75, bezier_p2_v = 1.0;
      double bezier_p3_u = 1.0, bezier_p3_v = 1.0;

      for (int t = 6; t < token_count; t++) {
        const gchar *token = tokens[t];

        if (expect_rotation) {
          if (!parse_double_value(token, &rotation_degrees)) {
            g_print("Failed to parse rotation angle: %s\n", token);
          } else {
            rotation_set = TRUE;
          }
          expect_rotation = FALSE;
          continue;
        }

        if (expect_stroke_style) {
          if (!parse_stroke_style_value(token, &stroke_style_value)) {
            g_print("Failed to parse stroke style: %s\n", token);
          }
          expect_stroke_style = FALSE;
          continue;
        }

        if (expect_fill_style) {
          if (!parse_fill_style_value(token, &fill_style_value)) {
            g_print("Failed to parse fill style: %s\n", token);
          }
          expect_fill_style = FALSE;
          continue;
        }

        if (expect_bg) {
          if (!parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
            g_print("Failed to parse background color: %s\n", token);
          } else {
            bg_set = TRUE;
          }
          expect_bg = FALSE;
          continue;
        }

        if (expect_text_color) {
          if (!parse_color_token(token, &text_r, &text_g, &text_b, &text_a)) {
            g_print("Failed to parse text color: %s\n", token);
          } else {
            text_color_set = TRUE;
          }
          expect_text_color = FALSE;
          continue;
        }

        if (expect_stroke_color) {
          if (!parse_color_token(token, &stroke_r, &stroke_g, &stroke_b, &stroke_a)) {
            g_print("Failed to parse stroke color: %s\n", token);
          } else {
            stroke_color_set = TRUE;
          }
          expect_stroke_color = FALSE;
          continue;
        }

        if (expect_font) {
          if (!parse_font_value(token, &font_override)) {
            g_print("Failed to parse font description: %s\n", token);
          } else {
            font_set = TRUE;
          }
          expect_font = FALSE;
          continue;
        }

        if (expect_stroke) {
          int value;
          if (parse_int_value(token, &value)) {
            stroke_width = value;
            stroke_set = TRUE;
            expect_stroke = FALSE;
            continue;
          } else {
            g_print("Failed to parse stroke width: %s\n", token);
            expect_stroke = FALSE;
          }
        }

        if (expect_filled) {
          gboolean bool_value;
          if (parse_bool_value(token, &bool_value)) {
            filled = bool_value;
            filled_set = TRUE;
            expect_filled = FALSE;
            continue;
          } else {
            // Treat previous 'filled' token as a toggle to TRUE
            filled = TRUE;
            filled_set = TRUE;
            expect_filled = FALSE;
            // Fall through to allow this token to be processed further
          }
        }

        if (expect_line_start && (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse line start point: %s\n", token);
          } else {
            line_start_u = fx;
            line_start_v = fy;
            line_start_defined = TRUE;
          }
          expect_line_start = FALSE;
          continue;
        }

        if (expect_line_end && (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse line end point: %s\n", token);
          } else {
            line_end_u = fx;
            line_end_v = fy;
            line_end_defined = TRUE;
          }
          expect_line_end = FALSE;
          continue;
        }

        if (!bg_set && (g_strcmp0(token, "bg") == 0 || g_strcmp0(token, "background") == 0)) {
          expect_bg = TRUE;
          continue;
        }
        if (!bg_set && (g_str_has_prefix(token, "bg=") || g_str_has_prefix(token, "background=") ||
                        g_str_has_prefix(token, "bg:") || g_str_has_prefix(token, "background:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_color_token(value + 1, &bg_r, &bg_g, &bg_b, &bg_a)) {
              bg_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse background color: %s\n", token);
          continue;
        }
        if (!bg_set && parse_color_token(token, &bg_r, &bg_g, &bg_b, &bg_a)) {
          bg_set = TRUE;
          continue;
        }

        if (!text_color_set && (g_strcmp0(token, "text_color") == 0 ||
                                 g_strcmp0(token, "text") == 0 ||
                                 g_strcmp0(token, "font_color") == 0)) {
          expect_text_color = TRUE;
          continue;
        }
        if (!text_color_set && (g_str_has_prefix(token, "text_color=") ||
                                 g_str_has_prefix(token, "text=") ||
                                 g_str_has_prefix(token, "font_color="))) {
          const gchar *value = strchr(token, '=');
          if (value && *(value + 1) != '\0' &&
              parse_color_token(value + 1, &text_r, &text_g, &text_b, &text_a)) {
            text_color_set = TRUE;
            continue;
          }
          g_print("Failed to parse text color: %s\n", token);
          continue;
        }

        if (!stroke_color_set && (g_strcmp0(token, "stroke_color") == 0 ||
                                  g_strcmp0(token, "stroke-color") == 0)) {
          expect_stroke_color = TRUE;
          continue;
        }
        if (!stroke_color_set && (g_str_has_prefix(token, "stroke_color=") ||
                                  g_str_has_prefix(token, "stroke-color=") ||
                                  g_str_has_prefix(token, "stroke_color:") ||
                                  g_str_has_prefix(token, "stroke-color:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0' &&
              parse_color_token(value + 1, &stroke_r, &stroke_g, &stroke_b, &stroke_a)) {
            stroke_color_set = TRUE;
            continue;
          }
          g_print("Failed to parse stroke color: %s\n", token);
          continue;
        }

        if (!font_set && g_strcmp0(token, "font") == 0) {
          expect_font = TRUE;
          continue;
        }
        if (!font_set && (g_str_has_prefix(token, "font=") || g_str_has_prefix(token, "font:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_font_value(value + 1, &font_override)) {
              font_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse font description: %s\n", token);
          continue;
        }
        if (!font_set && token[0] == '"') {
          if (parse_font_value(token, &font_override)) {
            font_set = TRUE;
            continue;
          }
        }

        if (!stroke_set && (g_strcmp0(token, "stroke") == 0 ||
                             g_strcmp0(token, "stroke_width") == 0)) {
          expect_stroke = TRUE;
          continue;
        }
        if (!stroke_set && (g_str_has_prefix(token, "stroke=") ||
                             g_str_has_prefix(token, "stroke_width=") ||
                             g_str_has_prefix(token, "stroke:") ||
                             g_str_has_prefix(token, "stroke_width:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            int parsed_width;
            if (parse_int_value(value + 1, &parsed_width)) {
              stroke_width = parsed_width;
              stroke_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse stroke width: %s\n", token);
          continue;
        }

        if (!filled_set && (g_strcmp0(token, "filled") == 0 || g_strcmp0(token, "fill") == 0)) {
          expect_filled = TRUE;
          continue;
        }
        if (!filled_set && (g_str_has_prefix(token, "filled=") || g_str_has_prefix(token, "fill=") ||
                             g_str_has_prefix(token, "filled:") || g_str_has_prefix(token, "fill:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            gboolean bool_value;
            if (parse_bool_value(value + 1, &bool_value)) {
              filled = bool_value;
              filled_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse filled value: %s\n", token);
          continue;
        }

        if ((shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) &&
            (g_strcmp0(token, "line_from") == 0 || g_strcmp0(token, "line_start") == 0)) {
          expect_line_start = TRUE;
          continue;
        }

        if ((shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) &&
            (g_strcmp0(token, "line_to") == 0 || g_strcmp0(token, "line_end") == 0)) {
          expect_line_end = TRUE;
          continue;
        }

        if ((shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) &&
            (g_str_has_prefix(token, "line_from=") || g_str_has_prefix(token, "line_from:") ||
             g_str_has_prefix(token, "line_start=") || g_str_has_prefix(token, "line_start:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            double fx, fy;
            if (parse_float_point(value + 1, &fx, &fy)) {
              line_start_u = fx;
              line_start_v = fy;
              line_start_defined = TRUE;
              continue;
            }
          }
          g_print("Failed to parse line start point: %s\n", token);
          continue;
        }

        if (g_strcmp0(token, "stroke_style") == 0 || g_strcmp0(token, "stroke-style") == 0) {
          expect_stroke_style = TRUE;
          continue;
        }
        if (g_str_has_prefix(token, "stroke_style=") || g_str_has_prefix(token, "stroke_style:") ||
            g_str_has_prefix(token, "stroke-style=") || g_str_has_prefix(token, "stroke-style:")) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_stroke_style_value(value + 1, &stroke_style_value)) {
              continue;
            }
          }
          g_print("Failed to parse stroke style: %s\n", token);
          continue;
        }

        if (g_strcmp0(token, "fill_style") == 0 || g_strcmp0(token, "fill-style") == 0) {
          expect_fill_style = TRUE;
          continue;
        }
        if (g_str_has_prefix(token, "fill_style=") || g_str_has_prefix(token, "fill_style:") ||
            g_str_has_prefix(token, "fill-style=") || g_str_has_prefix(token, "fill-style:")) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_fill_style_value(value + 1, &fill_style_value)) {
              continue;
            }
          }
          g_print("Failed to parse fill style: %s\n", token);
          continue;
        }

        if (!rotation_set && g_strcmp0(token, "rotation") == 0) {
          expect_rotation = TRUE;
          continue;
        }
        if (!rotation_set && (g_str_has_prefix(token, "rotation=") || g_str_has_prefix(token, "rotation:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (parse_double_value(value + 1, &rotation_degrees)) {
              rotation_set = TRUE;
              continue;
            }
          }
          g_print("Failed to parse rotation angle: %s\n", token);
          continue;
        }

        if (!locked_set && (g_strcmp0(token, "locked") == 0 || g_strcmp0(token, "lock") == 0)) {
          locked = TRUE;
          locked_set = TRUE;
          continue;
        }
        if (!locked_set && (g_str_has_prefix(token, "locked=") || g_str_has_prefix(token, "locked:") ||
                            g_str_has_prefix(token, "lock=") || g_str_has_prefix(token, "lock:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            if (g_strcmp0(value + 1, "true") == 0 || g_strcmp0(value + 1, "1") == 0 || g_strcmp0(value + 1, "yes") == 0) {
              locked = TRUE;
              locked_set = TRUE;
            } else if (g_strcmp0(value + 1, "false") == 0 || g_strcmp0(value + 1, "0") == 0 || g_strcmp0(value + 1, "no") == 0) {
              locked = FALSE;
              locked_set = TRUE;
            } else {
              g_print("Failed to parse locked value: %s (expected true/false, 1/0, yes/no)\n", value + 1);
            }
          }
          continue;
        }

        if (expect_alignment) {
          alignment = g_strdup(token);
          alignment_set = TRUE;
          expect_alignment = FALSE;
          continue;
        }

        if (!alignment_set && (g_strcmp0(token, "align") == 0 || g_strcmp0(token, "alignment") == 0)) {
          expect_alignment = TRUE;
          continue;
        }
        if (!alignment_set && (g_str_has_prefix(token, "align=") || g_str_has_prefix(token, "alignment=") ||
                               g_str_has_prefix(token, "align:") || g_str_has_prefix(token, "alignment:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            alignment = g_strdup(value + 1);
            alignment_set = TRUE;
            continue;
          }
          g_print("Failed to parse alignment: %s\n", token);
          continue;
        }

        // Bezier curve control points
        if (expect_bezier_p0 && (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse bezier p0 point: %s\n", token);
          } else {
            bezier_p0_u = fx;
            bezier_p0_v = fy;
          }
          expect_bezier_p0 = FALSE;
          continue;
        }

        if (expect_bezier_p1 && (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse bezier p1 point: %s\n", token);
          } else {
            bezier_p1_u = fx;
            bezier_p1_v = fy;
          }
          expect_bezier_p1 = FALSE;
          continue;
        }

        if (expect_bezier_p2 && (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse bezier p2 point: %s\n", token);
          } else {
            bezier_p2_u = fx;
            bezier_p2_v = fy;
          }
          expect_bezier_p2 = FALSE;
          continue;
        }

        if (expect_bezier_p3 && (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW)) {
          double fx, fy;
          if (!parse_float_point(token, &fx, &fy)) {
            g_print("Failed to parse bezier p3 point: %s\n", token);
          } else {
            bezier_p3_u = fx;
            bezier_p3_v = fy;
          }
          expect_bezier_p3 = FALSE;
          continue;
        }

        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_strcmp0(token, "p0") == 0 || g_strcmp0(token, "bezier_p0") == 0)) {
          expect_bezier_p0 = TRUE;
          continue;
        }
        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_strcmp0(token, "p1") == 0 || g_strcmp0(token, "bezier_p1") == 0)) {
          expect_bezier_p1 = TRUE;
          continue;
        }
        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_strcmp0(token, "p2") == 0 || g_strcmp0(token, "bezier_p2") == 0)) {
          expect_bezier_p2 = TRUE;
          continue;
        }
        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_strcmp0(token, "p3") == 0 || g_strcmp0(token, "bezier_p3") == 0)) {
          expect_bezier_p3 = TRUE;
          continue;
        }

        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_str_has_prefix(token, "p0=") || g_str_has_prefix(token, "bezier_p0=") ||
                                            g_str_has_prefix(token, "p0:") || g_str_has_prefix(token, "bezier_p0:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            double fx, fy;
            if (parse_float_point(value + 1, &fx, &fy)) {
              bezier_p0_u = fx;
              bezier_p0_v = fy;
              continue;
            }
          }
          g_print("Failed to parse bezier p0 point: %s\n", token);
          continue;
        }

        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_str_has_prefix(token, "p1=") || g_str_has_prefix(token, "bezier_p1=") ||
                                            g_str_has_prefix(token, "p1:") || g_str_has_prefix(token, "bezier_p1:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            double fx, fy;
            if (parse_float_point(value + 1, &fx, &fy)) {
              bezier_p1_u = fx;
              bezier_p1_v = fy;
              continue;
            }
          }
          g_print("Failed to parse bezier p1 point: %s\n", token);
          continue;
        }

        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_str_has_prefix(token, "p2=") || g_str_has_prefix(token, "bezier_p2=") ||
                                            g_str_has_prefix(token, "p2:") || g_str_has_prefix(token, "bezier_p2:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            double fx, fy;
            if (parse_float_point(value + 1, &fx, &fy)) {
              bezier_p2_u = fx;
              bezier_p2_v = fy;
              continue;
            }
          }
          g_print("Failed to parse bezier p2 point: %s\n", token);
          continue;
        }

        if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && (g_str_has_prefix(token, "p3=") || g_str_has_prefix(token, "bezier_p3=") ||
                                            g_str_has_prefix(token, "p3:") || g_str_has_prefix(token, "bezier_p3:"))) {
          const gchar *value = strchr(token, '=');
          if (!value) value = strchr(token, ':');
          if (value && *(value + 1) != '\0') {
            double fx, fy;
            if (parse_float_point(value + 1, &fx, &fy)) {
              bezier_p3_u = fx;
              bezier_p3_v = fy;
              continue;
            }
          }
          g_print("Failed to parse bezier p3 point: %s\n", token);
          continue;
        }

        g_print("Warning: Unrecognized token in shape_create: %s\n", token);
      }

      if (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) {
        filled = FALSE;
      }

      ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
      ElementColor bg_color = { .r = bg_r, .g = bg_g, .b = bg_b, .a = bg_a };
      ElementColor text_color = { .r = text_r, .g = text_g, .b = text_b, .a = text_a };
      ElementSize size = { .width = width, .height = height };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };
      ElementConnection connection = {
        .from_element_uuid = NULL,
        .to_element_uuid = NULL,
        .from_point = -1,
        .to_point = -1,
      };
      ElementDrawing drawing = {
        .drawing_points = NULL,
        .stroke_width = stroke_width,
      };

      GArray *line_points = NULL;
      GArray *bezier_points = NULL;
      if (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) {
        if (!line_start_defined) {
          line_start_u = 0.0;
          line_start_v = 0.0;
        }
        if (!line_end_defined) {
          line_end_u = 1.0;
          line_end_v = 1.0;
        }

        line_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 2);

        DrawingPoint start_point;
        graphene_point_init(&start_point, (float)line_start_u, (float)line_start_v);
        g_array_append_val(line_points, start_point);

        DrawingPoint end_point;
        graphene_point_init(&end_point, (float)line_end_u, (float)line_end_v);
        g_array_append_val(line_points, end_point);

        drawing.drawing_points = line_points;
      } else if (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) {
        bezier_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 4);

        DrawingPoint p0, p1, p2, p3;
        graphene_point_init(&p0, (float)bezier_p0_u, (float)bezier_p0_v);
        graphene_point_init(&p1, (float)bezier_p1_u, (float)bezier_p1_v);
        graphene_point_init(&p2, (float)bezier_p2_u, (float)bezier_p2_v);
        graphene_point_init(&p3, (float)bezier_p3_u, (float)bezier_p3_v);

        g_array_append_val(bezier_points, p0);
        g_array_append_val(bezier_points, p1);
        g_array_append_val(bezier_points, p2);
        g_array_append_val(bezier_points, p3);

        drawing.drawing_points = bezier_points;
      }

      const gchar *font_to_use = font_override ? font_override : default_font;
      ElementText text_elem = {
        .text = clean_text,
        .text_color = text_color,
        .font_description = g_strdup(font_to_use),
        .alignment = alignment,
      };
      ElementShape shape_elem = {
        .shape_type = shape_type,
        .stroke_width = stroke_width,
        .filled = filled,
        .stroke_style = stroke_style_value,
        .fill_style = fill_style_value,
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
        if (rotation_set && rotation_degrees != 0.0) {
          model_element->rotation_degrees = rotation_degrees;
        }
        if (locked_set) {
          model_element->locked = locked;
        }
        g_hash_table_insert(element_map, g_strdup(id), model_element);
        dsl_runtime_register_element(data, id, model_element);
        new_elements = g_list_prepend(new_elements, model_element);
      }

      if (line_points) {
        g_array_free(line_points, TRUE);
      }
      if (bezier_points) {
        g_array_free(bezier_points, TRUE);
      }

      g_free(text_elem.font_description);
      g_free(clean_text);
      g_free(font_override);
      // Note: alignment is owned by text_elem, don't free here
    }
    else if (g_strcmp0(tokens[0], "connect") == 0 && token_count >= 3) {
      // connect FROM_ID TO_ID [TYPE] [ARROWHEAD] [color(...)|#RRGGBB[AA]]
      // Store for processing after all notes are created
      ConnectionInfo *info = g_new0(ConnectionInfo, 1);
      info->from_id = g_strdup(tokens[1]);
      info->to_id = g_strdup(tokens[2]);

      // Defaults
      info->connection_type = CONNECTION_TYPE_PARALLEL;
      info->arrowhead_type = ARROWHEAD_SINGLE;
      info->has_color = FALSE;
      info->r = 1.0;
      info->g = 1.0;
      info->b = 1.0;
      info->a = 1.0;

      gboolean type_set = FALSE;
      gboolean arrowhead_set = FALSE;

      for (int t = 3; t < token_count; t++) {
        const gchar *token = tokens[t];

        if (!info->has_color) {
          double r, g, b, a;

          if (g_strcmp0(token, "color") == 0 && (t + 1) < token_count) {
            if (parse_color_token(tokens[t + 1], &r, &g, &b, &a)) {
              info->has_color = TRUE;
              info->r = r;
              info->g = g;
              info->b = b;
              info->a = a;
              t++; // Skip the value token we just consumed
              continue;
            }
          }

          if (parse_color_token(token, &r, &g, &b, &a)) {
            info->has_color = TRUE;
            info->r = r;
            info->g = g;
            info->b = b;
            info->a = a;
            continue;
          }
        }

        if (!type_set) {
          if (g_strcmp0(token, "straight") == 0) {
            info->connection_type = CONNECTION_TYPE_STRAIGHT;
            type_set = TRUE;
            continue;
          } else if (g_strcmp0(token, "parallel") == 0) {
            info->connection_type = CONNECTION_TYPE_PARALLEL;
            type_set = TRUE;
            continue;
          }
        }

        if (!arrowhead_set) {
          if (g_strcmp0(token, "none") == 0) {
            info->arrowhead_type = ARROWHEAD_NONE;
            arrowhead_set = TRUE;
            continue;
          } else if (g_strcmp0(token, "single") == 0) {
            info->arrowhead_type = ARROWHEAD_SINGLE;
            arrowhead_set = TRUE;
            continue;
          } else if (g_strcmp0(token, "double") == 0) {
            info->arrowhead_type = ARROWHEAD_DOUBLE;
            arrowhead_set = TRUE;
            continue;
          }
        }

        g_print("Warning: Unrecognized token in connect command: %s\n", token);
      }

      connections = g_list_append(connections, info);
    }
    // Animation commands
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_move") == 0 && token_count >= 6) {
      // animate_move ELEMENT_ID (from_x,from_y) (to_x,to_y) START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      int from_x, from_y, to_x, to_y;
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      if (!parse_point(tokens[2], &from_x, &from_y)) {
        g_print("Error: Failed to parse from position: %s\n", tokens[2]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_point(tokens[3], &to_x, &to_y)) {
        g_print("Error: Failed to parse to position: %s\n", tokens[3]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[4], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[4]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[5], &duration)) {
        g_print("Error: Failed to parse duration: %s\n", tokens[5]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      // Parse interpolation type if provided
      if (token_count >= 7) {
        if (g_strcmp0(tokens[6], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[6], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[6], "bezier") == 0 || g_strcmp0(tokens[6], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        } else if (g_strcmp0(tokens[6], "ease-in") == 0 || g_strcmp0(tokens[6], "easein") == 0) {
          interp = ANIM_INTERP_EASE_IN;
        } else if (g_strcmp0(tokens[6], "ease-out") == 0 || g_strcmp0(tokens[6], "easeout") == 0) {
          interp = ANIM_INTERP_EASE_OUT;
        } else if (g_strcmp0(tokens[6], "bounce") == 0) {
          interp = ANIM_INTERP_BOUNCE;
        } else if (g_strcmp0(tokens[6], "elastic") == 0) {
          interp = ANIM_INTERP_ELASTIC;
        } else if (g_strcmp0(tokens[6], "back") == 0) {
          interp = ANIM_INTERP_BACK;
        }
      }

      // Look up element by ID and get UUID
      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_move(data->anim_engine, elem->uuid,
                          start_time, duration, interp,
                          from_x, from_y, to_x, to_y);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_resize") == 0 && token_count >= 6) {
      // animate_resize ELEMENT_ID (from_w,from_h) (to_w,to_h) START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      int from_w, from_h, to_w, to_h;
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      if (!parse_point(tokens[2], &from_w, &from_h)) {
        g_print("Error: Failed to parse from size: %s\n", tokens[2]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_point(tokens[3], &to_w, &to_h)) {
        g_print("Error: Failed to parse to size: %s\n", tokens[3]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[4], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[4]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[5], &duration)) {
        g_print("Error: Failed to parse duration: %s\n", tokens[5]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count >= 7) {
        if (g_strcmp0(tokens[6], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[6], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[6], "bezier") == 0 || g_strcmp0(tokens[6], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        } else if (g_strcmp0(tokens[6], "ease-in") == 0 || g_strcmp0(tokens[6], "easein") == 0) {
          interp = ANIM_INTERP_EASE_IN;
        } else if (g_strcmp0(tokens[6], "ease-out") == 0 || g_strcmp0(tokens[6], "easeout") == 0) {
          interp = ANIM_INTERP_EASE_OUT;
        } else if (g_strcmp0(tokens[6], "bounce") == 0) {
          interp = ANIM_INTERP_BOUNCE;
        } else if (g_strcmp0(tokens[6], "elastic") == 0) {
          interp = ANIM_INTERP_ELASTIC;
        } else if (g_strcmp0(tokens[6], "back") == 0) {
          interp = ANIM_INTERP_BACK;
        }
      }

      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_resize(data->anim_engine, elem->uuid,
                            start_time, duration, interp,
                            from_w, from_h, to_w, to_h);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_rotate") == 0 && token_count >= 4) {
      // animate_rotate ELEMENT_ID TO_DEGREES START_TIME DURATION [TYPE]
      // animate_rotate ELEMENT_ID FROM_DEGREES TO_DEGREES START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      double from_rotation = 0.0, to_rotation = 0.0;
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      // Count numeric parameters to determine syntax
      int numeric_count = 0;
      int temp_idx = 2;
      while (temp_idx < token_count && tokens[temp_idx][0] != '(') {
        double dummy;
        if (parse_double_value(tokens[temp_idx], &dummy)) {
          numeric_count++;
          temp_idx++;
        } else {
          break;
        }
      }

      int cursor = 2;
      if (numeric_count >= 4) {
        // Two rotation values provided
        if (!parse_double_value(tokens[2], &from_rotation) ||
            !parse_double_value(tokens[3], &to_rotation)) {
          g_print("Error: Failed to parse rotation degrees\n");
          for (int j = 0; j < token_count; j++) g_free(tokens[j]);
          g_free(tokens);
          continue;
        }
        cursor = 4;
      } else {
        // Single rotation value - from current rotation
        ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
        if (elem && elem->visual_element) {
          from_rotation = elem->visual_element->rotation_degrees;
        } else {
          from_rotation = 0.0;  // Default if element not found yet
        }
        if (!parse_double_value(tokens[2], &to_rotation)) {
          g_print("Error: Failed to parse rotation degrees\n");
          for (int j = 0; j < token_count; j++) g_free(tokens[j]);
          g_free(tokens);
          continue;
        }
        cursor = 3;
      }

      if (!parse_double_value(tokens[cursor], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[cursor]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[cursor + 1], &duration)) {
        g_print("Error: Failed to parse duration: %s\n", tokens[cursor + 1]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count >= cursor + 3) {
        if (g_strcmp0(tokens[cursor + 2], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[cursor + 2], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[cursor + 2], "bezier") == 0 || g_strcmp0(tokens[cursor + 2], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        }
      }

      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_rotate(data->anim_engine, elem->uuid,
                            start_time, duration, interp,
                            from_rotation, to_rotation);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_color") == 0 && token_count >= 5) {
      // animate_color ELEMENT_ID FROM_COLOR TO_COLOR START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      const gchar *from_color = tokens[2];
      const gchar *to_color = tokens[3];
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      if (!parse_double_value(tokens[4], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[4]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count < 6 || !parse_double_value(tokens[5], &duration)) {
        g_print("Error: Failed to parse duration\n");
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count >= 7) {
        if (g_strcmp0(tokens[6], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[6], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[6], "bezier") == 0 || g_strcmp0(tokens[6], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        } else if (g_strcmp0(tokens[6], "ease-in") == 0 || g_strcmp0(tokens[6], "easein") == 0) {
          interp = ANIM_INTERP_EASE_IN;
        } else if (g_strcmp0(tokens[6], "ease-out") == 0 || g_strcmp0(tokens[6], "easeout") == 0) {
          interp = ANIM_INTERP_EASE_OUT;
        } else if (g_strcmp0(tokens[6], "bounce") == 0) {
          interp = ANIM_INTERP_BOUNCE;
        } else if (g_strcmp0(tokens[6], "elastic") == 0) {
          interp = ANIM_INTERP_ELASTIC;
        } else if (g_strcmp0(tokens[6], "back") == 0) {
          interp = ANIM_INTERP_BACK;
        }
      }

      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_color(data->anim_engine, elem->uuid,
                           start_time, duration, interp,
                           from_color, to_color);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_appear") == 0 && token_count >= 4) {
      // animate_appear ELEMENT_ID START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      if (!parse_double_value(tokens[2], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[2]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[3], &duration)) {
        g_print("Error: Failed to parse duration: %s\n", tokens[3]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count >= 5) {
        if (g_strcmp0(tokens[4], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[4], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[4], "bezier") == 0 || g_strcmp0(tokens[4], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        }
      }

      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_create(data->anim_engine, elem->uuid,
                            start_time, duration, interp);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && g_strcmp0(tokens[0], "animate_disappear") == 0 && token_count >= 4) {
      // animate_disappear ELEMENT_ID START_TIME DURATION [TYPE]
      const gchar *elem_id = tokens[1];
      double start_time, duration;
      AnimInterpolationType interp = ANIM_INTERP_LINEAR;

      if (!parse_double_value(tokens[2], &start_time)) {
        g_print("Error: Failed to parse start_time: %s\n", tokens[2]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (!parse_double_value(tokens[3], &duration)) {
        g_print("Error: Failed to parse duration: %s\n", tokens[3]);
        for (int j = 0; j < token_count; j++) g_free(tokens[j]);
        g_free(tokens);
        continue;
      }

      if (token_count >= 5) {
        if (g_strcmp0(tokens[4], "immediate") == 0) {
          interp = ANIM_INTERP_IMMEDIATE;
        } else if (g_strcmp0(tokens[4], "linear") == 0) {
          interp = ANIM_INTERP_LINEAR;
        } else if (g_strcmp0(tokens[4], "bezier") == 0 || g_strcmp0(tokens[4], "curve") == 0) {
          interp = ANIM_INTERP_BEZIER;
        }
      }

      ModelElement *elem = g_hash_table_lookup(element_map, elem_id);
      if (elem && elem->uuid) {
        animation_add_delete(data->anim_engine, elem->uuid,
                            start_time, duration, interp);
      } else {
        g_print("Warning: Element %s not found for animation\n", elem_id);
      }
    }
    else if (is_animation_mode && (g_strcmp0(tokens[0], "animation_mode") == 0)) {
      // Just skip this line, already processed
    }

    for (int j = 0; j < token_count; j++)
      g_free(tokens[j]);
    g_free(tokens);
}

// Create visual elements for all collected elements (deferred rendering)
  int element_count = g_list_length(new_elements);
  g_print("DSL: Creating %d visual elements...\n", element_count);

  int elem_idx = 0;
  for (GList *l = new_elements; l != NULL; l = l->next) {
    ModelElement *element = (ModelElement *)l->data;
    if (element && !element->visual_element) {
      element->visual_element = create_visual_element(element, data);
    }
    elem_idx++;
    if (element_count > 10000 && elem_idx % 10000 == 0) {
      g_print("DSL: Created %d/%d visuals (%.1f%%)...\n", elem_idx, element_count, (elem_idx * 100.0) / element_count);
    }
  }

  // Push single batch undo action for all created elements
  if (new_elements) {
    g_print("DSL: Pushing batch undo for %d elements...\n", element_count);
    undo_manager_push_create_action_batch(data->undo_manager, new_elements);
  }

  // Process connections
  for (GList *l = connections; l != NULL; l = l->next) {
    ConnectionInfo *info = (ConnectionInfo *)l->data;

    ModelElement *from_model = g_hash_table_lookup(element_map, info->from_id);
    ModelElement *to_model = g_hash_table_lookup(element_map, info->to_id);

    if (from_model && to_model && from_model->visual_element && to_model->visual_element) {
      // Determine optimal connection points
      int from_point, to_point;
      determine_optimal_connection_points(from_model, to_model, &from_point, &to_point);

      ElementPosition position = {
        .x = 0, .y = 0,
        .z = MAX(from_model->position->z, to_model->position->z) - 1 // Place connection below notes
      };
      ElementColor bg_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 };
      if (info->has_color) {
        bg_color.r = info->r;
        bg_color.g = info->g;
        bg_color.b = info->b;
        bg_color.a = info->a;
      }
      ElementColor text_color = { .r = 0., .g = 0., .b = 0., .a = 0.0 };
      ElementSize size = { .width = 1, .height = 1 };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };
      ElementConnection connection = {
        .from_element = from_model->visual_element,
        .to_element = to_model->visual_element,
        .from_element_uuid = from_model->uuid,
        .to_element_uuid = to_model->uuid,
        .from_point = from_point,
        .to_point = to_point,
        .connection_type = info->connection_type,
        .arrowhead_type = info->arrowhead_type,
      };
      ElementDrawing drawing = {
        .drawing_points = NULL,
        .stroke_width = 0,
      };
      ElementText text = {
        .text = NULL,
        .text_color = text_color,
        .font_description = NULL,
      };
      ElementConfig config = {
        .type = ELEMENT_CONNECTION,
        .bg_color = bg_color,
        .position = position,
        .size = size,
        .media = media,
        .drawing = drawing,
        .connection = connection,
        .text = text,
      };

      ModelElement *model_conn = model_create_element(data->model, config);

      if (model_conn) {
        connection_elements = g_list_prepend(connection_elements, model_conn);
      }
    } else {
      g_print("Could not find elements for connection: %s -> %s\n", info->from_id, info->to_id);
    }

    g_free(info->from_id);
    g_free(info->to_id);
    g_free(info);
  }

  // Create visual elements for connections
  for (GList *l = connection_elements; l != NULL; l = l->next) {
    ModelElement *element = (ModelElement *)l->data;
    if (element && !element->visual_element) {
      element->visual_element = create_visual_element(element, data);
    }
  }

  // Push batch undo action for connections
  int connection_count = g_list_length(connection_elements);
  if (connection_elements) {
    undo_manager_push_create_action_batch(data->undo_manager, connection_elements);
  }

  // Clean up
  g_hash_table_destroy(element_map);
  g_list_free(connections);  // ConnectionInfo already freed in loop above
  g_list_free(new_elements);
  g_list_free(connection_elements);
  g_strfreev(lines);

  g_print("DSL: Complete! Total elements: %d + %d connections\n",
          element_count, connection_count);

  // Start animation if in animation mode
  if (is_animation_mode && data->anim_engine) {
    g_print("Starting animation...\n");
    animation_engine_start(data->anim_engine, data->drawing_area, data);
  }

  gtk_widget_queue_draw(data->drawing_area);
}

// Helper function to clear all elements in current space while preserving space settings
static void canvas_clear_space_for_presentation(CanvasData *data) {
  if (!data || !data->model) return;

  const gchar *current_space_uuid = data->model->current_space_uuid;
  if (!current_space_uuid) return;

  // Collect UUIDs of all elements in current space
  GList *all_elements = g_hash_table_get_values(data->model->elements);
  GList *uuids_to_remove = NULL;
  for (GList *l = all_elements; l != NULL; l = l->next) {
    ModelElement *element = (ModelElement *)l->data;
    if (element && element->space_uuid &&
        g_strcmp0(element->space_uuid, current_space_uuid) == 0 &&
        element->uuid) {
      uuids_to_remove = g_list_prepend(uuids_to_remove, g_strdup(element->uuid));
    }
  }
  g_list_free(all_elements);

  for (GList *l = uuids_to_remove; l != NULL; l = l->next) {
    gchar *uuid = (gchar *)l->data;
    ModelElement *element = g_hash_table_lookup(data->model->elements, uuid);
    if (element) {
      model_delete_element(data->model, element);
      g_hash_table_remove(data->model->elements, uuid);
    }
    g_free(uuid);
  }
  g_list_free(uuids_to_remove);

  // Clear visual selection
  canvas_clear_selection(data);
  canvas_sync_with_model(data);
}

// Check if we're in presentation mode
gboolean canvas_is_presentation_mode(CanvasData *data) {
  return data && data->presentation_mode_active;
}

gboolean dsl_handle_element_click(CanvasData *data, Element *element) {
  if (!data || !element) return FALSE;

  ModelElement *model_element = model_get_by_visual(data->model, element);
  if (!model_element) return FALSE;

  const gchar *element_id = dsl_runtime_lookup_element_id(data, model_element);
  if (!element_id) return FALSE;

  return dsl_runtime_handle_click(data, element_id);
}

// Navigate to next slide in presentation mode
void canvas_presentation_next_slide(CanvasData *data) {
  if (!data || !data->presentation_mode_active) return;
  if (data->presentation_current_slide >= data->presentation_slide_count - 1) {
    extern void canvas_show_notification(CanvasData *data, const char *message);
    canvas_show_notification(data, "Already at last slide");
    return;
  }

  data->presentation_current_slide++;
  g_print("Moving to slide %d/%d\n", data->presentation_current_slide + 1, data->presentation_slide_count);

  // Stop any running animation from previous slide
  if (data->anim_engine) {
    animation_engine_stop(data->anim_engine);
  }

  // Reset DSL runtime before clearing space to remove dangling element references
  dsl_runtime_reset(data);

  // Clear current space (preserving grid settings)
  canvas_clear_space_for_presentation(data);

  // Suppress auto-next during slide load to prevent immediate re-triggering
  data->presentation_suppress_auto_next = TRUE;

  // Execute the new slide's DSL
  if (data->presentation_dsl_slides[data->presentation_current_slide]) {
    canvas_execute_script(data, data->presentation_dsl_slides[data->presentation_current_slide]);
  }

  // Re-enable auto-next after slide is fully loaded
  data->presentation_suppress_auto_next = FALSE;
}

// Navigate to previous slide in presentation mode
void canvas_presentation_prev_slide(CanvasData *data) {
  if (!data || !data->presentation_mode_active) return;
  if (data->presentation_current_slide <= 0) {
    extern void canvas_show_notification(CanvasData *data, const char *message);
    canvas_show_notification(data, "Already at first slide");
    return;
  }

  data->presentation_current_slide--;
  g_print("Moving to slide %d/%d\n", data->presentation_current_slide + 1, data->presentation_slide_count);

  // Stop any running animation from previous slide
  if (data->anim_engine) {
    animation_engine_stop(data->anim_engine);
  }

  // Reset DSL runtime before clearing space to remove dangling element references
  dsl_runtime_reset(data);

  // Clear current space (preserving grid settings)
  canvas_clear_space_for_presentation(data);

  // Suppress auto-next during slide load to prevent immediate re-triggering
  data->presentation_suppress_auto_next = TRUE;

  // Execute the new slide's DSL
  if (data->presentation_dsl_slides[data->presentation_current_slide]) {
    canvas_execute_script(data, data->presentation_dsl_slides[data->presentation_current_slide]);
  }

  // Re-enable auto-next after slide is fully loaded
  data->presentation_suppress_auto_next = FALSE;
}

static gchar* escape_text_for_dsl(const gchar *text) {
  if (!text) return g_strdup("\"\"");

  GString *escaped = g_string_new(NULL);
  g_string_append_c(escaped, '"');

  for (const gchar *p = text; *p; p++) {
    switch (*p) {
    case '\n':
      g_string_append(escaped, "\\n");
      break;
    case '\r':
      g_string_append(escaped, "\\r");
      break;
    case '\t':
      g_string_append(escaped, "\\t");
      break;
    case '"':
      g_string_append(escaped, "\\\"");
      break;
    case '\\':
      g_string_append(escaped, "\\\\");
      break;
    default:
      g_string_append_c(escaped, *p);
      break;
    }
  }

  g_string_append_c(escaped, '"');
  return g_string_free(escaped, FALSE);
}

// Main DSL generation function
gchar* canvas_generate_dsl_from_model(CanvasData *data) {
  if (!data || !data->model) {
    return g_strdup("");
  }

  GString *dsl = g_string_new(NULL);
  GHashTable *element_id_map = g_hash_table_new(g_str_hash, g_str_equal);
  int element_counter = 1;

  // First pass: create notes and paper notes
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, data->model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement*)value;

    // Skip deleted elements and connections for now
    if (element->state == MODEL_STATE_DELETED ||
        element->type->type == ELEMENT_CONNECTION ||
        g_strcmp0(element->space_uuid, data->model->current_space_uuid) != 0) {
      continue;
    }

    // Generate a unique ID for the element
    gchar *element_id = NULL;
    if (element->text && element->text->text && strlen(element->text->text) > 0) {
      // Create ID from first few words of text
      gchar *clean_text = g_ascii_strdown(element->text->text, -1);

      // Replace non-alphanumeric characters with underscores
      for (char *p = clean_text; *p; p++) {
        if (!g_ascii_isalnum(*p)) {
          *p = '_';
        }
      }

      // Remove consecutive underscores
      gchar *dest = clean_text;
      gchar *src = clean_text;
      gboolean last_was_underscore = FALSE;

      while (*src) {
        if (*src == '_') {
          if (!last_was_underscore) {
            *dest++ = '_';
            last_was_underscore = TRUE;
          }
        } else {
          *dest++ = *src;
          last_was_underscore = FALSE;
        }
        src++;
      }
      *dest = '\0';

      // Trim leading/trailing underscores
      g_strstrip(clean_text);

      // If we have a reasonable ID, use it
      if (strlen(clean_text) > 0 && strlen(clean_text) < 30) {
        element_id = g_strdup(clean_text);
      } else {
        // Fallback to generic ID
        element_id = g_strdup_printf("elem_%d", element_counter++);
      }

      g_free(clean_text);
    } else {
      // No text content, use generic ID
      element_id = g_strdup_printf("elem_%d", element_counter++);
    }

    // Ensure ID is unique
    if (g_hash_table_contains(element_id_map, element_id)) {
      gchar *unique_id = g_strdup_printf("%s_%d", element_id, element_counter++);
      g_free(element_id);
      element_id = unique_id;
    }

    // Store the mapping from UUID to generated ID
    g_hash_table_insert(element_id_map, g_strdup(element->uuid), g_strdup(element_id));

    // Generate DSL line based on element type
    if (element->type->type == ELEMENT_NOTE || element->type->type == ELEMENT_PAPER_NOTE) {
      const gchar *command = (element->type->type == ELEMENT_PAPER_NOTE) ?
        "paper_note_create" : "note_create";

      gchar *text_escaped = escape_text_for_dsl(element->text ? element->text->text : "");
      gchar *pos_str = g_strdup_printf("(%d,%d)", element->position->x, element->position->y);
      gchar *size_str = g_strdup_printf("(%d,%d)", element->size->width, element->size->height);
      double bg_r = element->bg_color ? element->bg_color->r : 1.0;
      double bg_g = element->bg_color ? element->bg_color->g : 1.0;
      double bg_b = element->bg_color ? element->bg_color->b : 1.0;
      double bg_a = element->bg_color ? element->bg_color->a : 1.0;

      double text_r = (element->text) ? element->text->r : 0.2;
      double text_g = (element->text) ? element->text->g : 0.2;
      double text_b = (element->text) ? element->text->b : 0.2;
      double text_a = (element->text) ? element->text->a : 1.0;

      const gchar *default_font = (element->type->type == ELEMENT_PAPER_NOTE) ?
                                  PAPER_NOTE_DEFAULT_FONT : "Ubuntu 16";
      gchar *font_str = escape_text_for_dsl(element->text && element->text->font_description ?
                                            element->text->font_description : default_font);

      g_string_append_printf(dsl,
                             "%s %s %s %s %s bg color(%.2f,%.2f,%.2f,%.2f) font %s text_color color(%.2f,%.2f,%.2f,%.2f)",
                             command,
                             element_id,
                             text_escaped,
                             pos_str,
                             size_str,
                             bg_r, bg_g, bg_b, bg_a,
                             font_str,
                             text_r, text_g, text_b, text_a);

      if (element->rotation_degrees != 0.0) {
        g_string_append_printf(dsl, " rotation %.1f", element->rotation_degrees);
      }
      g_string_append_c(dsl, '\n');

      g_free(text_escaped);
      g_free(pos_str);
      g_free(size_str);
      g_free(font_str);
    }
    else if (element->type->type == ELEMENT_SPACE) {
      gchar *text_escaped = escape_text_for_dsl(element->text ? element->text->text : "");
      gchar *pos_str = g_strdup_printf("(%d,%d)", element->position->x, element->position->y);
      gchar *size_str = g_strdup_printf("(%d,%d)", element->size->width, element->size->height);

      g_string_append_printf(dsl, "space_create %s %s %s %s",
                             element_id, text_escaped, pos_str, size_str);

      if (element->rotation_degrees != 0.0) {
        g_string_append_printf(dsl, " rotation %.1f", element->rotation_degrees);
      }
      g_string_append_c(dsl, '\n');

      g_free(text_escaped);
      g_free(pos_str);
      g_free(size_str);
    }
    else if (element->type->type == ELEMENT_SHAPE) {
      const gchar *shape_type_str = "circle";
      switch (element->shape_type) {
        case SHAPE_CIRCLE: shape_type_str = "circle"; break;
        case SHAPE_RECTANGLE: shape_type_str = "rectangle"; break;
        case SHAPE_TRIANGLE: shape_type_str = "triangle"; break;
        case SHAPE_DIAMOND: shape_type_str = "diamond"; break;
        case SHAPE_CYLINDER_VERTICAL: shape_type_str = "vcylinder"; break;
        case SHAPE_CYLINDER_HORIZONTAL: shape_type_str = "hcylinder"; break;
        case SHAPE_ROUNDED_RECTANGLE: shape_type_str = "rounded_rectangle"; break;
        case SHAPE_TRAPEZOID: shape_type_str = "trapezoid"; break;
        case SHAPE_LINE: shape_type_str = "line"; break;
        case SHAPE_ARROW: shape_type_str = "arrow"; break;
        case SHAPE_BEZIER: shape_type_str = "bezier"; break;
        case SHAPE_CURVED_ARROW: shape_type_str = "curved_arrow"; break;
        case SHAPE_CUBE: shape_type_str = "cube"; break;
      }

      gchar *text_escaped = escape_text_for_dsl(element->text ? element->text->text : "");
      gchar *pos_str = g_strdup_printf("(%d,%d)", element->position->x, element->position->y);
      gchar *size_str = g_strdup_printf("(%d,%d)", element->size->width, element->size->height);
      double bg_r = element->bg_color ? element->bg_color->r : 0.95;
      double bg_g = element->bg_color ? element->bg_color->g : 0.95;
      double bg_b = element->bg_color ? element->bg_color->b : 0.98;
      double bg_a = element->bg_color ? element->bg_color->a : 1.0;

      double text_r = (element->text) ? element->text->r : 0.1;
      double text_g = (element->text) ? element->text->g : 0.1;
      double text_b = (element->text) ? element->text->b : 0.1;
      double text_a = (element->text) ? element->text->a : 1.0;

      gchar *font_str = escape_text_for_dsl(element->text && element->text->font_description ?
                                            element->text->font_description : "Ubuntu Bold 14");

      // Get stroke style name
      const gchar *stroke_style_str = "solid";
      switch (element->stroke_style) {
        case STROKE_STYLE_SOLID: stroke_style_str = "solid"; break;
        case STROKE_STYLE_DASHED: stroke_style_str = "dashed"; break;
        case STROKE_STYLE_DOTTED: stroke_style_str = "dotted"; break;
      }

      // Get fill style name
      const gchar *fill_style_str = "solid";
      switch (element->fill_style) {
        case FILL_STYLE_SOLID: fill_style_str = "solid"; break;
        case FILL_STYLE_HACHURE: fill_style_str = "hachure"; break;
        case FILL_STYLE_CROSS_HATCH: fill_style_str = "crosshatch"; break;
      }

      // Build the shape_create command
      g_string_append_printf(dsl,
                             "shape_create %s %s %s %s %s bg color(%.2f,%.2f,%.2f,%.2f) stroke %d",
                             element_id,
                             shape_type_str,
                             text_escaped,
                             pos_str,
                             size_str,
                             bg_r, bg_g, bg_b, bg_a,
                             element->stroke_width);

      // Add stroke_color if available
      if (element->stroke_color) {
        g_string_append_printf(dsl, " stroke_color %s", element->stroke_color);
      }

      // Add stroke_style if not solid
      if (element->stroke_style != STROKE_STYLE_SOLID) {
        g_string_append_printf(dsl, " stroke_style %s", stroke_style_str);
      }

      // Add filled flag
      g_string_append_printf(dsl, " filled %s", element->filled ? "true" : "false");

      // Add fill_style if filled and not solid
      if (element->filled && element->fill_style != FILL_STYLE_SOLID) {
        g_string_append_printf(dsl, " fill_style %s", fill_style_str);
      }

      // Add font and text_color
      g_string_append_printf(dsl, " font %s text_color color(%.2f,%.2f,%.2f,%.2f)",
                             font_str,
                             text_r, text_g, text_b, text_a);

      // Add rotation if non-zero
      if (element->rotation_degrees != 0.0) {
        g_string_append_printf(dsl, " rotation %.1f", element->rotation_degrees);
      }
      g_string_append_c(dsl, '\n');

      g_free(text_escaped);
      g_free(pos_str);
      g_free(size_str);
      g_free(font_str);
    }
    else if (element->type->type == ELEMENT_INLINE_TEXT) {
      gchar *text_escaped = escape_text_for_dsl(element->text ? element->text->text : "");
      gchar *pos_str = g_strdup_printf("(%d,%d)", element->position->x, element->position->y);
      gchar *size_str = g_strdup_printf("(%d,%d)", element->size->width, element->size->height);

      double bg_r = element->bg_color ? element->bg_color->r : 0.0;
      double bg_g = element->bg_color ? element->bg_color->g : 0.0;
      double bg_b = element->bg_color ? element->bg_color->b : 0.0;
      double bg_a = element->bg_color ? element->bg_color->a : 0.0;

      double text_r = (element->text) ? element->text->r : 0.6;
      double text_g = (element->text) ? element->text->g : 0.6;
      double text_b = (element->text) ? element->text->b : 0.6;
      double text_a = (element->text) ? element->text->a : 1.0;

      gchar *font_str = escape_text_for_dsl(element->text && element->text->font_description ?
                                            element->text->font_description : "Ubuntu Mono 14");

      g_string_append_printf(dsl,
                             "text_create %s %s %s %s bg color(%.2f,%.2f,%.2f,%.2f) font %s text_color color(%.2f,%.2f,%.2f,%.2f)",
                             element_id,
                             text_escaped,
                             pos_str,
                             size_str,
                             bg_r, bg_g, bg_b, bg_a,
                             font_str,
                             text_r, text_g, text_b, text_a);

      if (element->rotation_degrees != 0.0) {
        g_string_append_printf(dsl, " rotation %.1f", element->rotation_degrees);
      }
      g_string_append_c(dsl, '\n');

      g_free(text_escaped);
      g_free(pos_str);
      g_free(size_str);
      g_free(font_str);
    }
    else if (element->type->type == ELEMENT_MEDIA_FILE) {
      gboolean is_video = (element->video && element->video->duration > 0);
      const gchar *command = is_video ? "video_create" : "image_create";

      gchar *pos_str = g_strdup_printf("(%d,%d)", element->position->x, element->position->y);
      gchar *size_str = g_strdup_printf("(%d,%d)", element->size->width, element->size->height);

      const gchar *placeholder_path = is_video ? "REPLACE_WITH_VIDEO_PATH.mp4" : "REPLACE_WITH_IMAGE_PATH.png";

      const gchar *label_source = (element->text && element->text->text && element->text->text[0] != '\0') ?
        element->text->text : element_id;
      gchar *label_copy = g_strdup(label_source);
      for (gchar *p = label_copy; p && *p; p++) {
        if (*p == '\n' || *p == '\r' || *p == '\t') {
          *p = ' ';
        }
      }

      g_string_append_printf(dsl,
                             "# TODO: Update %s file path for %s (%s) before executing\n",
                             is_video ? "video" : "image",
                             element_id,
                             label_copy ? label_copy : ""
                             );

      if (is_video && element->video) {
        g_string_append_printf(dsl,
                               "# Hint: original runtime %d seconds\n",
                               element->video->duration);
      }

      g_string_append_printf(dsl,
                             "%s %s %s %s %s",
                             command,
                             element_id,
                             placeholder_path,
                             pos_str,
                             size_str);

      if (element->rotation_degrees != 0.0) {
        g_string_append_printf(dsl, " rotation %.1f", element->rotation_degrees);
      }
      g_string_append_c(dsl, '\n');

      g_free(pos_str);
      g_free(size_str);
      g_free(label_copy);
    }
    // Media elements are exported with placeholder paths; users must update paths before re-importing
  }

  // Second pass: create connections
  g_hash_table_iter_init(&iter, data->model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = (ModelElement*)value;

    // Only process connections in current space
    if (element->state == MODEL_STATE_DELETED ||
        element->type->type != ELEMENT_CONNECTION ||
        g_strcmp0(element->space_uuid, data->model->current_space_uuid) != 0) {
      continue;
    }

    if (element->from_element_uuid && element->to_element_uuid) {
      gchar *from_id = g_hash_table_lookup(element_id_map, element->from_element_uuid);
      gchar *to_id = g_hash_table_lookup(element_id_map, element->to_element_uuid);

      if (from_id && to_id) {
        // Convert connection type enum to string
        const gchar *conn_type_str = (element->connection_type == CONNECTION_TYPE_STRAIGHT) ?
                                     "straight" : "parallel";

        // Convert arrowhead type enum to string
        const gchar *arrowhead_str = "single";
        if (element->arrowhead_type == ARROWHEAD_NONE) {
          arrowhead_str = "none";
        } else if (element->arrowhead_type == ARROWHEAD_DOUBLE) {
          arrowhead_str = "double";
        }

        gboolean include_color = FALSE;
        gchar *color_str = NULL;
        if (element->bg_color) {
          double r = element->bg_color->r;
          double g = element->bg_color->g;
          double b = element->bg_color->b;
          double a = element->bg_color->a;

          if (fabs(r - 1.0) > 1e-6 || fabs(g - 1.0) > 1e-6 ||
              fabs(b - 1.0) > 1e-6 || fabs(a - 1.0) > 1e-6) {
            include_color = TRUE;
            color_str = g_strdup_printf("color(%.2f,%.2f,%.2f,%.2f)", r, g, b, a);
          }
        }

        if (include_color && color_str) {
          g_string_append_printf(dsl, "connect %s %s %s %s %s\n",
                                 from_id, to_id, conn_type_str, arrowhead_str, color_str);
          g_free(color_str);
        } else {
          g_string_append_printf(dsl, "connect %s %s %s %s\n",
                                 from_id, to_id, conn_type_str, arrowhead_str);
        }
      } else {
        g_print("Warning: Could not find IDs for connection from %s to %s\n",
                element->from_element_uuid, element->to_element_uuid);
      }
    }
  }

  // Clean up
  GHashTableIter id_iter;
  gpointer id_key, id_value;
  g_hash_table_iter_init(&id_iter, element_id_map);
  while (g_hash_table_iter_next(&id_iter, &id_key, &id_value)) {
    g_free(id_key);    // Free the UUID key
    g_free(id_value);  // Free the generated ID value
  }
  g_hash_table_destroy(element_id_map);

  return g_string_free(dsl, FALSE);
}

// Update the dialog response handler to include the export button
static void on_script_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = user_data;
    GtkWidget *text_view = g_object_get_data(G_OBJECT(dialog), "text_view");

    if (!text_view) {
      gtk_window_destroy(GTK_WINDOW(dialog));
      return;
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *script = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    canvas_execute_script(data, script);
    g_free(script);
  } else if (response_id == 100) { // Custom response for export
    CanvasData *data = user_data;
    GtkWidget *text_view = g_object_get_data(G_OBJECT(dialog), "text_view");

    if (!text_view) {
      gtk_window_destroy(GTK_WINDOW(dialog));
      return;
    }

    gchar *dsl = canvas_generate_dsl_from_model(data);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, dsl, -1);

    g_free(dsl);
    return; // Don't destroy the dialog
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

// Update the dialog creation to include export button
void canvas_show_script_dialog(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
                                                  "DSL Executor",
                                                  GTK_WINDOW(gtk_widget_get_root(data->drawing_area)),
                                                  GTK_DIALOG_MODAL,
                                                  "Execute", GTK_RESPONSE_OK,
                                                  "Export to DSL", 100, // Custom response code
                                                  "Cancel", GTK_RESPONSE_CANCEL,
                                                  NULL
                                                  );

  // Get content area
  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Create a box to hold everything
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_hexpand(main_box, TRUE);
  gtk_widget_set_vexpand(main_box, TRUE);

  // Create a scrolled window
  GtkWidget *scrolled_window = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(scrolled_window, TRUE);
  gtk_widget_set_vexpand(scrolled_window, TRUE);

  // Create text view
  GtkWidget *text_view = gtk_text_view_new();
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);

  // Store the text view in dialog data
  g_object_set_data(G_OBJECT(dialog), "text_view", text_view);

  // Add text view to scrolled window
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);

  // Add scrolled window to main box
  gtk_box_append(GTK_BOX(main_box), scrolled_window);

  // Add main box to content area
  gtk_box_append(GTK_BOX(content_area), main_box);

  // Set dialog size
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 600);

  g_signal_connect(dialog, "response", G_CALLBACK(on_script_dialog_response), data);
  gtk_widget_show(dialog);
}

// Public wrapper function without filename (for interactive use)
void canvas_execute_script(CanvasData *data, const gchar *script) {
  canvas_execute_script_internal(data, script, NULL);
}

// Public wrapper function with filename (for file-based DSL execution)
void canvas_execute_script_file(CanvasData *data, const gchar *script, const gchar *filename) {
  canvas_execute_script_internal(data, script, filename);
}
