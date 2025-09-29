#include "canvas_space_tree.h"
#include "canvas_core.h"
#include "model.h"
#include "database.h"
#include "canvas.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test database file
#define TEST_DB_FILE "test_space_tree.db"

// Test fixture
typedef struct {
  Model *model;
  CanvasData *canvas_data;
  SpaceTreeView *tree_view;
  sqlite3 *db;
} TestFixture;

// Setup function
static void test_setup(TestFixture *fixture, gconstpointer user_data) {
  // Remove any existing test database
  remove(TEST_DB_FILE);

  // Create model (this will initialize the database)
  fixture->model = model_new_with_file(TEST_DB_FILE);
  fixture->db = fixture->model->db;

  // Create minimal canvas data structure for tree view
  fixture->canvas_data = g_new0(CanvasData, 1);
  fixture->canvas_data->model = fixture->model;
  fixture->canvas_data->hidden_elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  // Initialize tree view (we'll create it in individual tests if needed)
  fixture->tree_view = NULL;
}

// Teardown function
static void test_teardown(TestFixture *fixture, gconstpointer user_data) {
  if (fixture->tree_view) {
    space_tree_view_free(fixture->tree_view);
  }
  if (fixture->canvas_data) {
    canvas_data_free(fixture->canvas_data);
    fixture->canvas_data = NULL;
  }
  if (fixture->model) {
    model_free(fixture->model);
  }
  if (fixture->db) {
    sqlite3_close(fixture->db);
  }
  remove(TEST_DB_FILE);
}

// Helper function to create a basic ElementConfig
static ElementConfig create_basic_config(ElementType type, const char* text) {
  ElementConfig config = {0};
  config.type = type;
  config.bg_color = (ElementColor){1.0, 1.0, 1.0, 1.0};
  config.position = (ElementPosition){100, 200, 1};
  config.size = (ElementSize){50, 30};
  config.media = (ElementMedia){MEDIA_TYPE_NONE, NULL, 0, NULL, 0, 0};
  config.text = (ElementText){g_strdup(text), (ElementColor){0.0, 0.0, 0.0, 1.0}, g_strdup("Ubuntu Mono 12")};
  config.connection = (ElementConnection){NULL, NULL, NULL, NULL, -1, -1, 0, 0};
  config.drawing = (ElementDrawing){NULL, 0};
  return config;
}


// Helper function to count tree nodes of a specific type
static int count_tree_nodes_of_type(GtkTreeModel *model, GtkTreeIter *parent, const char *type) {
  int count = 0;
  GtkTreeIter iter;
  gboolean valid;

  if (parent) {
    valid = gtk_tree_model_iter_children(model, &iter, parent);
  } else {
    valid = gtk_tree_model_get_iter_first(model, &iter);
  }

  while (valid) {
    gchar *node_type = NULL;
    gtk_tree_model_get(model, &iter, SPACE_TREE_COL_TYPE, &node_type, -1);

    if (node_type && g_strcmp0(node_type, type) == 0) {
      count++;
    }

    // Recursively count in children
    count += count_tree_nodes_of_type(model, &iter, type);

    g_free(node_type);
    valid = gtk_tree_model_iter_next(model, &iter);
  }

  return count;
}

// Helper function to find tree node by UUID
static gboolean find_tree_node_by_uuid(GtkTreeModel *model, GtkTreeIter *start_iter,
                                       const char *uuid, GtkTreeIter *found_iter) {
  GtkTreeIter iter;
  gboolean valid;

  if (start_iter) {
    valid = gtk_tree_model_iter_children(model, &iter, start_iter);
  } else {
    valid = gtk_tree_model_get_iter_first(model, &iter);
  }

  while (valid) {
    gchar *node_uuid = NULL;
    gtk_tree_model_get(model, &iter, SPACE_TREE_COL_UUID, &node_uuid, -1);

    if (node_uuid && g_strcmp0(node_uuid, uuid) == 0) {
      *found_iter = iter;
      g_free(node_uuid);
      return TRUE;
    }
    g_free(node_uuid);

    // Search recursively in children
    if (find_tree_node_by_uuid(model, &iter, uuid, found_iter)) {
      return TRUE;
    }

    valid = gtk_tree_model_iter_next(model, &iter);
  }

  return FALSE;
}

static void flush_gtk_events(void) {
  while (g_main_context_pending(NULL)) {
    g_main_context_iteration(NULL, FALSE);
  }
}

