#include "model.h"
#include "database.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

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
}

// Test: Create a note element
static void test_create_note(TestFixture *fixture, gconstpointer user_data) {
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Test Note");

  g_assert_nonnull(element);
  g_assert_nonnull(element->uuid);
  g_assert_cmpint(element->type->type, ==, ELEMENT_NOTE);
  g_assert_cmpint(element->position->x, ==, 100);
  g_assert_cmpint(element->position->y, ==, 200);
  g_assert_cmpint(element->size->width, ==, 50);
  g_assert_cmpint(element->size->height, ==, 30);
  g_assert_nonnull(element->text);
  g_assert_cmpstr(element->text->text, ==, "Test Note");
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Check that element was added to model's hash table
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, element->uuid);
  g_assert_nonnull(found);
  g_assert_true(found == element);
}

// Test: Create a paper note element
static void test_create_paper_note(TestFixture *fixture, gconstpointer user_data) {
  ModelElement *element = model_create_paper_note(fixture->model, 300, 400, 1, 80, 40, "Test Paper Note");

  g_assert_nonnull(element);
  g_assert_nonnull(element->uuid);
  g_assert_cmpint(element->type->type, ==, ELEMENT_PAPER_NOTE);
  g_assert_cmpint(element->position->x, ==, 300);
  g_assert_cmpint(element->position->y, ==, 400);
  g_assert_cmpint(element->size->width, ==, 80);
  g_assert_cmpint(element->size->height, ==, 40);
  g_assert_nonnull(element->text);
  g_assert_cmpstr(element->text->text, ==, "Test Paper Note");
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Check that element was added to model's hash table
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, element->uuid);
  g_assert_nonnull(found);
  g_assert_true(found == element);
}

// Test: Create a connection element
static void test_create_connection(TestFixture *fixture, gconstpointer user_data) {
  // First create two elements to connect
  ModelElement *element1 = model_create_note(fixture->model, 100, 100, 1, 50, 30, "Element 1");
  ModelElement *element2 = model_create_note(fixture->model, 200, 200, 1, 50, 30, "Element 2");

  g_assert_nonnull(element1);
  g_assert_nonnull(element2);

  // Create connection between them
  ModelElement *connection = model_create_connection(fixture->model, element1->uuid, element2->uuid, 0, 2, 1);

  g_assert_nonnull(connection);
  g_assert_nonnull(connection->uuid);
  g_assert_cmpint(connection->type->type, ==, ELEMENT_CONNECTION);
  g_assert_nonnull(connection->from_element_uuid);
  g_assert_nonnull(connection->to_element_uuid);
  g_assert_cmpstr(connection->from_element_uuid, ==, element1->uuid);
  g_assert_cmpstr(connection->to_element_uuid, ==, element2->uuid);
  g_assert_cmpint(connection->from_point, ==, 0);
  g_assert_cmpint(connection->to_point, ==, 2);
  g_assert_cmpint(connection->state, ==, MODEL_STATE_NEW);

  // Check that connection was added to model's hash table
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, connection->uuid);
  g_assert_nonnull(found);
  g_assert_true(found == connection);
}

// Test: Create a space element
static void test_create_space_element(TestFixture *fixture, gconstpointer user_data) {
  // Create a target space first
  char *target_space_uuid = NULL;
  if (!database_create_space(fixture->db, "Target Space", NULL, &target_space_uuid)) {
    g_error("Failed to create target space");
  }
  g_assert_nonnull(target_space_uuid);

  // Create space element that links to the target space
  ModelElement *element = model_create_space(fixture->model, "Test space", 500, 600, 1, 100, 100, target_space_uuid);

  g_assert_nonnull(element);
  g_assert_nonnull(element->uuid);
  g_assert_cmpint(element->type->type, ==, ELEMENT_SPACE);
  g_assert_cmpint(element->position->x, ==, 500);
  g_assert_cmpint(element->position->y, ==, 600);
  g_assert_cmpint(element->size->width, ==, 100);
  g_assert_cmpint(element->size->height, ==, 100);
  g_assert_nonnull(element->target_space_uuid);
  g_assert_cmpstr(element->target_space_uuid, ==, target_space_uuid);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Check that element was added to model's hash table
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, element->uuid);
  g_assert_nonnull(found);
  g_assert_true(found == element);

  g_free(target_space_uuid);
}

static void test_model_load_empty_space(TestFixture *fixture, gconstpointer user_data) {
  // Clear the model first
  g_hash_table_remove_all(fixture->model->elements);
  g_hash_table_remove_all(fixture->model->types);
  g_hash_table_remove_all(fixture->model->texts);
  g_hash_table_remove_all(fixture->model->positions);
  g_hash_table_remove_all(fixture->model->sizes);
  g_hash_table_remove_all(fixture->model->colors);

  // Load space (should be empty since we didn't create any elements in DB)
  model_load_space(fixture->model);

  // Verify model is empty
  g_assert_cmpuint(g_hash_table_size(fixture->model->elements), ==, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->types), ==, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->texts), ==, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->positions), ==, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->sizes), ==, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->colors), ==, 0);
}

