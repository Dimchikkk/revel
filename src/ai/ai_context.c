#include "ai_context.h"

#include <math.h>
#include <string.h>
#include "dsl_executor.h"
#include "canvas.h"
#include "model.h"
#include "element.h"
#include "shape.h"

#define AI_HISTORY_DEFAULT_LIMIT 3
#define AI_GRAMMAR_SNIPPET_LIMIT (2 * 1024)

static guint resolve_history_limit(const AiContextOptions *options) {
  if (!options || options->history_limit == 0) {
    return AI_HISTORY_DEFAULT_LIMIT;
  }
  return options->history_limit;
}

static guint resolve_max_bytes(const AiContextOptions *options) {
  if (!options || options->max_context_bytes == 0) {
    return AI_CONTEXT_DEFAULT_MAX_BYTES;
  }
  return options->max_context_bytes;
}

static gboolean resolve_include_grammar(const AiContextOptions *options) {
  return options ? options->include_grammar : TRUE;
}

static gchar *load_grammar_snippet(void) {
  gchar *contents = NULL;
  gsize length = 0;
  if (!g_file_get_contents("DSL.md", &contents, &length, NULL)) {
    return NULL;
  }
  if (length > AI_GRAMMAR_SNIPPET_LIMIT) {
    contents[AI_GRAMMAR_SNIPPET_LIMIT] = '\0';
  }
  return contents;
}

static const char *element_human_label(ElementType type) {
  switch (type) {
  case ELEMENT_NOTE: return "Rich Notes";
  case ELEMENT_PAPER_NOTE: return "Paper Notes";
  case ELEMENT_MEDIA_FILE: return "Media Files";
  case ELEMENT_SHAPE: return "Shapes";
  case ELEMENT_FREEHAND_DRAWING: return "Freehand";
  case ELEMENT_CONNECTION: return "Connections";
  case ELEMENT_SPACE: return "Spaces";
  default: return "Elements";
  }
}

static gchar *build_space_summary(CanvasData *data) {
  if (!data || !data->model || !data->model->elements) {
    return g_strdup("No model data available.");
  }

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, data->model->elements);

  GHashTable *counts = g_hash_table_new(g_direct_hash, g_direct_equal);
  GPtrArray *titles = g_ptr_array_new_with_free_func(g_free);
  guint total = 0;
  guint media_image = 0;
  guint media_video = 0;
  guint media_audio = 0;

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = value;
    if (!element || element->state == MODEL_STATE_DELETED) {
      continue;
    }
    if (g_strcmp0(element->space_uuid, data->model->current_space_uuid) != 0) {
      continue;
    }
    total++;

    gpointer count_ptr = g_hash_table_lookup(counts, GINT_TO_POINTER(element->type->type));
    guint count = GPOINTER_TO_UINT(count_ptr);
    count++;
    g_hash_table_insert(counts, GINT_TO_POINTER(element->type->type), GUINT_TO_POINTER(count));

    if (element->type->type == ELEMENT_MEDIA_FILE) {
      if (element->image) {
        media_image++;
      } else if (element->video) {
        media_video++;
      } else if (element->audio) {
        media_audio++;
      }
    }

    if ((element->type->type == ELEMENT_NOTE || element->type->type == ELEMENT_PAPER_NOTE) &&
        titles->len < 5 && element->text && element->text->text && *element->text->text) {
      gchar *excerpt = g_strdup(element->text->text);
      if (g_utf8_strlen(excerpt, -1) > 80) {
        gchar *tmp = g_utf8_substring(excerpt, 0, 80);
        g_free(excerpt);
        excerpt = g_strdup_printf("%s…", tmp);
        g_free(tmp);
      }
      g_ptr_array_add(titles, excerpt);
    }
  }

  GString *summary = g_string_new(NULL);
  gchar *space_name = NULL;
  if (data->model->current_space_uuid) {
    model_get_space_name(data->model, data->model->current_space_uuid, &space_name);
  }

  g_string_append_printf(summary, "Space: %s\n", space_name ? space_name : "(unnamed)");
  g_string_append_printf(summary, "Total elements: %u\n", total);

  const ElementType interesting[] = {
    ELEMENT_NOTE,
    ELEMENT_PAPER_NOTE,
    ELEMENT_MEDIA_FILE,
    ELEMENT_SHAPE,
    ELEMENT_FREEHAND_DRAWING,
    ELEMENT_CONNECTION,
    ELEMENT_SPACE
  };

  for (guint i = 0; i < G_N_ELEMENTS(interesting); i++) {
    guint count = GPOINTER_TO_UINT(g_hash_table_lookup(counts, GINT_TO_POINTER(interesting[i])));
    if (count > 0) {
      g_string_append_printf(summary, "- %s: %u\n", element_human_label(interesting[i]), count);
    }
  }

  if (media_image + media_video + media_audio > 0) {
    g_string_append(summary, "  Media breakdown:\n");
    if (media_image > 0) {
      g_string_append_printf(summary, "    • Images: %u\n", media_image);
    }
    if (media_video > 0) {
      g_string_append_printf(summary, "    • Video: %u\n", media_video);
    }
    if (media_audio > 0) {
      g_string_append_printf(summary, "    • Audio: %u\n", media_audio);
    }
  }

  if (titles->len > 0) {
    g_string_append(summary, "Sample note titles:\n");
    for (guint i = 0; i < titles->len; i++) {
      g_string_append_printf(summary, "  • %s\n", (char *)g_ptr_array_index(titles, i));
    }
  }

  g_hash_table_destroy(counts);
  g_ptr_array_free(titles, TRUE);
  g_free(space_name);
  return g_string_free(summary, FALSE);
}