static void assert_no_duplicate_root_spaces(GtkTreeModel *model) {
  GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

  while (valid) {
    gchar *uuid = NULL;
    gtk_tree_model_get(model, &iter, SPACE_TREE_COL_UUID, &uuid, -1);

    if (uuid) {
      if (g_hash_table_contains(seen, uuid)) {
        g_hash_table_destroy(seen);
        g_free(uuid);
        g_assert_true(FALSE && "Duplicate root space detected");
      }
      g_hash_table_add(seen, uuid); // takes ownership
    }

    valid = gtk_tree_model_iter_next(model, &iter);
  }

  g_hash_table_destroy(seen);
}

// Test: Create 3 spaces with one element each and 1 element on root, make second space active
// Test the actual SpaceTreeView algorithm and GTK tree building
static void test_tree_three_spaces_second_active(TestFixture *fixture, gconstpointer user_data) {
  // Get the default root space UUID
  char *root_space_uuid = g_strdup(fixture->model->current_space_uuid);
  g_assert_nonnull(root_space_uuid);

  // Create 3 child spaces
  char *space1_uuid = NULL, *space2_uuid = NULL, *space3_uuid = NULL;
  int result;

  result = database_create_space(fixture->db, "Space 1", root_space_uuid, &space1_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space1_uuid);

  result = database_create_space(fixture->db, "Space 2", root_space_uuid, &space2_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space2_uuid);

  result = database_create_space(fixture->db, "Space 3", root_space_uuid, &space3_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space3_uuid);

  // Create an element in the root space
  ElementConfig config_root = create_basic_config(ELEMENT_NOTE, "Element in Root Space");
  ModelElement *element_root = model_create_element(fixture->model, config_root);
  g_assert_nonnull(element_root);

  // Create elements in default space, then move them
  ElementConfig config1 = create_basic_config(ELEMENT_NOTE, "Element in Space 1");
  ModelElement *element1 = model_create_element(fixture->model, config1);
  g_assert_nonnull(element1);

  ElementConfig config2 = create_basic_config(ELEMENT_NOTE, "Element in Space 2");
  ModelElement *element2 = model_create_element(fixture->model, config2);
  g_assert_nonnull(element2);

  ElementConfig config3 = create_basic_config(ELEMENT_NOTE, "Element in Space 3");
  ModelElement *element3 = model_create_element(fixture->model, config3);
  g_assert_nonnull(element3);

  // Move elements to their respective spaces
  int move_result;
  move_result = move_element_to_space(fixture->model, element1, space1_uuid);
  g_assert_cmpint(move_result, ==, 1);

  move_result = move_element_to_space(fixture->model, element2, space2_uuid);
  g_assert_cmpint(move_result, ==, 1);

  move_result = move_element_to_space(fixture->model, element3, space3_uuid);
  g_assert_cmpint(move_result, ==, 1);

  // Save all elements
  model_save_elements(fixture->model);

  // Make space 2 the current/active space
  g_free(fixture->model->current_space_uuid);
  fixture->model->current_space_uuid = g_strdup(space2_uuid);

  // NOW TEST THE ACTUAL CANVAS SPACE TREE ALGORITHM!

  // Create the SpaceTreeView - this will run the tree building algorithm
  fixture->tree_view = space_tree_view_new(fixture->canvas_data);
  g_assert_nonnull(fixture->tree_view);

  GtkTreeModel *tree_model = GTK_TREE_MODEL(fixture->tree_view->tree_store);
  g_assert_nonnull(tree_model);

  // Test 1: Verify tree structure contains EXACTLY the expected nodes
  int space_count = count_tree_nodes_of_type(tree_model, NULL, "space");
  int element_count = count_tree_nodes_of_type(tree_model, NULL, "element");

  // Expected: 1 root + 3 child spaces = 4 spaces total (NO DUPLICATES)
  g_assert_cmpint(space_count, ==, 4);

  // Expected: Elements from the root space and the active space should be loaded
  g_assert_cmpint(element_count, ==, 2);

  // Test 2: Find space 2 in the tree and verify it's marked as current
  GtkTreeIter space2_iter;
  gboolean found_space2 = find_tree_node_by_uuid(tree_model, NULL, space2_uuid, &space2_iter);
  g_assert_true(found_space2);

  gboolean is_current = FALSE;
  gtk_tree_model_get(tree_model, &space2_iter, SPACE_TREE_COL_IS_CURRENT, &is_current, -1);
  g_assert_true(is_current);

  // Test 3: Verify ONLY element from space2 is visible in tree
  GtkTreeIter element2_iter;
  gboolean found_element2 = find_tree_node_by_uuid(tree_model, NULL, element2->uuid, &element2_iter);
  g_assert_true(found_element2);

  // Test 3.1: Verify elements from other spaces are NOT in tree
  GtkTreeIter element1_iter, element3_iter, element_root_iter;
  gboolean found_element1 = find_tree_node_by_uuid(tree_model, NULL, element1->uuid, &element1_iter);
  gboolean found_element3 = find_tree_node_by_uuid(tree_model, NULL, element3->uuid, &element3_iter);
  gboolean found_element_root = find_tree_node_by_uuid(tree_model, NULL, element_root->uuid, &element_root_iter);

  g_assert_false(found_element1);
  g_assert_false(found_element3);
  g_assert_true(found_element_root);

  // --- Test 4: Switch to root space and verify element is visible ---
  g_free(fixture->model->current_space_uuid);
  fixture->model->current_space_uuid = g_strdup(root_space_uuid);
  space_tree_view_refresh(fixture->tree_view);

  // Now, only the root element should be visible
  element_count = count_tree_nodes_of_type(tree_model, NULL, "element");
  g_assert_cmpint(element_count, ==, 1);

  found_element_root = find_tree_node_by_uuid(tree_model, NULL, element_root->uuid, &element_root_iter);
  g_assert_true(found_element_root);

  // Elements from other spaces should not be visible
  found_element1 = find_tree_node_by_uuid(tree_model, NULL, element1->uuid, &element1_iter);
  found_element2 = find_tree_node_by_uuid(tree_model, NULL, element2->uuid, &element2_iter);
  found_element3 = find_tree_node_by_uuid(tree_model, NULL, element3->uuid, &element3_iter);
  g_assert_false(found_element1);
  g_assert_false(found_element2);
  g_assert_false(found_element3);


  // Test 4: Verify root space exists and is properly structured
  GtkTreeIter root_iter;
  gboolean found_root = find_tree_node_by_uuid(tree_model, NULL, root_space_uuid, &root_iter);
  g_assert_true(found_root);

  // Refresh invalidates previous iterators, reacquire space2 iterator before expansion checks
  found_space2 = find_tree_node_by_uuid(tree_model, NULL, space2_uuid, &space2_iter);
  g_assert_true(found_space2);

  // Cleanup
  g_free(root_space_uuid);
  g_free(space1_uuid);
  g_free(space2_uuid);
  g_free(space3_uuid);
  g_free(config_root.text.text);
  g_free(config_root.text.font_description);
  g_free(config1.text.text);
  g_free(config1.text.font_description);
  g_free(config2.text.text);
  g_free(config2.text.font_description);
  g_free(config3.text.text);
  g_free(config3.text.font_description);
}