static void test_model_load_space(TestFixture *fixture, gconstpointer user_data) {
  // Create one of each element type and save to database
  ModelElement *note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Test Note");
  ModelElement *paper_note = model_create_paper_note(fixture->model, 300, 400, 1, 80, 40, "Test Paper Note");

  // Create a target space first
  char *target_space_uuid = NULL;
  database_create_space(fixture->db, "Target Space", NULL, &target_space_uuid);
  ModelElement *space_element = model_create_space(fixture->model, "Test space", 500, 600, 1, 100, 100, target_space_uuid);

  // Create connection between note and paper_note
  ModelElement *connection = model_create_connection(fixture->model, note->uuid, paper_note->uuid, 0, 2, 1);

  g_assert_nonnull(note);
  g_assert_nonnull(paper_note);
  g_assert_nonnull(space_element);
  g_assert_nonnull(connection);

  // Save all elements to database
  g_assert_true(database_create_element(fixture->db, fixture->model->current_space_uuid, note));
  g_assert_true(database_create_element(fixture->db, fixture->model->current_space_uuid, paper_note));
  g_assert_true(database_create_element(fixture->db, fixture->model->current_space_uuid, space_element));
  g_assert_true(database_create_element(fixture->db, fixture->model->current_space_uuid, connection));

  // Remember UUIDs for verification
  char *note_uuid = g_strdup(note->uuid);
  char *paper_note_uuid = g_strdup(paper_note->uuid);
  char *space_element_uuid = g_strdup(space_element->uuid);
  char *connection_uuid = g_strdup(connection->uuid);

  // Clear the model completely
  g_hash_table_remove_all(fixture->model->elements);
  g_hash_table_remove_all(fixture->model->types);
  g_hash_table_remove_all(fixture->model->texts);
  g_hash_table_remove_all(fixture->model->positions);
  g_hash_table_remove_all(fixture->model->sizes);
  g_hash_table_remove_all(fixture->model->colors);

  // Verify model is empty
  g_assert_cmpuint(g_hash_table_size(fixture->model->elements), ==, 0);

  // Load the space from database
  model_load_space(fixture->model);

  // Verify all 4 elements were loaded back
  g_assert_cmpuint(g_hash_table_size(fixture->model->elements), ==, 4);

  // Check that all elements have STATE_SAVED
  ModelElement *loaded_note = g_hash_table_lookup(fixture->model->elements, note_uuid);
  ModelElement *loaded_paper_note = g_hash_table_lookup(fixture->model->elements, paper_note_uuid);
  ModelElement *loaded_space = g_hash_table_lookup(fixture->model->elements, space_element_uuid);
  ModelElement *loaded_connection = g_hash_table_lookup(fixture->model->elements, connection_uuid);

  g_assert_nonnull(loaded_note);
  g_assert_nonnull(loaded_paper_note);
  g_assert_nonnull(loaded_space);
  g_assert_nonnull(loaded_connection);

  g_assert_cmpint(loaded_note->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(loaded_paper_note->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(loaded_space->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(loaded_connection->state, ==, MODEL_STATE_SAVED);

  // Check that types are correct
  g_assert_cmpint(loaded_note->type->type, ==, ELEMENT_NOTE);
  g_assert_cmpint(loaded_paper_note->type->type, ==, ELEMENT_PAPER_NOTE);
  g_assert_cmpint(loaded_space->type->type, ==, ELEMENT_SPACE);
  g_assert_cmpint(loaded_connection->type->type, ==, ELEMENT_CONNECTION);

  // Check that shared resource caches are populated
  g_assert_cmpuint(g_hash_table_size(fixture->model->types), >, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->texts), >, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->positions), >, 0);
  g_assert_cmpuint(g_hash_table_size(fixture->model->sizes), >, 0);

  // Verify connection references are intact
  g_assert_nonnull(loaded_connection->from_element_uuid);
  g_assert_nonnull(loaded_connection->to_element_uuid);
  g_assert_cmpstr(loaded_connection->from_element_uuid, ==, note_uuid);
  g_assert_cmpstr(loaded_connection->to_element_uuid, ==, paper_note_uuid);

  // Verify space element target is intact
  g_assert_nonnull(loaded_space->target_space_uuid);
  g_assert_cmpstr(loaded_space->target_space_uuid, ==, target_space_uuid);

  g_free(target_space_uuid);
  g_free(note_uuid);
  g_free(paper_note_uuid);
  g_free(space_element_uuid);
  g_free(connection_uuid);
}

// Test: Update text content
static void test_model_update_text(TestFixture *fixture, gconstpointer user_data) {
  // Create a note element
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Initial Text");
  g_assert_nonnull(element);

  // Test 1: Update with same text (should return 0)
  int result = model_update_text(fixture->model, element, "Initial Text");
  g_assert_cmpint(result, ==, 0);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW); // State shouldn't change

  // Test 2: Update with different text (should return 1)
  result = model_update_text(fixture->model, element, "Updated Text");
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpstr(element->text->text, ==, "Updated Text");
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW); // Still NEW

  // Test 3: Update with NULL element (should return 0)
  result = model_update_text(fixture->model, NULL, "Test Text");
  g_assert_cmpint(result, ==, 0);

  // Test 4: Update with NULL text (should return 0)
  result = model_update_text(fixture->model, element, NULL);
  g_assert_cmpint(result, ==, 0);

  // Test 5: Update element that has no text reference
  ModelElement *empty_element = g_new0(ModelElement, 1);
  empty_element->uuid = g_strdup("test-uuid");
  result = model_update_text(fixture->model, empty_element, "New Text");
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(empty_element->text);
  g_assert_cmpstr(empty_element->text->text, ==, "New Text");

  // Test 6: Update SAVED element (should change to UPDATED)
  // First save the element
  model_save_elements(fixture->model);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Now update text of saved element
  result = model_update_text(fixture->model, element, "Saved Element Updated Text");
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpstr(element->text->text, ==, "Saved Element Updated Text");
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED); // Should be UPDATED now

  // Cleanup
  g_free(empty_element->uuid);
  if (empty_element->text) {
    g_free(empty_element->text->text);
    g_free(empty_element->text);
  }
  g_free(empty_element);
}

