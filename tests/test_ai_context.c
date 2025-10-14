#include <glib.h>
#include "ai/ai_context.h"

static void test_truncate_noop(void) {
  const char *sample = "hello world";
  char *result = ai_context_truncate_utf8(sample, 64);
  g_assert_cmpstr(result, ==, sample);
  g_free(result);
}

static void test_truncate_basic(void) {
  const char *sample = "abcdefghij";
  char *result = ai_context_truncate_utf8(sample, 5);
  g_assert_cmpint(strlen(result), ==, 5);
  g_assert_true(g_str_has_prefix(sample, result));
  g_free(result);
}

static void test_truncate_utf8_boundary(void) {
  const char *sample = "ééé"; // each char is 2 bytes
  char *result = ai_context_truncate_utf8(sample, 3); // should keep only one character (2 bytes)
  g_assert_cmpint(g_utf8_strlen(result, -1), ==, 1);
  g_free(result);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/ai_context/truncate_noop", test_truncate_noop);
  g_test_add_func("/ai_context/truncate_basic", test_truncate_basic);
  g_test_add_func("/ai_context/truncate_utf8_boundary", test_truncate_utf8_boundary);
  return g_test_run();
}
