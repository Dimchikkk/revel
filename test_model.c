#include "model.h"
#include "database.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test database file
#define TEST_DB_FILE "test_model.db"

// Test fixture
typedef struct {
  Model *model;
  sqlite3 *db;
} TestFixture;

// Setup function
static void test_setup(TestFixture *fixture, gconstpointer user_data) {
  // Remove any existing test database
  remove(TEST_DB_FILE);

  // Initialize database
  if (sqlite3_open_v2(TEST_DB_FILE, &fixture->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
    g_error("Failed to open test database");
  }

  // Create tables
  if (!database_create_tables(fixture->db)) {
    g_error("Failed to create tables");
  }

  // Initialize default namespace
  if (!database_init_default_namespace(fixture->db)) {
    g_error("Failed to initialize default namespace");
  }

  // Create model
  fixture->model = model_new();
  fixture->model->db = fixture->db;

  // Get current space UUID
  char *current_space_uuid = NULL;
  if (!database_get_current_space_uuid(fixture->db, &current_space_uuid)) {
    g_error("Failed to get current space UUID");
  }
  fixture->model->current_space_uuid = current_space_uuid;
}

// Teardown function
static void test_teardown(TestFixture *fixture, gconstpointer user_data) {
  if (fixture->model) {
    model_free(fixture->model);
  }
  if (fixture->db) {
    sqlite3_close(fixture->db);
  }
  remove(TEST_DB_FILE);
}

// Test: Model creation and basic functionality
static void test_model_creation(TestFixture *fixture, gconstpointer user_data) {
  g_assert_nonnull(fixture->model);
  g_assert_nonnull(fixture->model->current_space_uuid);
  g_assert_nonnull(fixture->model->elements);
  g_assert_nonnull(fixture->model->types);
  g_assert_nonnull(fixture->model->texts);
  g_assert_nonnull(fixture->model->positions);
  g_assert_nonnull(fixture->model->sizes);
  g_assert_nonnull(fixture->model->colors);
  g_assert_nonnull(fixture->model->images);
}

// Test: Create different element types
static void test_create_elements(TestFixture *fixture, gconstpointer user_data) {
  // Create note element
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  ModelElement *note = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                            media, NULL, NULL, -1, -1, NULL, 0, "Test Note");
  g_assert_nonnull(note);
  g_assert_cmpint(note->type->type, ==, ELEMENT_NOTE);
  g_assert_cmpstr(note->text->text, ==, "Test Note");

  // Create paper note element
  ElementColor paper_color = {1.0, 1.0, 0.8, 1.0};
  ModelElement *paper_note = model_create_element(fixture->model, ELEMENT_PAPER_NOTE, paper_color, pos, size,
                                                  media, NULL, NULL, -1, -1, NULL, 0, "Test Paper Note");
  g_assert_nonnull(paper_note);
  g_assert_cmpint(paper_note->type->type, ==, ELEMENT_PAPER_NOTE);

  // Create connection between elements
  ModelElement *connection = model_create_element(fixture->model, ELEMENT_CONNECTION, color, pos, size,
                                                  media, note->uuid, paper_note->uuid, 0, 2, NULL, 0, NULL);
  g_assert_nonnull(connection);
  g_assert_cmpint(connection->type->type, ==, ELEMENT_CONNECTION);
  g_assert_cmpstr(connection->from_element_uuid, ==, note->uuid);
  g_assert_cmpstr(connection->to_element_uuid, ==, paper_note->uuid);
}

// Test: Update element properties
static void test_update_elements(TestFixture *fixture, gconstpointer user_data) {
  // Create element
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  ModelElement *element = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                               media, NULL, NULL, -1, -1, NULL, 0, "Initial Text");
  g_assert_nonnull(element);

  // Update text
  int result = model_update_text(fixture->model, element, "Updated Text");
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpstr(element->text->text, ==, "Updated Text");

  // Update position
  result = model_update_position(fixture->model, element, 300, 400, 5);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->position->x, ==, 300);
  g_assert_cmpint(element->position->y, ==, 400);

  // Update size
  result = model_update_size(fixture->model, element, 80, 40);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->size->width, ==, 80);
  g_assert_cmpint(element->size->height, ==, 40);

  // Update color
  result = model_update_color(fixture->model, element, 0.5, 0.5, 0.5, 1.0);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpfloat(element->bg_color->r, ==, 0.5);
  g_assert_cmpfloat(element->bg_color->g, ==, 0.5);
}