// Test: Update position
static void test_model_update_position(TestFixture *fixture, gconstpointer user_data) {
  // Create a note element
  ModelElement *element = model_create_note(fixture->model, 100, 200, 0, 50, 30, "Test Note");
  g_assert_nonnull(element);

  // Test 1: Update with same position (should return 0)
  int result = model_update_position(fixture->model, element, 100, 200, 0);
  g_assert_cmpint(result, ==, 0);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Test 2: Update with different position (should return 1)
  result = model_update_position(fixture->model, element, 150, 250, 5);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->position->x, ==, 150);
  g_assert_cmpint(element->position->y, ==, 250);
  g_assert_cmpint(element->position->z, ==, 5);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Test 3: Update with NULL element (should return 0)
  result = model_update_position(fixture->model, NULL, 100, 200, 0);
  g_assert_cmpint(result, ==, 0);

  // Test 4: Update element with no position
  ModelElement *empty_element = g_new0(ModelElement, 1);
  empty_element->uuid = g_strdup("test-uuid");
  result = model_update_position(fixture->model, empty_element, 100, 200, 0);
  g_assert_cmpint(result, ==, 1);

  // Test 5: Update SAVED element (should change to UPDATED)
  // First save the element
  model_save_elements(fixture->model);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Now update position of saved element
  result = model_update_position(fixture->model, element, 300, 400, 10);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->position->x, ==, 300);
  g_assert_cmpint(element->position->y, ==, 400);
  g_assert_cmpint(element->position->z, ==, 10);
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED); // Should be UPDATED now

  // Cleanup
  g_free(empty_element->uuid);
  g_free(empty_element);
}

// Test: Update size
static void test_model_update_size(TestFixture *fixture, gconstpointer user_data) {
  // Create a note element
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Test Note");
  g_assert_nonnull(element);

  // Test 1: Update with same size (should return 0)
  int result = model_update_size(fixture->model, element, 50, 30);
  g_assert_cmpint(result, ==, 0);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Test 2: Update with different size (should return 1)
  result = model_update_size(fixture->model, element, 80, 40);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->size->width, ==, 80);
  g_assert_cmpint(element->size->height, ==, 40);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Test 3: Update with NULL element (should return 0)
  result = model_update_size(fixture->model, NULL, 100, 200);
  g_assert_cmpint(result, ==, 0);

  // Test 4: Update element with no size
  ModelElement *empty_element = g_new0(ModelElement, 1);
  empty_element->uuid = g_strdup("test-uuid");
  result = model_update_size(fixture->model, empty_element, 100, 200);
  g_assert_cmpint(result, ==, 1);

  // Test 5: Update SAVED element (should change to UPDATED)
  // First save the element
  model_save_elements(fixture->model);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Now update size of saved element
  result = model_update_size(fixture->model, element, 120, 80);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->size->width, ==, 120);
  g_assert_cmpint(element->size->height, ==, 80);
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED); // Should be UPDATED now

  // Cleanup
  g_free(empty_element->uuid);
  g_free(empty_element);
}

// Test: Delete element
static void test_model_delete_element(TestFixture *fixture, gconstpointer user_data) {
  // Create a note element
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Test Note");
  g_assert_nonnull(element);
  g_assert_cmpint(element->state, ==, MODEL_STATE_NEW);

  // Test 1: Delete element (should return 1)
  int result = model_delete_element(fixture->model, element);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_DELETED);

  // Test 2: Try to delete already deleted element (should return 0)
  result = model_delete_element(fixture->model, element);
  g_assert_cmpint(result, ==, 0);
  g_assert_cmpint(element->state, ==, MODEL_STATE_DELETED);

  // Test 3: Delete with NULL model (should return 0)
  result = model_delete_element(NULL, element);
  g_assert_cmpint(result, ==, 0);

  // Test 4: Delete with NULL element (should return 0)
  result = model_delete_element(fixture->model, NULL);
  g_assert_cmpint(result, ==, 0);

  // Test 5: Delete element with different initial state
  ModelElement *saved_element = model_create_note(fixture->model, 300, 400, 1, 60, 40, "Saved Note");
  g_assert_nonnull(saved_element);
  saved_element->state = MODEL_STATE_SAVED; // Simulate saved element

  result = model_delete_element(fixture->model, saved_element);
  g_assert_cmpint(result, ==, 1);
  g_assert_cmpint(saved_element->state, ==, MODEL_STATE_DELETED);
}

static void test_model_element_fork(TestFixture *fixture, gconstpointer user_data) {
  // Create original elements of each type (they start as NEW state)
  ModelElement *original_note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Original Note");
  ModelElement *original_paper_note = model_create_paper_note(fixture->model, 300, 400, 1, 80, 40, "Original Paper Note");

  // Create a target space for space element
  char *target_space_uuid = NULL;
  database_create_space(fixture->db, "Target Space", NULL, &target_space_uuid);
  ModelElement *original_space = model_create_space(fixture->model, "Space", 500, 600, 1, 100, 100, target_space_uuid);

  // Create connection between note and paper_note
  ModelElement *original_connection = model_create_connection(fixture->model,
                                                              original_note->uuid, original_paper_note->uuid, 0, 2, 1);

  g_assert_nonnull(original_note);
  g_assert_nonnull(original_paper_note);
  g_assert_nonnull(original_space);
  g_assert_nonnull(original_connection);

  // Test 1: Try to fork NEW elements (should return NULL)
  ModelElement *forked_note = model_element_fork(fixture->model, original_note);
  g_assert_null(forked_note); // Should fail because element is NEW

  ModelElement *forked_paper_note = model_element_fork(fixture->model, original_paper_note);
  g_assert_null(forked_paper_note);

  ModelElement *forked_space = model_element_fork(fixture->model, original_space);
  g_assert_null(forked_space);

  ModelElement *forked_connection = model_element_fork(fixture->model, original_connection);
  g_assert_null(forked_connection);

  // Change states to SAVED to allow forking
  original_note->state = MODEL_STATE_SAVED;
  original_paper_note->state = MODEL_STATE_SAVED;
  original_space->state = MODEL_STATE_SAVED;
  original_connection->state = MODEL_STATE_SAVED;

  // Test 2: Fork SAVED elements (should succeed)
  forked_note = model_element_fork(fixture->model, original_note);
  g_assert_nonnull(forked_note);
  g_assert_cmpstr(forked_note->uuid, !=, original_note->uuid);
  g_assert_cmpint(forked_note->type->type, ==, ELEMENT_NOTE);
  g_assert_cmpint(forked_note->state, ==, MODEL_STATE_NEW); // Forked element should be NEW state

  forked_paper_note = model_element_fork(fixture->model, original_paper_note);
  g_assert_nonnull(forked_paper_note);
  g_assert_cmpstr(forked_paper_note->uuid, !=, original_paper_note->uuid);

  forked_space = model_element_fork(fixture->model, original_space);
  g_assert_nonnull(forked_space);
  g_assert_cmpstr(forked_space->uuid, !=, original_space->uuid);

  forked_connection = model_element_fork(fixture->model, original_connection);
  g_assert_nonnull(forked_connection);
  g_assert_cmpstr(forked_connection->uuid, !=, original_connection->uuid);

  // Test 3: Try to fork UPDATED elements (should succeed)
  original_note->state = MODEL_STATE_UPDATED;
  ModelElement *forked_modified = model_element_fork(fixture->model, original_note);
  g_assert_nonnull(forked_modified);
  g_assert_cmpint(forked_modified->state, ==, MODEL_STATE_NEW);

  // Test 4: Fork with NULL parameters (should return NULL)
  ModelElement *null_result = model_element_fork(NULL, original_note);
  g_assert_null(null_result);

  null_result = model_element_fork(fixture->model, NULL);
  g_assert_null(null_result);

  // Test 5: Fork element without type (should return NULL)
  ModelElement element_no_type = {0};
  element_no_type.uuid = g_strdup("test-no-type");
  element_no_type.state = MODEL_STATE_SAVED;
  null_result = model_element_fork(fixture->model, &element_no_type);
  g_assert_null(null_result);
  g_free(element_no_type.uuid);

  // Cleanup
  g_free(target_space_uuid);
}