typedef struct {
  ModelElement *element;
} ElementIndexEntry;

static const char *shape_type_to_name(int shape_type) {
  switch (shape_type) {
    case SHAPE_CIRCLE: return "circle";
    case SHAPE_RECTANGLE: return "rectangle";
    case SHAPE_TRIANGLE: return "triangle";
    case SHAPE_CYLINDER_VERTICAL: return "vcylinder";
    case SHAPE_CYLINDER_HORIZONTAL: return "hcylinder";
    case SHAPE_DIAMOND: return "diamond";
    case SHAPE_ROUNDED_RECTANGLE: return "roundedrect";
    case SHAPE_TRAPEZOID: return "trapezoid";
    case SHAPE_LINE: return "line";
    case SHAPE_ARROW: return "arrow";
    case SHAPE_BEZIER: return "bezier";
    case SHAPE_CURVED_ARROW: return "curved_arrow";
    case SHAPE_CUBE: return "cube";
    case SHAPE_PLOT: return "plot";
    case SHAPE_OVAL: return "oval";
    case SHAPE_TEXT_OUTLINE: return "text_outline";
    default: return "shape";
  }
}

static gint compare_element_entries(gconstpointer a, gconstpointer b) {
  const ElementIndexEntry *ea = *(const ElementIndexEntry **)a;
  const ElementIndexEntry *eb = *(const ElementIndexEntry **)b;
  if (!ea || !eb || !ea->element || !eb->element) {
    return 0;
  }
  const gchar *ca = ea->element->created_at;
  const gchar *cb = eb->element->created_at;
  if (ca && cb) {
    return g_strcmp0(cb, ca);
  }
  if (ca) {
    return -1;
  }
  if (cb) {
    return 1;
  }
  return g_strcmp0(eb->element->uuid, ea->element->uuid);
}