// Test: Collapsing the active space switches to parent and reloads child content on reselect
static void test_tree_collapse_active_space_with_child(TestFixture *fixture, gconstpointer user_data) {
  char *root_space_uuid = g_strdup(fixture->model->current_space_uuid);
  g_assert_nonnull(root_space_uuid);

  char *space1_uuid = NULL, *space2_uuid = NULL, *space3_uuid = NULL, *space2_child_uuid = NULL;
  int result;

  result = database_create_space(fixture->db, "Space 1", root_space_uuid, &space1_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space1_uuid);

  result = database_create_space(fixture->db, "Space 2", root_space_uuid, &space2_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space2_uuid);

  result = database_create_space(fixture->db, "Space 3", root_space_uuid, &space3_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space3_uuid);

  result = database_create_space(fixture->db, "Space 2 Child", space2_uuid, &space2_child_uuid);
  g_assert_cmpint(result, ==, 1);
  g_assert_nonnull(space2_child_uuid);

  ElementConfig config_root = create_basic_config(ELEMENT_NOTE, "Root Element");
  ModelElement *element_root = model_create_element(fixture->model, config_root);
  g_assert_nonnull(element_root);
  char *element_root_uuid = g_strdup(element_root->uuid);

  ElementConfig config2 = create_basic_config(ELEMENT_NOTE, "Space 2 Element");
  ModelElement *element2 = model_create_element(fixture->model, config2);
  g_assert_nonnull(element2);
  char *element2_uuid = g_strdup(element2->uuid);

  g_assert_cmpint(move_element_to_space(fixture->model, element2, space2_uuid), ==, 1);

  model_save_elements(fixture->model);

  g_free(fixture->model->current_space_uuid);
  fixture->model->current_space_uuid = g_strdup(space2_uuid);

  fixture->tree_view = space_tree_view_new(fixture->canvas_data);
  g_assert_nonnull(fixture->tree_view);

  GtkTreeModel *tree_model = GTK_TREE_MODEL(fixture->tree_view->tree_store);
  g_assert_nonnull(tree_model);

  int space_count = count_tree_nodes_of_type(tree_model, NULL, "space");
  g_assert_cmpint(space_count, ==, 5);

  int element_count = count_tree_nodes_of_type(tree_model, NULL, "element");
  g_assert_cmpint(element_count, ==, 2);

  GtkTreeIter space2_iter;
  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space2_uuid, &space2_iter));

  GtkTreeView *tree_view_widget = GTK_TREE_VIEW(fixture->tree_view->tree_view);
  GtkTreePath *space2_path = gtk_tree_model_get_path(tree_model, &space2_iter);
  gtk_tree_view_collapse_row(tree_view_widget, space2_path);
  gtk_tree_path_free(space2_path);
  flush_gtk_events();

  g_assert_cmpstr(fixture->model->current_space_uuid, ==, root_space_uuid);

  element_count = count_tree_nodes_of_type(tree_model, NULL, "element");
  g_assert_cmpint(element_count, ==, 1);

  GtkTreeIter iter_tmp;
  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, element_root_uuid, &iter_tmp));
  g_assert_false(find_tree_node_by_uuid(tree_model, NULL, element2_uuid, &iter_tmp));

  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space2_uuid, &space2_iter));
  gtk_tree_selection_select_iter(fixture->tree_view->selection, &space2_iter);
  flush_gtk_events();

  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space2_uuid, &space2_iter));

  g_assert_cmpstr(fixture->model->current_space_uuid, ==, space2_uuid);

  space2_path = gtk_tree_model_get_path(tree_model, &space2_iter);
  gtk_tree_view_expand_row(tree_view_widget, space2_path, FALSE);
  gtk_tree_path_free(space2_path);
  flush_gtk_events();

  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, element2_uuid, &iter_tmp));

  g_free(root_space_uuid);
  g_free(space1_uuid);
  g_free(space2_uuid);
  g_free(space3_uuid);
  g_free(space2_child_uuid);
  g_free(config_root.text.text);
  g_free(config_root.text.font_description);
  g_free(config2.text.text);
  g_free(config2.text.font_description);
  g_free(element_root_uuid);
  g_free(element2_uuid);
}