// Test: Verify element independence after forking (with state restriction)
static void test_model_element_fork_independence(TestFixture *fixture, gconstpointer user_data) {
  // Create original note (NEW state)
  ModelElement *original_note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Original Text");
  g_assert_nonnull(original_note);
  g_assert_cmpint(original_note->state, ==, MODEL_STATE_NEW);

  // Try to fork NEW element (should fail)
  ModelElement *forked_note = model_element_fork(fixture->model, original_note);
  g_assert_null(forked_note);

  // Change state to SAVED to allow forking
  original_note->state = MODEL_STATE_SAVED;

  // Now fork should succeed
  forked_note = model_element_fork(fixture->model, original_note);
  g_assert_nonnull(forked_note);
  g_assert_cmpint(forked_note->state, ==, MODEL_STATE_NEW); // Forked element is NEW

  // Verify independence
  model_update_text(fixture->model, forked_note, "Forked Text Updated");
  g_assert_cmpstr(original_note->text->text, ==, "Original Text"); // Original unchanged
  g_assert_cmpstr(forked_note->text->text, ==, "Forked Text Updated"); // Forked changed

  // Verify state changes are independent
  g_assert_cmpint(original_note->state, ==, MODEL_STATE_SAVED); // Original state unchanged
  g_assert_cmpint(forked_note->state, ==, MODEL_STATE_NEW); // Forked state changed
}

// Test: Clone by text
static void test_model_element_clone_by_text(TestFixture *fixture, gconstpointer user_data) {
  // Create original note (SAVED state)
  ModelElement *original_note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Original Text");
  g_assert_nonnull(original_note);
  original_note->state = MODEL_STATE_SAVED;

  // Remember original text reference count
  int original_ref_count = original_note->text->ref_count;

  // Test 1: Clone by text (should succeed)
  ModelElement *cloned_note = model_element_clone_by_text(fixture->model, original_note);
  g_assert_nonnull(cloned_note);
  g_assert_cmpstr(cloned_note->uuid, !=, original_note->uuid);
  g_assert_true(cloned_note->text == original_note->text); // Same text object
  g_assert_cmpint(cloned_note->text->ref_count, ==, original_ref_count + 1);

  // Test 2: Try to clone NEW element (should fail)
  ModelElement *new_note = model_create_note(fixture->model, 300, 400, 1, 60, 40, "New Note");
  g_assert_nonnull(new_note);
  g_assert_cmpint(new_note->state, ==, MODEL_STATE_NEW);

  ModelElement *cloned_new = model_element_clone_by_text(fixture->model, new_note);
  g_assert_null(cloned_new);

  // Test 3: Try to clone DELETED element (should fail)
  ModelElement *deleted_note = model_create_note(fixture->model, 500, 600, 1, 70, 50, "Deleted Note");
  g_assert_nonnull(deleted_note);
  deleted_note->state = MODEL_STATE_DELETED;

  ModelElement *cloned_deleted = model_element_clone_by_text(fixture->model, deleted_note);
  g_assert_null(cloned_deleted);

  // Test 4: Clone with NULL parameters (should fail)
  ModelElement *null_result = model_element_clone_by_text(NULL, original_note);
  g_assert_null(null_result);

  null_result = model_element_clone_by_text(fixture->model, NULL);
  g_assert_null(null_result);
}

// Test: Clone by size
static void test_model_element_clone_by_size(TestFixture *fixture, gconstpointer user_data) {
  // Create original note (SAVED state)
  ModelElement *original_note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Original Text");
  g_assert_nonnull(original_note);
  original_note->state = MODEL_STATE_SAVED;

  // Remember original size pointer
  ModelSize *original_size = original_note->size;

  // Test 1: Clone by size (should succeed)
  ModelElement *cloned_note = model_element_clone_by_size(fixture->model, original_note);
  g_assert_nonnull(cloned_note);
  g_assert_cmpstr(cloned_note->uuid, !=, original_note->uuid);
  g_assert_true(cloned_note->size == original_size); // Same size object

  // Verify other properties are forked correctly
  g_assert_cmpint(cloned_note->position->x, ==, original_note->position->x);
  g_assert_cmpint(cloned_note->position->y, ==, original_note->position->y);
  g_assert_cmpstr(cloned_note->text->text, ==, original_note->text->text);
  g_assert_true(cloned_note->text != original_note->text); // Different text objects

  // Test 2: Try to clone NEW element (should fail)
  ModelElement *new_note = model_create_note(fixture->model, 300, 400, 1, 60, 40, "New Note");
  g_assert_nonnull(new_note);

  ModelElement *cloned_new = model_element_clone_by_size(fixture->model, new_note);
  g_assert_null(cloned_new);

  // Test 3: Try to clone DELETED element (should fail)
  ModelElement *deleted_note = model_create_note(fixture->model, 500, 600, 1, 70, 50, "Deleted Note");
  g_assert_nonnull(deleted_note);
  deleted_note->state = MODEL_STATE_DELETED;

  ModelElement *cloned_deleted = model_element_clone_by_size(fixture->model, deleted_note);
  g_assert_null(cloned_deleted);

  // Test 4: Clone with NULL parameters (should fail)
  ModelElement *null_result = model_element_clone_by_size(NULL, original_note);
  g_assert_null(null_result);

  null_result = model_element_clone_by_size(fixture->model, NULL);
  g_assert_null(null_result);
}

