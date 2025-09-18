// test_undo_manager.c
#include "undo_manager.h"
#include "model.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

// Simple test fixture
typedef struct {
  UndoManager *undo_manager;
  Model *model;
} TestFixture;

static void test_setup(TestFixture *fixture, gconstpointer user_data) {
  fixture->model = g_new0(Model, 1);
  fixture->model->elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  fixture->undo_manager = undo_manager_new(fixture->model);
}

static void test_teardown(TestFixture *fixture, gconstpointer user_data) {
  if (fixture->model) {
    g_free(fixture->model);
  }
}

// Test 1: UndoManager creation and basic functionality
static void test_undo_manager_creation(TestFixture *fixture, gconstpointer user_data) {
  g_assert_nonnull(fixture->undo_manager);
  g_assert_null(fixture->undo_manager->undo_stack);
  g_assert_null(fixture->undo_manager->redo_stack);
  g_assert_null(fixture->undo_manager->action_log);
  g_assert_null(fixture->undo_manager->log_window);
  g_assert_null(fixture->undo_manager->log_store);
  g_assert_nonnull(fixture->undo_manager->model);
}

// Test 2: Push a simple action
static void test_push_simple_action(TestFixture *fixture, gconstpointer user_data) {
  ModelElement *element = g_new0(ModelElement, 1);
  element->type = g_new0(ModelType, 1);
  element->type->type = ELEMENT_NOTE;

  undo_manager_push_create_action(fixture->undo_manager, element);

  g_assert_nonnull(fixture->undo_manager->undo_stack);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 1);

  Action *action = (Action*)g_list_last(fixture->undo_manager->undo_stack)->data;
  g_assert_cmpint(action->type, ==, ACTION_CREATE_ELEMENT);
  g_assert_nonnull(action->description);
  g_assert_true(g_str_has_prefix(action->description, "Created Note"));

  // Clean up - the element is not referenced by the model, so we free it
  g_free(element->type);
  g_free(element);
}

// Test 3: Can undo/redo functionality
static void test_can_undo_redo(TestFixture *fixture, gconstpointer user_data) {
  // Initially should not be able to undo or redo
  g_assert_false(undo_manager_can_undo(fixture->undo_manager));
  g_assert_false(undo_manager_can_redo(fixture->undo_manager));

  // Push an action
  ModelElement *element = g_new0(ModelElement, 1);
  element->type = g_new0(ModelType, 1);
  element->type->type = ELEMENT_NOTE;

  undo_manager_push_create_action(fixture->undo_manager, element);

  // Should be able to undo but not redo
  g_assert_true(undo_manager_can_undo(fixture->undo_manager));
  g_assert_false(undo_manager_can_redo(fixture->undo_manager));

  // Clean up - the element is not referenced by the model, so we free it
  g_free(element->type);
  g_free(element);
}

// Test 4: Basic undo functionality - simplified
static void test_basic_undo(TestFixture *fixture, gconstpointer user_data) {
  ModelElement *element = g_new0(ModelElement, 1);
  element->type = g_new0(ModelType, 1);
  element->type->type = ELEMENT_NOTE;
  element->uuid = g_strdup("test-uuid");
  element->state = MODEL_STATE_SAVED;

  // Don't add to model's hash table to avoid ownership conflict
  undo_manager_push_delete_action(fixture->undo_manager, element);

  // Perform undo
  undo_manager_undo(fixture->undo_manager);

  // Should have moved action to redo stack
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 0);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->redo_stack), ==, 1);

  // Clean up the element manually since it's not in the model
  g_free(element->uuid);
  g_free(element->type);
  g_free(element);
}

// Test 5: Action log gets populated
static void test_action_log(TestFixture *fixture, gconstpointer user_data) {
  ModelElement *element = g_new0(ModelElement, 1);
  element->type = g_new0(ModelType, 1);
  element->type->type = ELEMENT_NOTE;

  undo_manager_push_create_action(fixture->undo_manager, element);

  // Action log should also contain the action
  g_assert_nonnull(fixture->undo_manager->action_log);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->action_log), ==, 1);

  Action *log_action = (Action*)g_list_last(fixture->undo_manager->action_log)->data;
  g_assert_cmpint(log_action->type, ==, ACTION_CREATE_ELEMENT);
  g_assert_nonnull(log_action->description);

  // Clean up - the element is not referenced by the model, so we free it
  g_free(element->type);
  g_free(element);
}

