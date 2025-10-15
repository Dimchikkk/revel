#include "canvas_placement.h"
#include "canvas_core.h"
#include "../model.h"
#include <math.h>

#define PLACEMENT_STEP 20
#define MAX_SEARCH_RADIUS 1000

// Check if a rectangle overlaps with any existing elements
static gboolean check_overlap(CanvasData *data, int x, int y, int width, int height) {
  if (!data->model || !data->model->elements) {
    return FALSE;
  }

  // Add some padding around elements
  const int padding = 20;

  // Iterate through hash table of elements
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, data->model->elements);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *elem = (ModelElement *)value;

    // Skip connections as they don't really occupy space
    if (elem->type && elem->type->type == ELEMENT_CONNECTION) {
      continue;
    }

    // Check if this element is in the current space
    if (data->model->current_space_uuid) {
      if (!elem->space_uuid || strcmp(elem->space_uuid, data->model->current_space_uuid) != 0) {
        continue;
      }
    }

    // Skip elements without position or size
    if (!elem->position || !elem->size) {
      continue;
    }

    // Check rectangle overlap with padding
    int elem_x = elem->position->x;
    int elem_y = elem->position->y;
    int elem_w = elem->size->width;
    int elem_h = elem->size->height;

    if (!(x + width + padding < elem_x - padding ||
          x - padding > elem_x + elem_w + padding ||
          y + height + padding < elem_y - padding ||
          y - padding > elem_y + elem_h + padding)) {
      return TRUE; // Overlap detected
    }
  }

  return FALSE; // No overlap
}

// Find the closest empty position from viewport center using spiral search
void canvas_find_empty_position(CanvasData *data, int width, int height, int *out_x, int *out_y) {
  // Get viewport dimensions
  int viewport_width = gtk_widget_get_width(data->drawing_area);
  int viewport_height = gtk_widget_get_height(data->drawing_area);

  // Calculate viewport center in canvas coordinates
  int viewport_center_screen_x = viewport_width / 2;
  int viewport_center_screen_y = viewport_height / 2;

  int center_x, center_y;
  canvas_screen_to_canvas(data, viewport_center_screen_x, viewport_center_screen_y, &center_x, &center_y);

  // Try placing at center first
  int candidate_x = center_x - width / 2;
  int candidate_y = center_y - height / 2;

  if (!check_overlap(data, candidate_x, candidate_y, width, height)) {
    *out_x = candidate_x;
    *out_y = candidate_y;
    return;
  }

  // Spiral search outward from center
  // We'll use a rectangular spiral pattern
  int radius = PLACEMENT_STEP;

  while (radius < MAX_SEARCH_RADIUS) {
    // Try positions in a square around the center
    for (int angle = 0; angle < 360; angle += 15) {
      double rad = angle * M_PI / 180.0;
      int offset_x = (int)(radius * cos(rad));
      int offset_y = (int)(radius * sin(rad));

      candidate_x = center_x + offset_x - width / 2;
      candidate_y = center_y + offset_y - height / 2;

      if (!check_overlap(data, candidate_x, candidate_y, width, height)) {
        *out_x = candidate_x;
        *out_y = candidate_y;
        return;
      }
    }

    radius += PLACEMENT_STEP;
  }

  // If we couldn't find an empty spot (unlikely), just place at center
  *out_x = center_x - width / 2;
  *out_y = center_y - height / 2;
}