// Test: Save and load elements
static void test_save_load_elements(TestFixture *fixture, gconstpointer user_data) {
  // Create and save elements
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  ModelElement *element = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                               media, NULL, NULL, -1, -1, NULL, 0, "Test Note");
  g_assert_nonnull(element);

  // Save to database
  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Clear model and reload from database
  g_hash_table_remove_all(fixture->model->elements);
  model_load_space(fixture->model);

  // Verify element was loaded
  g_assert_cmpuint(g_hash_table_size(fixture->model->elements), ==, 1);

  ModelElement *loaded_element = NULL;
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, fixture->model->elements);
  if (g_hash_table_iter_next(&iter, &key, &value)) {
    loaded_element = (ModelElement*)value;
  }

  g_assert_nonnull(loaded_element);
  g_assert_cmpint(loaded_element->type->type, ==, ELEMENT_NOTE);
  g_assert_cmpstr(loaded_element->text->text, ==, "Test Note");
}

// Test: Delete element
static void test_delete_element(TestFixture *fixture, gconstpointer user_data) {
  // Create element
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  ModelElement *element = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                               media, NULL, NULL, -1, -1, NULL, 0, "Test Note");
  char* uuid = g_strdup(element->uuid);
  g_assert_nonnull(element);

  // Save first
  model_save_elements(fixture->model);

  // Delete element
  int result = model_delete_element(fixture->model, element);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_DELETED);

  // Save deletion
  model_save_elements(fixture->model);

  // Verify element is gone
  g_assert_null(g_hash_table_lookup(fixture->model->elements, uuid));
  g_free(uuid);
}