// Test: Collapsing a grandparent space while deepest descendant is active must not crash
static void test_tree_collapse_grandparent_of_active_space(TestFixture *fixture, gconstpointer user_data) {
  char *root_space_uuid = g_strdup(fixture->model->current_space_uuid);
  g_assert_nonnull(root_space_uuid);

  char *space_a_uuid = NULL;
  char *space_b_uuid = NULL;
  char *space_c_uuid = NULL;
  char *space_d_uuid = NULL;

  g_assert_cmpint(database_create_space(fixture->db, "Space A", root_space_uuid, &space_a_uuid), ==, 1);
  g_assert_nonnull(space_a_uuid);

  g_assert_cmpint(database_create_space(fixture->db, "Space B", space_a_uuid, &space_b_uuid), ==, 1);
  g_assert_nonnull(space_b_uuid);

  g_assert_cmpint(database_create_space(fixture->db, "Space C", space_b_uuid, &space_c_uuid), ==, 1);
  g_assert_nonnull(space_c_uuid);

  g_assert_cmpint(database_create_space(fixture->db, "Space D", space_c_uuid, &space_d_uuid), ==, 1);
  g_assert_nonnull(space_d_uuid);

  g_free(fixture->model->current_space_uuid);
  fixture->model->current_space_uuid = g_strdup(space_d_uuid);

  fixture->tree_view = space_tree_view_new(fixture->canvas_data);
  g_assert_nonnull(fixture->tree_view);

  GtkTreeModel *tree_model = GTK_TREE_MODEL(fixture->tree_view->tree_store);
  g_assert_nonnull(tree_model);

  flush_gtk_events();

  GtkTreeIter space_d_iter;
  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space_d_uuid, &space_d_iter));

  GtkTreeView *tree_view_widget = GTK_TREE_VIEW(fixture->tree_view->tree_view);
  GtkTreePath *space_d_path = gtk_tree_model_get_path(tree_model, &space_d_iter);
  gtk_tree_view_expand_to_path(tree_view_widget, space_d_path);
  gtk_tree_path_free(space_d_path);
  flush_gtk_events();

  GtkTreeIter space_b_iter;
  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space_b_uuid, &space_b_iter));

  GtkTreePath *space_b_path = gtk_tree_model_get_path(tree_model, &space_b_iter);
  gtk_tree_view_expand_row(tree_view_widget, space_b_path, FALSE);
  flush_gtk_events();

  g_assert_cmpstr(fixture->model->current_space_uuid, ==, space_d_uuid);

  gtk_tree_view_collapse_row(tree_view_widget, space_b_path);
  gtk_tree_path_free(space_b_path);
  flush_gtk_events();

  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, space_b_uuid, &space_b_iter));
  g_assert_false(find_tree_node_by_uuid(tree_model, NULL, space_d_uuid, &space_d_iter));

  g_free(root_space_uuid);
  g_free(space_a_uuid);
  g_free(space_b_uuid);
  g_free(space_c_uuid);
  g_free(space_d_uuid);
}

