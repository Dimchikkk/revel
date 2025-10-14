#include <glib.h>
#include <math.h>

#include "canvas.h"
#include "dsl_executor.h"
#include "model.h"
#include "animation.h"

static ElementConfig make_note_config(void) {
  ElementConfig config = {0};
  config.type = ELEMENT_NOTE;
  config.bg_color = (ElementColor){1.0, 1.0, 1.0, 1.0};
  config.position = (ElementPosition){0, 0, 1};
  config.size = (ElementSize){100, 60};
  config.media = (ElementMedia){MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};
  config.drawing = (ElementDrawing){NULL, 0};
  config.connection = (ElementConnection){0};
  config.text.text_color = (ElementColor){0.0, 0.0, 0.0, 1.0};
  config.text.strikethrough = FALSE;
  return config;
}

static void test_animate_move_accepts_uuid(void) {
  CanvasData *data = g_new0(CanvasData, 1);
  data->next_z_index = 1;
  data->dsl_aliases = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  const char *db_path = "test_dsl_uuid.db";
  remove(db_path);
  data->model = model_new_with_file(db_path);
  g_assert_nonnull(data->model);

  ElementConfig config = make_note_config();
  config.text.text = g_strdup("UUID Note");
  config.text.font_description = g_strdup("Sans 12");
  config.text.alignment = g_strdup("center");

  ModelElement *element = model_create_element(data->model, config);
  g_assert_nonnull(element);
  g_free(config.text.text);
  g_free(config.text.font_description);
  g_free(config.text.alignment);

  const gchar *uuid = element->uuid;
  g_assert_nonnull(uuid);

  gchar *script = g_strdup_printf("animate_move %s (0,0) (42,84) 0 0\n", uuid);
  canvas_execute_script_internal(data, script, "uuid_test.dsl", FALSE);

  g_assert_nonnull(element->position);
  g_assert_cmpint(element->position->x, ==, 42);
  g_assert_cmpint(element->position->y, ==, 84);

  if (data->anim_engine) {
    animation_engine_cleanup(data->anim_engine);
    g_free(data->anim_engine);
  }

  model_free(data->model);
  remove(db_path);
  g_free(script);
  if (data->dsl_aliases) g_hash_table_destroy(data->dsl_aliases);
  g_free(data);
}

static void test_animate_color_updates_model(void) {
  CanvasData *data = g_new0(CanvasData, 1);
  data->next_z_index = 1;
  data->dsl_aliases = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  const char *db_path = "test_dsl_color.db";
  remove(db_path);
  data->model = model_new_with_file(db_path);
  g_assert_nonnull(data->model);

  const char *create_script = "shape_create circle_B circle \"\" (0,0) (40,40) filled true bg color(0.5,0.5,0.5,1) stroke 1 stroke_color color(0,0,0,1)\n";
  canvas_execute_script_internal(data, create_script, "color_test_create.dsl", FALSE);

  const char *color_script = "animate_color circle_B color(0.5,0.5,0.5,1) color(1,0,0,1) 0 0\n";
  canvas_execute_script_internal(data, color_script, "color_test_update.dsl", FALSE);

  const gchar *uuid = g_hash_table_lookup(data->dsl_aliases, "circle_B");
  g_assert_nonnull(uuid);
  ModelElement *element = g_hash_table_lookup(data->model->elements, uuid);
  g_assert_nonnull(element);
  g_assert_nonnull(element->bg_color);
  g_assert_cmpfloat_with_epsilon(element->bg_color->r, 1.0, 1e-6);
  g_assert_cmpfloat_with_epsilon(element->bg_color->g, 0.0, 1e-6);
  g_assert_cmpfloat_with_epsilon(element->bg_color->b, 0.0, 1e-6);
  g_assert_cmpfloat_with_epsilon(element->bg_color->a, 1.0, 1e-6);

  if (data->anim_engine) {
    animation_engine_cleanup(data->anim_engine);
    g_free(data->anim_engine);
  }

  model_free(data->model);
  remove(db_path);
  if (data->dsl_aliases) g_hash_table_destroy(data->dsl_aliases);
  g_free(data);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/dsl/animate_move_uuid", test_animate_move_accepts_uuid);
  g_test_add_func("/dsl/animate_color_uuid", test_animate_color_updates_model);
  return g_test_run();
}