// Test: Verify text sharing between cloned elements and source
static void test_model_element_clone_text_sharing(TestFixture *fixture, gconstpointer user_data) {
  // Create original note (SAVED state)
  ModelElement *original_note = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Original Text");
  g_assert_nonnull(original_note);
  original_note->state = MODEL_STATE_SAVED;

  // Remember original text reference and count
  ModelText *original_text = original_note->text;
  int original_ref_count = original_text->ref_count;

  // Clone by text multiple times
  ModelElement *cloned_note1 = model_element_clone_by_text(fixture->model, original_note);
  ModelElement *cloned_note2 = model_element_clone_by_text(fixture->model, original_note);
  ModelElement *cloned_note3 = model_element_clone_by_text(fixture->model, original_note);

  g_assert_nonnull(cloned_note1);
  g_assert_nonnull(cloned_note2);
  g_assert_nonnull(cloned_note3);

  // Verify all elements share the same text object
  g_assert_true(original_note->text == original_text);
  g_assert_true(cloned_note1->text == original_text);
  g_assert_true(cloned_note2->text == original_text);
  g_assert_true(cloned_note3->text == original_text);

  // Verify reference count increased correctly
  g_assert_cmpint(original_text->ref_count, ==, original_ref_count + 3);

  // Test 1: Change text on cloned element - all should see the change
  model_update_text(fixture->model, cloned_note1, "Updated Text by Clone 1");

  // All elements should see the same updated text
  g_assert_cmpstr(original_note->text->text, ==, "Updated Text by Clone 1");
  g_assert_cmpstr(cloned_note1->text->text, ==, "Updated Text by Clone 1");
  g_assert_cmpstr(cloned_note2->text->text, ==, "Updated Text by Clone 1");
  g_assert_cmpstr(cloned_note3->text->text, ==, "Updated Text by Clone 1");

  // Test 2: Change text on original element - all should see the change
  model_update_text(fixture->model, original_note, "Updated Text by Original");

  // All elements should see the same updated text
  g_assert_cmpstr(original_note->text->text, ==, "Updated Text by Original");
  g_assert_cmpstr(cloned_note1->text->text, ==, "Updated Text by Original");
  g_assert_cmpstr(cloned_note2->text->text, ==, "Updated Text by Original");
  g_assert_cmpstr(cloned_note3->text->text, ==, "Updated Text by Original");

  // Test 3: Change text on another cloned element - all should see the change
  model_update_text(fixture->model, cloned_note2, "Final Updated Text");

  // All elements should see the same final text
  g_assert_cmpstr(original_note->text->text, ==, "Final Updated Text");
  g_assert_cmpstr(cloned_note1->text->text, ==, "Final Updated Text");
  g_assert_cmpstr(cloned_note2->text->text, ==, "Final Updated Text");
  g_assert_cmpstr(cloned_note3->text->text, ==, "Final Updated Text");

  // Verify reference count remains the same (text object reused, not recreated)
  g_assert_cmpint(original_text->ref_count, ==, original_ref_count + 3);

  // Verify text object pointer remains the same throughout all updates
  g_assert_true(original_note->text == original_text);
  g_assert_true(cloned_note1->text == original_text);
  g_assert_true(cloned_note2->text == original_text);
  g_assert_true(cloned_note3->text == original_text);
}

