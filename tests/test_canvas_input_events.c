#include "canvas_input.h"
#include "canvas_core.h"
#include "model.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

typedef struct {
  CanvasData *canvas;
  GtkWidget *overlay;
  GtkWidget *drawing_area;
  gchar *db_path;
} CanvasInputFixture;

static gchar* create_temp_db_path(void) {
  gchar *tmp_dir = g_dir_make_tmp("revel-canvas-input-XXXXXX", NULL);
  g_assert_nonnull(tmp_dir);
  gchar *db_path = g_build_filename(tmp_dir, "test.db", NULL);
  g_free(tmp_dir);
  return db_path;
}

static void setup_fixture(CanvasInputFixture *fixture, gconstpointer user_data) {
  (void)user_data;

  fixture->overlay = gtk_overlay_new();
  g_object_ref_sink(fixture->overlay);
  fixture->drawing_area = gtk_drawing_area_new();
  g_object_ref_sink(fixture->drawing_area);
  gtk_overlay_set_child(GTK_OVERLAY(fixture->overlay), fixture->drawing_area);

  fixture->db_path = create_temp_db_path();
  g_remove(fixture->db_path);

  fixture->canvas = canvas_data_new_with_db(fixture->drawing_area,
                                            fixture->overlay,
                                            fixture->db_path);
  g_assert_nonnull(fixture->canvas);
}

static void teardown_fixture(CanvasInputFixture *fixture, gconstpointer user_data) {
  (void)user_data;

  if (fixture->canvas) {
    canvas_data_free(fixture->canvas);
    fixture->canvas = NULL;
  }

  if (fixture->overlay) {
    g_object_unref(fixture->overlay);
    fixture->overlay = NULL;
  }
  if (fixture->drawing_area) {
    g_object_unref(fixture->drawing_area);
    fixture->drawing_area = NULL;
  }

  if (fixture->db_path) {
    g_remove(fixture->db_path);
    gchar *dir_path = g_path_get_dirname(fixture->db_path);
    if (dir_path) {
      g_rmdir(dir_path);
      g_free(dir_path);
    }
    g_free(fixture->db_path);
    fixture->db_path = NULL;
  }
}

static ModelElement* add_note(CanvasData *canvas, int x, int y) {
  ElementConfig config = {0};
  config.type = ELEMENT_NOTE;
  config.position = (ElementPosition){ .x = x, .y = y, .z = canvas->next_z_index++ };
  config.size = (ElementSize){ .width = 120, .height = 60 };
  config.bg_color = (ElementColor){ 0.2, 0.2, 0.25, 1.0 };
  config.text.text = g_strdup("Test Note");
  config.text.text_color = (ElementColor){ 1.0, 1.0, 1.0, 1.0 };
  config.text.font_description = g_strdup("Ubuntu Mono 12");
  config.text.strikethrough = FALSE;
  config.media = (ElementMedia){ .type = MEDIA_TYPE_NONE };
  config.drawing = (ElementDrawing){ .drawing_points = NULL, .stroke_width = 0 };
  config.connection = (ElementConnection){ 0 };

  ModelElement *model_element = model_create_element(canvas->model, config);
  g_assert_nonnull(model_element);

  g_free(config.text.text);
  g_free(config.text.font_description);

  model_element->visual_element = create_visual_element(model_element, canvas);
  g_assert_nonnull(model_element->visual_element);

  canvas_rebuild_quadtree(canvas);

  return model_element;
}

static gboolean emit_press(CanvasData *canvas, double x, double y, int n_press) {
  UIEvent event = {0};
  event.type = UI_EVENT_POINTER_PRIMARY_PRESS;
  event.canvas = canvas;
  event.data.pointer.x = x;
  event.data.pointer.y = y;
  event.data.pointer.n_press = n_press;
  event.data.pointer.modifiers = 0;
  return ui_event_bus_emit(&event);
}

static gboolean emit_release(CanvasData *canvas, double x, double y, int n_press) {
  UIEvent event = {0};
  event.type = UI_EVENT_POINTER_PRIMARY_RELEASE;
  event.canvas = canvas;
  event.data.pointer.x = x;
  event.data.pointer.y = y;
  event.data.pointer.n_press = n_press;
  event.data.pointer.modifiers = 0;
  return ui_event_bus_emit(&event);
}

static void test_pointer_press_selects_element(CanvasInputFixture *fixture, gconstpointer user_data) {
  (void)user_data;

  canvas_input_register_event_handlers(fixture->canvas);

  ModelElement *element = add_note(fixture->canvas, 100, 100);
  Element *visual = element->visual_element;

  gboolean handled = emit_press(fixture->canvas, 110, 110, 1);
  g_assert_true(handled);

  g_assert_nonnull(fixture->canvas->selected_elements);
  g_assert_true(fixture->canvas->selected_elements->data == visual);

  Element *picked = canvas_pick_element(fixture->canvas, 110, 110);
  g_assert_true(picked == visual);

  handled = emit_release(fixture->canvas, 110, 110, 1);
  g_assert_true(handled);

  g_assert_false(visual->dragging);
  g_assert_false(fixture->canvas->selecting);
}

static void test_empty_click_starts_selection(CanvasInputFixture *fixture, gconstpointer user_data) {
  (void)user_data;

  canvas_input_register_event_handlers(fixture->canvas);

  gboolean handled = emit_press(fixture->canvas, 10, 10, 1);
  g_assert_true(handled);
  g_assert_true(fixture->canvas->selecting);
  g_assert_null(fixture->canvas->selected_elements);

  handled = emit_release(fixture->canvas, 10, 10, 1);
  g_assert_true(handled);
  g_assert_false(fixture->canvas->selecting);
}

static void test_unregister_removes_handlers(CanvasInputFixture *fixture, gconstpointer user_data) {
  (void)user_data;

  canvas_input_register_event_handlers(fixture->canvas);
  canvas_input_unregister_event_handlers(fixture->canvas);

  UIEvent event = {0};
  event.type = UI_EVENT_POINTER_PRIMARY_PRESS;
  event.canvas = fixture->canvas;
  event.data.pointer.x = 0;
  event.data.pointer.y = 0;
  event.data.pointer.n_press = 1;
  event.data.pointer.modifiers = 0;

  gboolean handled = ui_event_bus_emit(&event);
  g_assert_false(handled);
  g_assert_false(fixture->canvas->selecting);
}

int main(int argc, char *argv[]) {
  gtk_init();
  g_test_init(&argc, &argv, NULL);

  g_test_add("/canvas-input/pointer-press-selects-element",
             CanvasInputFixture, NULL,
             setup_fixture, test_pointer_press_selects_element, teardown_fixture);

  g_test_add("/canvas-input/empty-click-starts-selection",
             CanvasInputFixture, NULL,
             setup_fixture, test_empty_click_starts_selection, teardown_fixture);

  g_test_add("/canvas-input/unregister-removes-handlers",
             CanvasInputFixture, NULL,
             setup_fixture, test_unregister_removes_handlers, teardown_fixture);

  return g_test_run();
}