static gchar *build_recent_element_index(CanvasData *data, guint max_items) {
  if (!data || !data->model || !data->model->elements) {
    return NULL;
  }

  const gchar *current_space = data->model->current_space_uuid;
  if (!current_space) {
    return NULL;
  }

  // Build reverse map: uuid -> alias from dsl_aliases
  GHashTable *uuid_to_alias = g_hash_table_new(g_str_hash, g_str_equal);
  if (data->dsl_aliases) {
    GHashTableIter alias_iter;
    gpointer alias_key, alias_value;
    g_hash_table_iter_init(&alias_iter, data->dsl_aliases);
    while (g_hash_table_iter_next(&alias_iter, &alias_key, &alias_value)) {
      const gchar *alias = alias_key;
      const gchar *uuid = alias_value;
      g_hash_table_insert(uuid_to_alias, (gpointer)uuid, (gpointer)alias);
    }
  }

  GPtrArray *entries = g_ptr_array_new_with_free_func(g_free);
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, data->model->elements);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *element = value;
    if (!element || element->state == MODEL_STATE_DELETED) {
      continue;
    }
    if (g_strcmp0(element->space_uuid, current_space) != 0) {
      continue;
    }
    ElementIndexEntry *entry = g_new0(ElementIndexEntry, 1);
    entry->element = element;
    g_ptr_array_add(entries, entry);
  }

  if (entries->len == 0) {
    g_ptr_array_free(entries, TRUE);
    g_hash_table_destroy(uuid_to_alias);
    return NULL;
  }

  g_ptr_array_sort(entries, compare_element_entries);

  GString *summary = g_string_new(NULL);
  guint count = MIN(entries->len, max_items > 0 ? max_items : entries->len);
  for (guint i = 0; i < count; i++) {
    ElementIndexEntry *entry = g_ptr_array_index(entries, i);
    ModelElement *element = entry ? entry->element : NULL;
    if (!element) {
      continue;
    }

    const char *type_name = element_get_type_name(element->type ? element->type->type : ELEMENT_SHAPE);
    const char *shape_name = NULL;
    if (element->type && element->type->type == ELEMENT_SHAPE) {
      shape_name = shape_type_to_name(element->shape_type);
    }
    int x = element->position ? element->position->x : 0;
    int y = element->position ? element->position->y : 0;
    int w = element->size ? element->size->width : 0;
    int h = element->size ? element->size->height : 0;

    // Use DSL alias if available, otherwise fall back to UUID
    const gchar *element_id = element->uuid;
    if (element->uuid) {
      const gchar *alias = g_hash_table_lookup(uuid_to_alias, element->uuid);
      if (alias) {
        element_id = alias;
      }
    }

    g_string_append_printf(summary,
                           "- %s (%s%s%s) at (%d,%d) size (%d,%d)\n",
                           element_id ? element_id : "(unknown)",
                           type_name ? type_name : "Element",
                           shape_name ? ": " : "",
                           shape_name ? shape_name : "",
                           x, y, w, h);
  }

  g_ptr_array_free(entries, TRUE);
  g_hash_table_destroy(uuid_to_alias);
  return g_string_free(summary, FALSE);
}

char *ai_context_truncate_utf8(const char *text, guint max_bytes) {
  if (!text) {
    return g_strdup("");
  }
  gsize len = strlen(text);
  if (len <= max_bytes) {
    return g_strdup(text);
  }
  guint target = max_bytes;
  while (target > 0 && (text[target] & 0xC0) == 0x80) {
    target--;
  }
  if (target == 0) {
    target = max_bytes;
  }
  return g_strndup(text, target);
}

static void append_history_block(GString *payload, const AiSessionState *session, guint history_limit) {
  if (!session || !session->log || session->log->len == 0) {
    return;
  }
  g_string_append(payload, "### HISTORY\n");
  guint count = session->log->len;
  guint start = count > history_limit ? count - history_limit : 0;
  for (guint i = start; i < count; i++) {
    AiConversationEntry *entry = g_ptr_array_index(session->log, i);
    if (!entry) continue;
    g_string_append(payload, "#### EXCHANGE\n");
    if (entry->prompt) {
      g_string_append(payload, "USER:\n");
      g_string_append(payload, entry->prompt);
      g_string_append_c(payload, '\n');
    }
    if (entry->dsl) {
      g_string_append(payload, "DSL:\n");
      g_string_append(payload, entry->dsl);
      g_string_append_c(payload, '\n');
    } else if (entry->error) {
      g_string_append(payload, "ERROR:\n");
      g_string_append(payload, entry->error);
      g_string_append_c(payload, '\n');
    }
  }
}