static void test_search_multiple_spaces(TestFixture *fixture, gconstpointer user_data) {
  // Create a new space
  char *new_space_uuid = NULL;
  int result = database_create_space(fixture->db, "Test Space", fixture->model->current_space_uuid, &new_space_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(new_space_uuid);

  // Create elements in different spaces
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  // Element in current space
  ModelElement *note1 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note in default space");
  g_assert_nonnull(note1);

  // Temporarily switch to new space to create element there
  char *old_space = fixture->model->current_space_uuid;
  fixture->model->current_space_uuid = new_space_uuid;

  ModelElement *note2 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note in test space");
  g_assert_nonnull(note2);

  // Switch back to original space
  fixture->model->current_space_uuid = old_space;

  // Save elements
  model_save_elements(fixture->model);

  // Search should find elements from both spaces
  GList *results = NULL;
  int search_result = model_search_elements(fixture->model, "space", &results);
  g_assert_cmpint(search_result, ==, 0);
  g_assert_nonnull(results);
  g_assert_cmpuint(g_list_length(results), >=, 2);

  // Verify both spaces are represented in results
  int found_default_space = 0, found_test_space = 0;
  GList *iter = results;
  while (iter != NULL) {
    ModelSearchResult *search_result = (ModelSearchResult*)iter->data;
    if (strstr(search_result->text_content, "default")) {
      found_default_space = 1;
    } else if (strstr(search_result->text_content, "test")) {
      found_test_space = 1;
    }
    iter = iter->next;
  }

  g_assert_true(found_default_space);
  g_assert_true(found_test_space);

  // Clean up
  g_list_free_full(results, (GDestroyNotify)model_free_search_result);
  g_free(new_space_uuid);
}

// Test: Cyclic connection space movement
static void test_cyclic_connection_space_movement(TestFixture *fixture, gconstpointer user_data) {
  // Create a new space
  char *target_space_uuid = NULL;
  int result = database_create_space(fixture->db, "Target Space", fixture->model->current_space_uuid, &target_space_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(target_space_uuid);

  // Create elements
  ElementPosition pos = {100, 200, 1};
  ElementColor color = {1.0, 1.0, 1.0, 1.0};
  ElementSize size = {50, 30};
  ElementMedia media = {MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};

  // Create 4 notes: 3 in a cycle, 1 separate
  ModelElement *note1 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note 1");
  ModelElement *note2 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note 2");
  ModelElement *note3 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note 3");
  ModelElement *note4 = model_create_element(fixture->model, ELEMENT_NOTE, color, pos, size,
                                             media, NULL, NULL, -1, -1, NULL, 0, "Note 4 (separate)");

  // Create cyclic connections: note1 -> note2 -> note3 -> note1
  ModelElement *conn1 = model_create_element(fixture->model, ELEMENT_CONNECTION, color, pos, size,
                                             media, note1->uuid, note2->uuid, 0, 1, NULL, 0, NULL);
  ModelElement *conn2 = model_create_element(fixture->model, ELEMENT_CONNECTION, color, pos, size,
                                             media, note2->uuid, note3->uuid, 2, 3, NULL, 0, NULL);
  ModelElement *conn3 = model_create_element(fixture->model, ELEMENT_CONNECTION, color, pos, size,
                                             media, note3->uuid, note1->uuid, 0, 2, NULL, 0, NULL);

  // Save everything first
  model_save_elements(fixture->model);

  // Verify all elements are in current space initially
  g_assert_cmpstr(note1->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(note2->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(note3->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(note4->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(conn1->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(conn2->space_uuid, ==, fixture->model->current_space_uuid);
  g_assert_cmpstr(conn3->space_uuid, ==, fixture->model->current_space_uuid);

  // Move note1 to target space - should move the entire cycle
  result = move_element_to_space(fixture->model, note1, target_space_uuid);
  g_assert_cmpint(result, ==, 1);

  // Verify the entire cycle moved to target space
  g_assert_cmpstr(note1->space_uuid, ==, target_space_uuid);
  g_assert_cmpstr(note2->space_uuid, ==, target_space_uuid);
  g_assert_cmpstr(note3->space_uuid, ==, target_space_uuid);
  g_assert_cmpstr(conn1->space_uuid, ==, target_space_uuid);
  g_assert_cmpstr(conn2->space_uuid, ==, target_space_uuid);
  g_assert_cmpstr(conn3->space_uuid, ==, target_space_uuid);

  // Verify the separate note stayed in original space
  g_assert_cmpstr(note4->space_uuid, ==, fixture->model->current_space_uuid);

  // Verify all moved elements are marked as updated
  g_assert_cmpint(note1->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(note2->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(note3->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(conn1->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(conn2->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(conn3->state, ==, MODEL_STATE_UPDATED);

  // Verify the separate note is not marked as updated
  g_assert_cmpint(note4->state, !=, MODEL_STATE_UPDATED);

  // Save changes
  int save_count = model_save_elements(fixture->model);
  g_assert_cmpint(save_count, ==, 6); // 3 notes + 3 connections

  // Cleanup
  g_free(target_space_uuid);
}

static void test_model_get_all_spaces(TestFixture *fixture, gconstpointer user_data) {
  // Create a test space
  char *test_space_uuid = NULL;
  int result = database_create_space(fixture->db, "Model Test Space", fixture->model->current_space_uuid, &test_space_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(test_space_uuid);

  // Get all spaces using model function
  GList *spaces = NULL;
  result = model_get_all_spaces(fixture->model, &spaces);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(spaces);

  // Should have at least 2 spaces (default + test space)
  g_assert_cmpuint(g_list_length(spaces), >=, 2);

  // Verify we can find our test space
  int found_test_space = 0;

  for (GList *iter = spaces; iter != NULL; iter = iter->next) {
    ModelSpaceInfo *space = (ModelSpaceInfo*)iter->data;

    if (g_strcmp0(space->uuid, test_space_uuid) == 0) {
      found_test_space = 1;
      g_assert_cmpstr(space->name, ==, "Model Test Space");
      break;
    }
  }

  g_assert_true(found_test_space);

  // Cleanup
  g_list_free_full(spaces, (GDestroyNotify)model_free_space_info);
  g_free(test_space_uuid);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);

  // Add essential tests
  g_test_add("/model/creation", TestFixture, NULL, test_setup, test_model_creation, test_teardown);
  g_test_add("/model/create-elements", TestFixture, NULL, test_setup, test_create_elements, test_teardown);
  g_test_add("/model/update-elements", TestFixture, NULL, test_setup, test_update_elements, test_teardown);
  g_test_add("/model/save-load-elements", TestFixture, NULL, test_setup, test_save_load_elements, test_teardown);
  g_test_add("/model/delete-element", TestFixture, NULL, test_setup, test_delete_element, test_teardown);
  g_test_add("/model/search", TestFixture, NULL, test_setup, test_search_multiple_spaces, test_teardown);
  g_test_add("/model/cyclic-connection-space-movement", TestFixture, NULL, test_setup, test_cyclic_connection_space_movement, test_teardown);
  g_test_add("/model/get-all-spaces", TestFixture, NULL, test_setup, test_model_get_all_spaces, test_teardown);

  return g_test_run();
}