// Test 6: Complex undo/redo scenario with multiple actions
static void test_complex_undo_redo(TestFixture *fixture, gconstpointer user_data) {
  // Create a properly allocated test element
  ModelElement *element = g_new0(ModelElement, 1);
  element->type = g_new0(ModelType, 1);
  element->type->type = ELEMENT_NOTE;
  element->uuid = g_strdup("test-uuid-2");
  element->state = MODEL_STATE_SAVED;
  element->position = g_new0(ModelPosition, 1);
  element->position->x = 100;
  element->position->y = 200;
  element->size = g_new0(ModelSize, 1);
  element->size->width = 50;
  element->size->height = 30;
  element->text = g_new0(ModelText, 1);
  element->text->text = g_strdup("Initial text");
  element->bg_color = g_new0(ModelColor, 1);
  element->bg_color->r = 1.0;
  element->bg_color->g = 1.0;
  element->bg_color->b = 1.0;
  element->bg_color->a = 1.0;

  // Add element to model's hash table - the hash table will own the element
  g_hash_table_insert(fixture->model->elements, g_strdup(element->uuid), element);

  // Push multiple actions
  undo_manager_push_move_action(fixture->undo_manager, element, 100, 200, 150, 250);
  undo_manager_push_resize_action(fixture->undo_manager, element, 50, 30, 80, 40);
  undo_manager_push_text_action(fixture->undo_manager, element, "Initial text", "Updated text");
  undo_manager_push_color_action(fixture->undo_manager, element,
                                 1.0, 1.0, 1.0, 1.0,
                                 0.8, 0.8, 0.9, 1.0);

  // Verify we have 4 actions in undo stack
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 4);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->redo_stack), ==, 0);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->action_log), ==, 4);

  // Perform two undos
  undo_manager_undo(fixture->undo_manager); // Undo color change
  undo_manager_undo(fixture->undo_manager); // Undo text change

  // Verify stacks after two undos
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 2);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->redo_stack), ==, 2);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->action_log), ==, 4); // Log should remain unchanged

  // Perform one redo
  undo_manager_redo(fixture->undo_manager); // Redo text change

  // Verify stacks after one redo
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 3);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->redo_stack), ==, 1);

  // Push a new action while we have redo stack (should clear redo stack)
  undo_manager_push_delete_action(fixture->undo_manager, element);

  // Verify redo stack was cleared and undo stack has new action + previous ones
  g_assert_cmpuint(g_list_length(fixture->undo_manager->undo_stack), ==, 4);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->redo_stack), ==, 0);
  g_assert_cmpuint(g_list_length(fixture->undo_manager->action_log), ==, 5);

  // Verify the last action is the delete action
  Action *last_action = (Action*)g_list_last(fixture->undo_manager->undo_stack)->data;
  g_assert_cmpint(last_action->type, ==, ACTION_DELETE_ELEMENT);
  // Don't free element here - it's owned by the model's hash table
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);

  // Add tests
  g_test_add("/undo_manager/creation", TestFixture, NULL, test_setup, test_undo_manager_creation, test_teardown);
  g_test_add("/undo_manager/push_simple_action", TestFixture, NULL, test_setup, test_push_simple_action, test_teardown);
  g_test_add("/undo_manager/can_undo_redo", TestFixture, NULL, test_setup, test_can_undo_redo, test_teardown);
  g_test_add("/undo_manager/basic_undo", TestFixture, NULL, test_setup, test_basic_undo, test_teardown);
  g_test_add("/undo_manager/action_log", TestFixture, NULL, test_setup, test_action_log, test_teardown);
  g_test_add("/undo_manager/complex_undo_redo", TestFixture, NULL, test_setup, test_complex_undo_redo, test_teardown);

  return g_test_run();
}