// Test: Repeated refreshes and toggles must not create duplicate root spaces
static void test_tree_toggle_refresh_no_duplicates(TestFixture *fixture, gconstpointer user_data) {
  char *root_space_uuid = g_strdup(fixture->model->current_space_uuid);
  g_assert_nonnull(root_space_uuid);

  char *child_uuid = NULL;
  g_assert_cmpint(database_create_space(fixture->db, "Child", root_space_uuid, &child_uuid), ==, 1);
  g_assert_nonnull(child_uuid);

  ElementConfig config_root = create_basic_config(ELEMENT_NOTE, "Root Note");
  ModelElement *element_root = model_create_element(fixture->model, config_root);
  g_assert_nonnull(element_root);
  g_assert_cmpint(model_save_elements(fixture->model), >=, 1);

  fixture->tree_view = space_tree_view_new(fixture->canvas_data);
  GtkTreeModel *tree_model = GTK_TREE_MODEL(fixture->tree_view->tree_store);
  g_assert_nonnull(tree_model);

  assert_no_duplicate_root_spaces(tree_model);

  for (int i = 0; i < 5; i++) {
    space_tree_view_refresh(fixture->tree_view);
    flush_gtk_events();
    assert_no_duplicate_root_spaces(tree_model);
  }

  space_tree_view_schedule_refresh(fixture->tree_view);
  flush_gtk_events();
  assert_no_duplicate_root_spaces(tree_model);

  GtkTreeIter root_iter;
  g_assert_true(find_tree_node_by_uuid(tree_model, NULL, root_space_uuid, &root_iter));
  GtkTreePath *root_path = gtk_tree_model_get_path(tree_model, &root_iter);
  gtk_tree_view_collapse_row(GTK_TREE_VIEW(fixture->tree_view->tree_view), root_path);
  flush_gtk_events();
  assert_no_duplicate_root_spaces(tree_model);
  gtk_tree_view_expand_row(GTK_TREE_VIEW(fixture->tree_view->tree_view), root_path, FALSE);
  gtk_tree_path_free(root_path);
  flush_gtk_events();
  assert_no_duplicate_root_spaces(tree_model);

  g_free(root_space_uuid);
  g_free(child_uuid);
  g_free(config_root.text.text);
  g_free(config_root.text.font_description);
}


int main(int argc, char *argv[]) {
  // Initialize GTK for tree view operations
  gtk_init();
  g_test_init(&argc, &argv, NULL);

  // Add the test case
  g_test_add("/space-tree/three-spaces-second-active", TestFixture, NULL,
             test_setup, test_tree_three_spaces_second_active, test_teardown);
  g_test_add("/space-tree/collapse-active-space-with-child", TestFixture, NULL,
             test_setup, test_tree_collapse_active_space_with_child, test_teardown);
  g_test_add("/space-tree/collapse-grandparent-of-active-space", TestFixture, NULL,
             test_setup, test_tree_collapse_grandparent_of_active_space, test_teardown);
  g_test_add("/space-tree/toggle-refresh-no-duplicates", TestFixture, NULL,
             test_setup, test_tree_toggle_refresh_no_duplicates, test_teardown);

  return g_test_run();
}