char *ai_context_build_payload(CanvasData *data,
                               AiSessionState *session,
                               const char *prompt,
                               const AiContextOptions *options,
                               char **out_snapshot,
                               gboolean *out_truncated,
                               GError **error) {
  if (!data || !prompt) {
    g_set_error(error, g_quark_from_static_string("ai_context"), 1, "Missing data or prompt");
    return NULL;
  }

  guint max_bytes = resolve_max_bytes(options);
  guint history_limit = resolve_history_limit(options);
  gboolean include_grammar = resolve_include_grammar(options);

  gchar *full_dsl = canvas_generate_dsl_from_model(data);
  if (!full_dsl) {
    full_dsl = g_strdup("");
  }

  gboolean truncated = FALSE;
  gchar *context_section = NULL;
  gchar *summary = NULL;

  if (strlen(full_dsl) > max_bytes) {
    truncated = TRUE;
    summary = build_space_summary(data);
    gchar *slice = ai_context_truncate_utf8(full_dsl, max_bytes);
    context_section = g_strdup_printf("%s\n...", slice);
    g_free(slice);
  } else {
    context_section = g_strdup(full_dsl);
  }

  GString *payload = g_string_new(NULL);
  g_string_append(payload, "### INSTRUCTIONS\n");
  g_string_append(payload, "You are a Revel DSL assistant. Generate valid DSL scripts to modify the infinite canvas.\n");
  g_string_append(payload, "The CURRENT_DSL shows existing elements. Use ELEMENT_INDEX to reference them by ID.\n");
  g_string_append(payload, "Make minimal, targeted changes. Don't recreate elements unless requested.\n");
  g_string_append(payload, "CRITICAL: Text content is PART OF the shape/note element, NOT a separate element. Never create standalone text for labels.\n\n");

  append_history_block(payload, session, history_limit);

  if (summary && *summary) {
    g_string_append(payload, "### CURRENT_DSL_SUMMARY\n");
    g_string_append(payload, summary);
    gsize summary_len = strlen(summary);
    if (summary_len > 0 && summary[summary_len - 1] != '\n') {
      g_string_append_c(payload, '\n');
    }
  }

  g_string_append(payload, "### CURRENT_DSL\n");
  g_string_append(payload, context_section);
  gsize context_len = strlen(context_section);
  if (context_len > 0 && context_section[context_len - 1] != '\n') {
    g_string_append_c(payload, '\n');
  }

  gchar *element_index = build_recent_element_index(data, 20);
  if (element_index && *element_index) {
    g_string_append(payload, "### ELEMENT_INDEX\n");
    g_string_append(payload, element_index);
    gsize index_len = strlen(element_index);
    if (index_len > 0 && element_index[index_len - 1] != '\n') {
      g_string_append_c(payload, '\n');
    }
  }
  g_free(element_index);

  g_string_append(payload, "### USER_REQUEST\n");
  g_string_append(payload, prompt);
  gsize prompt_len = strlen(prompt);
  if (prompt_len > 0 && prompt[prompt_len - 1] != '\n') {
    g_string_append_c(payload, '\n');
  }

  g_string_append(payload, "### RESPONSE_FORMAT\n");
  g_string_append(payload, "Output ONLY valid Revel DSL script. No explanations, comments, or markdown.\n");
  g_string_append(payload, "NEVER output text_create immediately after shape_create with the same coordinates.\n\n");

  g_string_append(payload, "### CRITICAL_RULES\n");
  g_string_append(payload, "1. **NEVER USE text_create TO LABEL SHAPES**: Shapes have built-in label parameter. Using text_create creates broken overlapping elements.\n");
  g_string_append(payload, "   ✓ CORRECT: shape_create box rectangle \"My Label\" (100,100) (200,80)\n");
  g_string_append(payload, "   ✗ WRONG: shape_create box rectangle \"\" (100,100) (200,80)\\ntext_create label \"My Label\" (100,100) (200,80)\n");
  g_string_append(payload, "   The WRONG example creates TWO elements - the shape AND a floating text box. This is a bug.\n\n");
  g_string_append(payload, "2. **Element IDs**: When modifying existing elements, use exact IDs from ELEMENT_INDEX. Never invent placeholders like elem_1.\n");
  g_string_append(payload, "3. **Coordinates**: Always use explicit integer coordinates. Reference from ELEMENT_INDEX for positioning.\n");
  g_string_append(payload, "4. **text_create is for standalone text ONLY**: Use it for titles, descriptions, captions - NOT shape labels.\n");
  g_string_append(payload, "5. **Animations**: Use instantaneous animations for immediate changes: `animate_move id (x,y) (x,y) 0 0`\n");
  g_string_append(payload, "6. **Colors**: Format as `color(r,g,b,a)` with values 0.0-1.0, or hex `#RRGGBB` / `#RRGGBBAA`\n");
  g_string_append(payload, "7. **Minimal changes**: Only modify/create what user requested. Don't recreate entire canvas.\n");
  g_string_append(payload, "8. **Visual clarity**: Leave sufficient spacing (100-200px) between distinct elements to avoid overlap.\n\n");

  g_string_append(payload, "### DSL_QUICK_REFERENCE\n");
  g_string_append(payload, "**Variables:**\n");
  g_string_append(payload, "  int count 0\n");
  g_string_append(payload, "  real temp 98.6\n");
  g_string_append(payload, "  bool active true\n");
  g_string_append(payload, "  string label \"Hello\"\n");
  g_string_append(payload, "  int cells[100] 0      # Arrays\n");
  g_string_append(payload, "  set count {count + 1} # Assignment (events only)\n\n");

  g_string_append(payload, "**Text/Notes (standalone text blocks):**\n");
  g_string_append(payload, "  note_create id \"Text\" (x,y) (w,h) [bg color(...)] [text_color color(...)] [font \"Ubuntu Bold 24\"]\n");
  g_string_append(payload, "  paper_note_create id \"Sticky\" (x,y) (w,h)\n");
  g_string_append(payload, "  text_create id \"Label\" (x,y) (w,h) [text_color color(...)]\n");
  g_string_append(payload, "  NOTE: Do NOT use text_create to label shapes - use shape label parameter instead!\n\n");

  g_string_append(payload, "**Shapes (text label is rendered INSIDE the shape):**\n");
  g_string_append(payload, "  shape_create id TYPE \"label\" (x,y) (w,h) [filled true|false] [bg color(...)] [stroke N] [stroke_color color(...)]\n");
  g_string_append(payload, "  The \"label\" parameter puts text INSIDE the shape. This is ONE element, not two.\n");
  g_string_append(payload, "  Types: circle, rectangle, roundedrect, triangle, diamond, vcylinder, hcylinder, trapezoid, line, arrow, bezier, oval, cube\n");
  g_string_append(payload, "  Example: shape_create node1 rectangle \"Input Layer\" (100,100) (200,80) creates a box with centered text inside.\n\n");

  g_string_append(payload, "**Media:**\n");
  g_string_append(payload, "  image_create id /path/to/file.png (x,y) (w,h)\n");
  g_string_append(payload, "  video_create id /path/to/file.mp4 (x,y) (w,h)\n");
  g_string_append(payload, "  audio_create id /path/to/file.mp3 (x,y) (w,h)\n\n");

  g_string_append(payload, "**Connections:**\n");
  g_string_append(payload, "  connect from_id to_id [parallel|straight] [none|single|double] [color(...)]\n\n");

  g_string_append(payload, "**Animations (immediate effect - use 0 0 for instant):**\n");
  g_string_append(payload, "  animate_move id (x,y) (x,y) 0 0          # Instant move\n");
  g_string_append(payload, "  animate_resize id (w,h) (w,h) 0 0        # Instant resize\n");
  g_string_append(payload, "  animate_color id color(...) color(...) 0 0\n");
  g_string_append(payload, "  animate_rotate id degrees 0 0\n");
  g_string_append(payload, "  Interpolation: linear, bezier, ease-in, ease-out, bounce, elastic, back\n\n");

  g_string_append(payload, "**Loops:**\n");
  g_string_append(payload, "  for i 0 9\n");
  g_string_append(payload, "    shape_create box${i} rectangle \"\" ({i*50},{i*50}) (40,40) filled true bg color(1,0,0,1)\n");
  g_string_append(payload, "  end\n\n");

  g_string_append(payload, "**Events:**\n");
  g_string_append(payload, "  on click button_id\n");
  g_string_append(payload, "    set count {count + 1}\n");
  g_string_append(payload, "    text_update label \"Count: ${count}\"\n");
  g_string_append(payload, "    element_delete button_id  # Delete element\n");
  g_string_append(payload, "  end\n");
  g_string_append(payload, "  on variable count == 10\n");
  g_string_append(payload, "    text_update status \"Done!\"\n");
  g_string_append(payload, "  end\n\n");

  if (include_grammar) {
    gchar *grammar = load_grammar_snippet();
    if (grammar && *grammar) {
      g_string_append(payload, "### DSL_GRAMMAR_SNIPPET\n");
      g_string_append(payload, grammar);
      gsize grammar_len = strlen(grammar);
      if (grammar_len > 0 && grammar[grammar_len - 1] != '\n') {
        g_string_append_c(payload, '\n');
      }
    }
    g_free(grammar);
  }

  g_string_append(payload, "### COMMON_PATTERNS\n");
  g_string_append(payload, "Move element: animate_move id (current_x,current_y) (new_x,new_y) 0 0\n");
  g_string_append(payload, "Resize element: animate_resize id (current_w,current_h) (new_w,new_h) 0 0\n");
  g_string_append(payload, "Change color: animate_color id color(old_r,old_g,old_b,old_a) color(new_r,new_g,new_b,new_a) 0 0\n");
  g_string_append(payload, "Update text: text_update id \"New text with ${variable}\"\n");
  g_string_append(payload, "Delete element: element_delete id\n");
  g_string_append(payload, "Add shape: shape_create new_id circle \"Label\" (x,y) (w,h) filled true bg color(r,g,b,a)\n");
  g_string_append(payload, "Connect: connect id1 id2 parallel single color(1,1,1,1)\n\n");

  g_string_append(payload, "### LAYOUT_GUIDELINES\n");
  g_string_append(payload, "**CRITICAL - Shapes with text labels (COMMON ERROR):**\n");
  g_string_append(payload, "RULE: shape_create already includes text. DO NOT follow it with text_create.\n\n");
  g_string_append(payload, "EXAMPLE - Creating 3 labeled boxes:\n");
  g_string_append(payload, "  ✓ CORRECT:\n");
  g_string_append(payload, "    shape_create box1 rectangle \"Label 1\" (100,100) (200,80) filled true bg color(0.9,0.7,0.2,1)\n");
  g_string_append(payload, "    shape_create box2 rectangle \"Label 2\" (350,100) (200,80) filled true bg color(0.9,0.7,0.2,1)\n");
  g_string_append(payload, "    shape_create box3 rectangle \"Label 3\" (600,100) (200,80) filled true bg color(0.9,0.7,0.2,1)\n\n");
  g_string_append(payload, "  ✗ WRONG (creates duplicate overlapping text):\n");
  g_string_append(payload, "    shape_create box1 rectangle \"\" (100,100) (200,80) filled true bg color(0.9,0.7,0.2,1)\n");
  g_string_append(payload, "    text_create box1_text \"Label 1\" (100,100) (200,80)\n");
  g_string_append(payload, "    ^ This is WRONG - now you have 2 elements at same position\n\n");
  g_string_append(payload, "Empty shapes: Use empty string: shape_create line rectangle \"\" (x,y) (w,h)\n\n");
  g_string_append(payload, "**Connected diagrams:**\n");
  g_string_append(payload, "- Position boxes with 200-300px horizontal spacing to prevent overlap\n");
  g_string_append(payload, "- Vertical spacing: 150-200px between rows to accommodate connections\n");
  g_string_append(payload, "- For flowcharts: arrange in clear vertical or horizontal flows\n");
  g_string_append(payload, "- Connections auto-route between shapes - ensure adequate spacing\n\n");
  g_string_append(payload, "**VALIDATION CHECKLIST before outputting DSL:**\n");
  g_string_append(payload, "□ Did I use text_create after shape_create? → If YES, DELETE the text_create and put text in shape label\n");
  g_string_append(payload, "□ Are any text_create coordinates within 50px of a shape? → If YES, that text should be the shape's label\n");
  g_string_append(payload, "□ Did I create empty shape labels (\"\")?  → If YES, and there's a text element nearby, merge them\n");
  g_string_append(payload, "□ Are elements spaced 150-300px apart? → If NO, increase spacing\n\n");
  g_string_append(payload, "**Common mistakes to AVOID:**\n");
  g_string_append(payload, "❌ NEVER: shape_create + text_create at same/nearby position\n");
  g_string_append(payload, "❌ NEVER: text_create for labeling shapes\n");
  g_string_append(payload, "❌ NEVER: Tight spacing <100px\n");
  g_string_append(payload, "✓ ALWAYS: Put text in shape label parameter\n");
  g_string_append(payload, "✓ ALWAYS: Use text_create ONLY for standalone titles/descriptions\n");
  g_string_append(payload, "✓ ALWAYS: Space elements 150-300px apart\n\n");

  if (out_snapshot) {
    *out_snapshot = full_dsl;
  } else {
    g_free(full_dsl);
  }

  if (out_truncated) {
    *out_truncated = truncated;
  }

  g_free(context_section);
  g_free(summary);

  return g_string_free(payload, FALSE);
}
