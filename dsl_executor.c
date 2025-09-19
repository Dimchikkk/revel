#include <gtk/gtk.h>
#include "canvas.h"
#include <ctype.h>
#include <math.h>
#include "canvas_core.h"
#include "undo_manager.h"
#include "dsl_executor.h"

static gchar* unescape_text(const gchar *str) {
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

// Helper function to determine optimal connection points based on relative positions
static void determine_optimal_connection_points(ModelElement *from, ModelElement *to,
                                                int *from_point, int *to_point) {
  // Get center positions of both elements
  double from_center_x = from->position->x + from->size->width / 2.0;
  double from_center_y = from->position->y + from->size->height / 2.0;
  double to_center_x = to->position->x + to->size->width / 2.0;
  double to_center_y = to->position->y + to->size->height / 2.0;

  // Calculate angle between centers
  double dx = to_center_x - from_center_x;
  double dy = to_center_y - from_center_y;
  double angle = atan2(dy, dx);

  // Convert angle to degrees and normalize to 0-360 range
  angle = angle * 180.0 / M_PI;
  if (angle < 0) angle += 360;

  // Determine optimal connection points based on angle
  if (angle >= 45 && angle < 135) {
    // To element is below from element
    *from_point = 2; // Bottom of from element
    *to_point = 0;   // Top of to element
  } else if (angle >= 135 && angle < 225) {
    // To element is to the left of from element
    *from_point = 3; // Left of from element
    *to_point = 1;   // Right of to element
  } else if (angle >= 225 && angle < 315) {
    // To element is above from element
    *from_point = 0; // Top of from element
    *to_point = 2;   // Bottom of to element
  } else {
    // To element is to the right of from element (or very close)
    *from_point = 1; // Right of from element
    *to_point = 3;   // Left of to element
  }
}

// Helper function to trim whitespace from a string
static gchar* trim_whitespace(gchar *str) {
  while (isspace(*str)) str++;
  if (*str == 0) return str;

  gchar *end = str + strlen(str) - 1;
  while (end > str && isspace(*end)) end--;
  *(end + 1) = 0;

  return str;
}

// Helper function to parse a point string like "(50, 50)"
// Helper function to parse a point string like "(50, 50)"
static gboolean parse_point(const gchar *str, int *x, int *y) {
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

// Helper function to parse a color string like "(.1,.1,.1,.1)"
static gboolean parse_color(const gchar *str, double *r, double *g, double *b, double *a) {
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

// Custom tokenizer that handles quotes and parentheses
static gchar** tokenize_line(const gchar *line, int *token_count) {
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
    } else {
      // Regular token
      const gchar *start = p;
      while (*p && !isspace(*p)) p++;
      gchar *token = g_strndup(start, p - start);
      g_array_append_val(tokens, token);
    }
  }

  *token_count = tokens->len;
  return (gchar**)g_array_free(tokens, FALSE);
}

void canvas_execute_script(CanvasData *data, const gchar *script) {
  if (!data || !script) {
    g_print("Error: No data or script provided\n");
    return;
  }

  GHashTable *element_map = g_hash_table_new(g_str_hash, g_str_equal);
  GList *connections = NULL;

  gchar **lines = g_strsplit(script, "\n", 0);
  for (int i = 0; lines[i] != NULL; i++) {
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

    // Common note creation function
    if ((g_strcmp0(tokens[0], "note_create") == 0 ||
         g_strcmp0(tokens[0], "paper_note_create") == 0) && token_count >= 6) {

      ElementType element_type = (g_strcmp0(tokens[0], "paper_note_create") == 0) ?
        ELEMENT_PAPER_NOTE : ELEMENT_NOTE;

      const gchar *id = tokens[1];
      const gchar *text = tokens[2];

      gchar *clean_text = NULL;
      if (text[0] == '"' && text[strlen(text)-1] == '"') {
        // Extract text without quotes and unescape
        gchar *quoted_text = g_strndup(text + 1, strlen(text) - 2);
        clean_text = unescape_text(quoted_text);
        g_free(quoted_text);
      } else {
        // Text doesn't have quotes, unescape as is
        clean_text = unescape_text(text);
      }
      int x, y, width, height;
      double r, g, b, a;

      if (parse_point(tokens[3], &x, &y) &&
          parse_point(tokens[4], &width, &height) &&
          parse_color(tokens[5], &r, &g, &b, &a)) {

        ElementPosition position = { .x = x, .y = y, .z = data->next_z_index++ };
        ElementColor bg_color = { .r = r, .g = g, .b = b, .a = a };
        ElementSize size = { .width = width, .height = height };
        ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                               .video_data = NULL, .video_size = 0, .duration = 0 };

        ModelElement *model_element = model_create_element(
                                                           data->model, element_type, bg_color, position, size, media,
                                                           0, NULL, -1, -1, clean_text
                                                           );

        if (model_element) {
          model_element->visual_element = create_visual_element(model_element, data);
          g_hash_table_insert(element_map, g_strdup(id), model_element);
          undo_manager_push_create_action(data->undo_manager, model_element);
        }
      } else {
        g_print("Failed to parse parameters for %s\n", tokens[0]);
      }

      g_free(clean_text);
    }
    else if (g_strcmp0(tokens[0], "connect") == 0 && token_count >= 3) {
      // connect FROM_ID TO_ID
      // Store for processing after all notes are created
      ConnectionInfo *info = g_new0(ConnectionInfo, 1);
      info->from_id = g_strdup(tokens[1]);
      info->to_id = g_strdup(tokens[2]);
      connections = g_list_append(connections, info);
    }

    for (int j = 0; j < token_count; j++)
      g_free(tokens[j]);
    g_free(tokens);
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
      ElementSize size = { .width = 1, .height = 1 };
      ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0,
                             .video_data = NULL, .video_size = 0, .duration = 0 };

      ModelElement *model_conn = model_create_element(
                                                      data->model, ELEMENT_CONNECTION, bg_color, position, size, media,
                                                      from_model->uuid, to_model->uuid, from_point, to_point, NULL
                                                      );

      if (model_conn) {
        model_conn->visual_element = create_visual_element(model_conn, data);
        undo_manager_push_create_action(data->undo_manager, model_conn);
      }
    } else {
      g_print("Could not find elements for connection: %s -> %s\n", info->from_id, info->to_id);
    }

    g_free(info->from_id);
    g_free(info->to_id);
    g_free(info);
  }

  g_list_free(connections);
  g_hash_table_destroy(element_map);
  g_strfreev(lines);

  canvas_sync_with_model(data);
  gtk_widget_queue_draw(data->drawing_area);
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
      gchar *color_str = g_strdup_printf("(%.2f,%.2f,%.2f,%.2f)",
                                         element->bg_color->r,
                                         element->bg_color->g,
                                         element->bg_color->b,
                                         element->bg_color->a);

      g_string_append_printf(dsl, "%s %s %s %s %s %s\n",
                             command, element_id, text_escaped, pos_str, size_str, color_str);

      g_free(text_escaped);
      g_free(pos_str);
      g_free(size_str);
      g_free(color_str);
    }
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
        g_string_append_printf(dsl, "connect %s %s\n", from_id, to_id);
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