// Test: Save NEW elements to database
static void test_model_save_new_elements(TestFixture *fixture, gconstpointer user_data) {
  // Create several NEW elements
  ModelElement *note1 = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Note 1");
  ModelElement *note2 = model_create_note(fixture->model, 300, 400, 1, 60, 40, "Note 2");
  ModelElement *paper_note = model_create_paper_note(fixture->model, 500, 600, 1, 80, 50, "Paper Note");

  g_assert_nonnull(note1);
  g_assert_nonnull(note2);
  g_assert_nonnull(paper_note);

  // Verify they are in NEW state
  g_assert_cmpint(note1->state, ==, MODEL_STATE_NEW);
  g_assert_cmpint(note2->state, ==, MODEL_STATE_NEW);
  g_assert_cmpint(paper_note->state, ==, MODEL_STATE_NEW);

  // Verify they don't have database IDs yet
  g_assert_cmpint(note1->type->id, ==, -1);
  g_assert_cmpint(note1->position->id, ==, -1);
  g_assert_cmpint(note1->size->id, ==, -1);
  g_assert_cmpint(note1->text->id, ==, -1);

  // Save elements
  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 3);

  // Verify states changed to SAVED
  g_assert_cmpint(note1->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(note2->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(paper_note->state, ==, MODEL_STATE_SAVED);

  // Verify they now have database IDs
  g_assert_cmpint(note1->type->id, >, 0);
  g_assert_cmpint(note1->position->id, >, 0);
  g_assert_cmpint(note1->size->id, >, 0);
  g_assert_cmpint(note1->text->id, >, 0);

  // Verify shared resources are in model caches
  g_assert_nonnull(g_hash_table_lookup(fixture->model->types, GINT_TO_POINTER(note1->type->id)));
  g_assert_nonnull(g_hash_table_lookup(fixture->model->positions, GINT_TO_POINTER(note1->position->id)));
  g_assert_nonnull(g_hash_table_lookup(fixture->model->sizes, GINT_TO_POINTER(note1->size->id)));
  g_assert_nonnull(g_hash_table_lookup(fixture->model->texts, GINT_TO_POINTER(note1->text->id)));

  // Verify elements are actually in database
  int count = 0;
  const char *sql = "SELECT COUNT(*) FROM elements WHERE space_uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(fixture->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, fixture->model->current_space_uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  g_assert_cmpint(count, ==, 3); // Should have 3 elements in database
}

static void test_model_save_cloned_text_reference(TestFixture *fixture, gconstpointer user_data) {
  // Create and save an initial element
  ModelElement *original = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Shared Text");
  g_assert_nonnull(original);

  // Save the original element
  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(original->state, ==, MODEL_STATE_SAVED);

  // Verify it has database IDs
  g_assert_cmpint(original->type->id, >, 0);
  g_assert_cmpint(original->position->id, >, 0);
  g_assert_cmpint(original->size->id, >, 0);
  g_assert_cmpint(original->text->id, >, 0);
  g_assert_cmpint(original->text->ref_count, ==, 1);

  // Clone the element by text (this should share the text reference)
  ModelElement *cloned = model_element_clone_by_text(fixture->model, original);
  g_assert_nonnull(cloned);
  g_assert_cmpint(cloned->state, ==, MODEL_STATE_NEW);

  // Verify cloned element shares the same text reference
  g_assert_true(cloned->text == original->text);
  g_assert_cmpint(original->text->ref_count, ==, 2);

  // Verify other properties are different (new IDs for position, size, etc.)
  g_assert_cmpint(cloned->type->id, ==, -1); // Should be new
  g_assert_cmpint(cloned->position->id, ==, -1); // Should be new
  g_assert_cmpint(cloned->size->id, ==, -1); // Should be new
  g_assert_cmpint(cloned->text->id, >, 0); // Should be same as original

  // Save the cloned element
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(cloned->state, ==, MODEL_STATE_SAVED);

  // Verify text reference count remains 2 (shared between both elements)
  g_assert_cmpint(original->text->ref_count, ==, 2);
  g_assert_cmpint(cloned->text->ref_count, ==, 2);

  // Verify both elements use the same text ID
  g_assert_cmpint(original->text->id, ==, cloned->text->id);

  // Verify there are exactly 2 elements in database but only 1 text reference
  int element_count = 0;
  int text_ref_count = 0;

  const char *element_sql = "SELECT COUNT(*) FROM elements WHERE space_uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(fixture->db, element_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, fixture->model->current_space_uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      element_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  const char *text_sql = "SELECT COUNT(*) FROM text_refs WHERE id = ?";
  if (sqlite3_prepare_v2(fixture->db, text_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, original->text->id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      text_ref_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  g_assert_cmpint(element_count, ==, 2); // Two elements in database
  g_assert_cmpint(text_ref_count, ==, 1); // But only one text reference

  // Verify both elements reference the same text ID in the database
  int original_text_id = 0;
  int cloned_text_id = 0;

  const char *verify_sql = "SELECT text_id FROM elements WHERE uuid = ?";
  if (sqlite3_prepare_v2(fixture->db, verify_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, original->uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      original_text_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  if (sqlite3_prepare_v2(fixture->db, verify_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, cloned->uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      cloned_text_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  g_assert_cmpint(original_text_id, >, 0);
  g_assert_cmpint(cloned_text_id, >, 0);
  g_assert_cmpint(original_text_id, ==, cloned_text_id); // Both use same text reference
}


static void test_model_save_updated_elements(TestFixture *fixture, gconstpointer user_data) {
  // Create and save an initial element
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Initial Text");
  g_assert_nonnull(element);

  // Save the element (should be in NEW state initially)
  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Remember original values and IDs
  int original_type_id = element->type->id;
  int original_position_id = element->position->id;
  int original_size_id = element->size->id;
  int original_text_id = element->text->id;
  char *original_text = g_strdup(element->text->text);
  int original_x = element->position->x;
  int original_y = element->position->y;
  int original_width = element->size->width;
  int original_height = element->size->height;

  // Verify IDs are positive (saved to database)
  g_assert_cmpint(original_type_id, >, 0);
  g_assert_cmpint(original_position_id, >, 0);
  g_assert_cmpint(original_size_id, >, 0);
  g_assert_cmpint(original_text_id, >, 0);

  // Test 1: Update text (should change state to UPDATED)
  int update_result = model_update_text(fixture->model, element, "Updated Text");
  g_assert_cmpint(update_result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpstr(element->text->text, ==, "Updated Text");

  // Test 2: Update position (should remain UPDATED)
  update_result = model_update_position(fixture->model, element, 300, 400, 5);
  g_assert_cmpint(update_result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(element->position->x, ==, 300);
  g_assert_cmpint(element->position->y, ==, 400);
  g_assert_cmpint(element->position->z, ==, 5);

  // Test 3: Update size (should remain UPDATED)
  update_result = model_update_size(fixture->model, element, 80, 40);
  g_assert_cmpint(update_result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(element->size->width, ==, 80);
  g_assert_cmpint(element->size->height, ==, 40);

  // Save the updated element
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Verify IDs remain the same (should reuse existing database entries)
  g_assert_cmpint(element->type->id, ==, original_type_id);
  g_assert_cmpint(element->position->id, ==, original_position_id);
  g_assert_cmpint(element->size->id, ==, original_size_id);
  g_assert_cmpint(element->text->id, ==, original_text_id);

  // Verify the updates were actually saved to database
  int db_x = 0, db_y = 0, db_z = 0;
  int db_width = 0, db_height = 0;
  char *db_text = NULL;

  const char *sql = "SELECT p.x, p.y, p.z, s.width, s.height, t.text "
    "FROM elements e "
    "JOIN position_refs p ON e.position_id = p.id "
    "JOIN size_refs s ON e.size_id = s.id "
    "JOIN text_refs t ON e.text_id = t.id "
    "WHERE e.uuid = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(fixture->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, element->uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      db_x = sqlite3_column_int(stmt, 0);
      db_y = sqlite3_column_int(stmt, 1);
      db_z = sqlite3_column_int(stmt, 2);
      db_width = sqlite3_column_int(stmt, 3);
      db_height = sqlite3_column_int(stmt, 4);
      db_text = g_strdup((char *)sqlite3_column_text(stmt, 5));
    }
    sqlite3_finalize(stmt);
  }

  // Verify database contains the updated values
  g_assert_cmpint(db_x, ==, 300);
  g_assert_cmpint(db_y, ==, 400);
  g_assert_cmpint(db_z, ==, 5);
  g_assert_cmpint(db_width, ==, 80);
  g_assert_cmpint(db_height, ==, 40);
  g_assert_cmpstr(db_text, ==, "Updated Text");

  // Verify database does NOT contain the original values
  g_assert_cmpint(db_x, !=, original_x);
  g_assert_cmpint(db_y, !=, original_y);
  g_assert_cmpint(db_width, !=, original_width);
  g_assert_cmpint(db_height, !=, original_height);
  g_assert_cmpstr(db_text, !=, original_text);

  // Test 4: Multiple UPDATED elements
  ModelElement *element2 = model_create_note(fixture->model, 500, 600, 1, 70, 50, "Element 2");
  g_assert_nonnull(element2);

  // Save second element
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element2->state, ==, MODEL_STATE_SAVED);

  // Update both elements
  model_update_text(fixture->model, element, "Final Text 1");
  model_update_text(fixture->model, element2, "Final Text 2");

  g_assert_cmpint(element->state, ==, MODEL_STATE_UPDATED);
  g_assert_cmpint(element2->state, ==, MODEL_STATE_UPDATED);

  // Save both updated elements
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 2);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(element2->state, ==, MODEL_STATE_SAVED);
}

// Test: Delete element from database
static void test_model_delete_element_from_database(TestFixture *fixture, gconstpointer user_data) {
  // Create and save an element to database
  ModelElement *element = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Test Note");
  g_assert_nonnull(element);
  g_assert_nonnull(element->uuid);

  // Save UUID before any operations that might free it
  char *element_uuid = g_strdup(element->uuid);

  // Save element to database
  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_SAVED);

  // Verify element exists in database
  ModelElement *db_element = NULL;
  int exists = database_read_element(fixture->db, element->uuid, &db_element);
  g_assert_true(exists);
  g_assert_nonnull(db_element);
  if (db_element) {
    model_element_free(db_element);
  }

  // Mark element for deletion
  int delete_result = model_delete_element(fixture->model, element);
  g_assert_cmpint(delete_result, ==, 1);
  g_assert_cmpint(element->state, ==, MODEL_STATE_DELETED);

  // Save changes (this should delete from database)
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);

  // Verify element is removed from model's elements table
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, element_uuid);
  g_assert_null(found);

  // Verify element no longer exists in database
  db_element = NULL;
  exists = database_read_element(fixture->db, element_uuid, &db_element);
  g_assert_true(exists); // Function should succeed
  g_assert_null(db_element); // But element should be NULL (not found)

  // Clean up
  g_free(element_uuid);
}

// Test: Comprehensive element deletion with resource cleanup
static void test_model_delete_element_comprehensive(TestFixture *fixture, gconstpointer user_data) {
  // Create and save multiple elements that share resources
  ModelElement *element1 = model_create_note(fixture->model, 100, 200, 1, 50, 30, "Shared Text");
  ModelElement *element3 = model_create_note(fixture->model, 500, 600, 1, 50, 30, "Different Text");

  g_assert_nonnull(element1);
  g_assert_nonnull(element3);

  int saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 2);

  ModelElement *element2 = model_element_clone_by_text(fixture->model, element1);
  g_assert_nonnull(element2);

  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);
  g_assert_cmpint(element1->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(element2->state, ==, MODEL_STATE_SAVED);
  g_assert_cmpint(element3->state, ==, MODEL_STATE_SAVED);

  // Verify elements share text reference (element1 and element2 should share same text object)
  g_assert_true(element1->text == element2->text);
  g_assert_cmpint(element1->text->ref_count, ==, 2); // Shared by element1 and element2
  g_assert_cmpint(element3->text->ref_count, ==, 1); // element3 has its own text

  // Remember UUIDs and text IDs for verification
  char *uuid1 = g_strdup(element1->uuid);
  char *uuid2 = g_strdup(element2->uuid);
  char *uuid3 = g_strdup(element3->uuid);
  int shared_text_id = element1->text->id;
  int unique_text_id = element3->text->id;

  // Test 1: Delete element that shares text (should decrement ref_count but not remove text)
  int delete_result = model_delete_element(fixture->model, element1);
  g_assert_cmpint(delete_result, ==, 1);
  g_assert_cmpint(element1->state, ==, MODEL_STATE_DELETED);
  g_assert_cmpint(element1->text->ref_count, ==, 1); // ref_count decreased

  // Save deletion (should remove from database but keep shared text)
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);

  // Verify element1 is removed from model
  ModelElement *found = g_hash_table_lookup(fixture->model->elements, uuid1);
  g_assert_null(found);

  // Verify element1 is removed from database
  ModelElement *db_element = NULL;
  int exists = database_read_element(fixture->db, uuid1, &db_element);
  g_assert_true(exists);
  g_assert_null(db_element);

  // Verify shared text still exists in database (because element2 still uses it)
  int text_ref_count = 0;
  const char *text_sql = "SELECT ref_count FROM text_refs WHERE id = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(fixture->db, text_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, shared_text_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      text_ref_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }
  g_assert_cmpint(text_ref_count, ==, 1); // Should still exist with ref_count = 1

  // Test 2: Delete element2 (last user of shared text - should allow text cleanup)
  delete_result = model_delete_element(fixture->model, element2);
  g_assert_cmpint(delete_result, ==, 1);
  g_assert_cmpint(element2->state, ==, MODEL_STATE_DELETED);
  g_assert_cmpint(element2->text->ref_count, ==, 0); // ref_count now 0

  // Save deletion
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);

  // Verify element2 is removed
  found = g_hash_table_lookup(fixture->model->elements, uuid2);
  g_assert_null(found);

  // Verify shared text might be removed from database (ref_count = 0)
  // Note: cleanup_database_references might not run immediately, so we'll check later

  // Test 3: Delete element3 (unique text - should remove text completely)
  delete_result = model_delete_element(fixture->model, element3);
  g_assert_cmpint(delete_result, ==, 1);
  g_assert_cmpint(element3->state, ==, MODEL_STATE_DELETED);
  g_assert_cmpint(element3->text->ref_count, ==, 0); // ref_count now 0

  // Save deletion
  saved_count = model_save_elements(fixture->model);
  g_assert_cmpint(saved_count, ==, 1);

  // Verify element3 is removed
  found = g_hash_table_lookup(fixture->model->elements, uuid3);
  g_assert_null(found);

  element1 = element2 = element3 = NULL;

  // Verify all elements are gone from database
  int element_count = 0;
  const char *element_sql = "SELECT COUNT(*) FROM elements WHERE space_uuid = ?";
  if (sqlite3_prepare_v2(fixture->db, element_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, fixture->model->current_space_uuid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      element_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }
  g_assert_cmpint(element_count, ==, 0); // No elements left

  // Test 4: Verify database cleanup (run cleanup manually to ensure orphaned references are removed)
  int cleanup_count = cleanup_database_references(fixture->db);
  g_assert_cmpint(cleanup_count, >=, 0); // Should clean up references with ref_count < 1

  // Verify both text references are gone from database (ref_count should be 0 for both)
  int shared_text_exists = 0;
  int unique_text_exists = 0;

  const char *check_sql = "SELECT COUNT(*) FROM text_refs WHERE id = ?";
  if (sqlite3_prepare_v2(fixture->db, check_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, shared_text_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      shared_text_exists = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  if (sqlite3_prepare_v2(fixture->db, check_sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, unique_text_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      unique_text_exists = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  // Both text references should be removed (ref_count reached 0)
  g_assert_cmpint(shared_text_exists, ==, 0);
  g_assert_cmpint(unique_text_exists, ==, 0);

  // Test 5: Try to delete already deleted elements (should fail)
  delete_result = model_delete_element(fixture->model, element1);
  g_assert_cmpint(delete_result, ==, 0);
  delete_result = model_delete_element(fixture->model, element2);
  g_assert_cmpint(delete_result, ==, 0);
  delete_result = model_delete_element(fixture->model, element3);
  g_assert_cmpint(delete_result, ==, 0);

  // Test 6: Try to delete NULL element (should fail)
  delete_result = model_delete_element(fixture->model, NULL);
  g_assert_cmpint(delete_result, ==, 0);

  // Test 7: Try to delete with NULL model (should fail)
  delete_result = model_delete_element(NULL, element1);
  g_assert_cmpint(delete_result, ==, 0);

  // Cleanup
  g_free(uuid1);
  g_free(uuid2);
  g_free(uuid3);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);

  // Add tests
  g_test_add("/model/creation", TestFixture, NULL, test_setup, test_model_creation, test_teardown);
  g_test_add("/model/create-note", TestFixture, NULL, test_setup, test_create_note, test_teardown);
  g_test_add("/model/create-paper-note", TestFixture, NULL, test_setup, test_create_paper_note, test_teardown);
  g_test_add("/model/create-connection", TestFixture, NULL, test_setup, test_create_connection, test_teardown);
  g_test_add("/model/create-space-element", TestFixture, NULL, test_setup, test_create_space_element, test_teardown);
  g_test_add("/model/load-empty-space", TestFixture, NULL, test_setup, test_model_load_empty_space, test_teardown);
  g_test_add("/model/load-space", TestFixture, NULL, test_setup, test_model_load_space, test_teardown);
  g_test_add("/model/update-text", TestFixture, NULL, test_setup, test_model_update_text, test_teardown);
  g_test_add("/model/update-position", TestFixture, NULL, test_setup, test_model_update_position, test_teardown);
  g_test_add("/model/update-size", TestFixture, NULL, test_setup, test_model_update_size, test_teardown);
  g_test_add("/model/delete-element", TestFixture, NULL, test_setup, test_model_delete_element, test_teardown);
  g_test_add("/model/element", TestFixture, NULL, test_setup, test_model_element_fork, test_teardown);
  g_test_add("/model/element-independence", TestFixture, NULL, test_setup, test_model_element_fork_independence, test_teardown);
  g_test_add("/model/clone-by-text", TestFixture, NULL, test_setup, test_model_element_clone_by_text, test_teardown);
  g_test_add("/model/clone-by-size", TestFixture, NULL, test_setup, test_model_element_clone_by_size, test_teardown);
  g_test_add("/model/clone-text-sharing", TestFixture, NULL, test_setup, test_model_element_clone_text_sharing, test_teardown);
  g_test_add("/model/save-new-elements", TestFixture, NULL, test_setup, test_model_save_new_elements, test_teardown);
  g_test_add("/model/save-new-elements-cloned", TestFixture, NULL, test_setup, test_model_save_cloned_text_reference, test_teardown);
  g_test_add("/model/save-new-elements-updated", TestFixture, NULL, test_setup, test_model_save_updated_elements, test_teardown);
  g_test_add("/model/delete-from-db", TestFixture, NULL, test_setup, test_model_delete_element_from_database, test_teardown);
  g_test_add("/model/delete-from-db-shared", TestFixture, NULL, test_setup, test_model_delete_element_comprehensive, test_teardown);

  return g_test_run();
}
