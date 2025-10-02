#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_spaces.h"
#include "connection.h"
#include "element.h"
#include "freehand_drawing.h"
#include "paper_note.h"
#include "media_note.h"
#include "note.h"
#include "inline_text.h"
#include <pango/pangocairo.h>
#include <string.h>
#include "model.h"
#include "space.h"
#include "undo_manager.h"
#include "shape.h"
#include "database.h"

static gint compare_elements_by_z_index(gconstpointer a, gconstpointer b) {
  const Element *element_a = (const Element*)a;
  const Element *element_b = (const Element*)b;
  return element_a->z - element_b->z;
}

static gboolean parse_hex_color(const char *hex_color, double *r, double *g, double *b) {
  if (!hex_color || hex_color[0] != '#' || strlen(hex_color) != 7) {
    return FALSE;
  }

  unsigned int color_int;
  if (sscanf(hex_color + 1, "%x", &color_int) != 1) {
    return FALSE;
  }

  *r = ((color_int >> 16) & 0xFF) / 255.0;
  *g = ((color_int >> 8) & 0xFF) / 255.0;
  *b = (color_int & 0xFF) / 255.0;
  return TRUE;
}


static gboolean parse_hex_color_rgba(const char *hex_color, ElementColor *color) {
  if (!hex_color || hex_color[0] != '#') {
    return FALSE;
  }

  unsigned int r, g, b, a = 255;
  size_t len = strlen(hex_color);
  if (len == 9) {
    if (sscanf(hex_color + 1, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4) {
      return FALSE;
    }
  } else if (len == 7) {
    if (sscanf(hex_color + 1, "%02x%02x%02x", &r, &g, &b) != 3) {
      return FALSE;
    }
  } else {
    return FALSE;
  }

  color->r = r / 255.0;
  color->g = g / 255.0;
  color->b = b / 255.0;
  color->a = a / 255.0;
  return TRUE;
}

CanvasData* canvas_data_new_with_db(GtkWidget *drawing_area, GtkWidget *overlay, const char *db_filename) {
  CanvasData *data = g_new0(CanvasData, 1);
  data->selected_elements = NULL;
  data->drawing_area = drawing_area;
  data->overlay = overlay;
  data->next_z_index = 1;
  data->selecting = FALSE;
  data->start_x = 0;
  data->start_y = 0;
  data->current_x = 0;
  data->current_y = 0;
  data->modifier_state = 0;

  data->default_cursor = gdk_cursor_new_from_name("default", NULL);
  data->move_cursor = gdk_cursor_new_from_name("move", NULL);
  data->resize_cursor = gdk_cursor_new_from_name("nwse-resize", NULL);
  data->connect_cursor = gdk_cursor_new_from_name("crosshair", NULL);
  data->current_cursor = NULL;

  data->panning = FALSE;
  data->pan_start_x = 0;
  data->pan_start_y = 0;
  data->offset_x = 0;
  data->offset_y = 0;
  data->zoom_scale = 1.0;
  data->last_mouse_x = 0;
  data->last_mouse_y = 0;

  data->model = model_new_with_file(db_filename);
  data->undo_manager = undo_manager_new(data->model);
  data->drag_start_positions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  data->drag_start_sizes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  data->drawing_mode = FALSE;
  data->current_drawing = NULL;
  data->drawing_color = (ElementColor) INITIAL_DRAWING_COLOR;
  data->drawing_stroke_width = 3;
  data->draw_cursor = gdk_cursor_new_from_name("pencil", NULL);
  data->line_cursor = gdk_cursor_new_from_name("crosshair", NULL);

  data->shape_mode = FALSE;
  data->selected_shape_type = SHAPE_CIRCLE;
  data->shape_filled = FALSE;
  data->shape_stroke_style = STROKE_STYLE_SOLID;
  data->shape_fill_style = FILL_STYLE_SOLID;
  data->current_shape = NULL;
  data->shape_start_x = 0;
  data->shape_start_y = 0;

  // Initialize grid settings
  data->show_grid = FALSE;
  data->grid_color = (GdkRGBA){0.8, 0.8, 0.8, 1.0}; // Default light gray

  // Initialize space name display (default to shown)
  data->show_space_name = TRUE;

  // Initialize hidden elements tracking
  data->hidden_elements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  // OPTIMIZATION: Initialize hidden children cache
  data->hidden_children_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  // Initialize animation timer
  data->animation_timer_id = 0;
  data->is_loading_space = FALSE;

  // Initialize quadtree with a large canvas bounds
  // The quadtree will cover a 100000x100000 canvas centered at origin
  data->quadtree = quadtree_new(-50000, -50000, 100000, 100000);

  if (data->model != NULL && data->model->db != NULL) {
    canvas_sync_with_model(data);
  }

  return data;
}

GList* sort_model_elements_for_serialization(GHashTable *elements_table) {
  GList *elements_list = g_hash_table_get_values(elements_table);
  return g_list_sort(elements_list, (GCompareFunc)model_compare_for_saving_loading);
}

static void update_text_base(char **dest_text,
                             char **dest_font,
                             double *r, double *g, double *b, double *a,
                             ModelText *src_text) {
  if (!src_text || !src_text->text) return;

  if (*dest_text == NULL ||
      strcmp(*dest_text, src_text->text) != 0 ||
      *r != src_text->r || *g != src_text->g ||
      *b != src_text->b || *a != src_text->a ||
      *dest_font == NULL ||
      strcmp(*dest_font, src_text->font_description) != 0) {

    g_free(*dest_text);
    *dest_text = g_strdup(src_text->text);

    g_free(*dest_font);
    *dest_font = g_strdup(src_text->font_description);

    *r = src_text->r;
    *g = src_text->g;
    *b = src_text->b;
    *a = src_text->a;
  }
}

void create_or_update_visual_elements(GList *sorted_elements, CanvasData *data) {
  GList *iter = sorted_elements;
  while (iter != NULL) {
    ModelElement *model_element = (ModelElement*)iter->data;

    if (model_element->visual_element != NULL) {
      // Update existing visual element properties directly
      Element *visual_element = model_element->visual_element;

      // OPTIMIZATION: Ensure reverse pointer is always set for existing elements
      visual_element->model_element = model_element;

      // Update position if changed
      if (model_element->position) {
        if (visual_element->x != model_element->position->x ||
            visual_element->y != model_element->position->y ||
            visual_element->z != model_element->position->z) {
          visual_element->x = model_element->position->x;
          visual_element->y = model_element->position->y;
          visual_element->z = model_element->position->z;
        }
      }

      // Update size if changed
      if (model_element->size) {
        if (visual_element->width != model_element->size->width ||
            visual_element->height != model_element->size->height) {
          visual_element->width = model_element->size->width;
          visual_element->height = model_element->size->height;
        }
      }

      // Update color if changed
      if (model_element->bg_color) {
        if (visual_element->bg_r != model_element->bg_color->r ||
            visual_element->bg_g != model_element->bg_color->g ||
            visual_element->bg_b != model_element->bg_color->b ||
            visual_element->bg_a != model_element->bg_color->a) {
          visual_element->bg_r = model_element->bg_color->r;
          visual_element->bg_g = model_element->bg_color->g;
          visual_element->bg_b = model_element->bg_color->b;
          visual_element->bg_a = model_element->bg_color->a;
        }
      }

      // Handle text updates for specific element types
      if (model_element->text && model_element->text->text) {
        switch (visual_element->type) {
        case ELEMENT_NOTE: {
          Note *note = (Note *)visual_element;
          update_text_base(&note->text, &note->font_description,
                           &note->text_r, &note->text_g,
                           &note->text_b, &note->text_a,
                           model_element->text);
          break;
        }
        case ELEMENT_PAPER_NOTE: {
          PaperNote *note = (PaperNote *)visual_element;
          update_text_base(&note->text, &note->font_description,
                           &note->text_r, &note->text_g,
                           &note->text_b, &note->text_a,
                           model_element->text);
          break;
        }
        case ELEMENT_MEDIA_FILE: {
          MediaNote *note = (MediaNote *)visual_element;
          update_text_base(&note->text, &note->font_description,
                           &note->text_r, &note->text_g,
                           &note->text_b, &note->text_a,
                           model_element->text);
          break;
        }
        case ELEMENT_SPACE: {
          SpaceElement *note = (SpaceElement *)visual_element;
          update_text_base(&note->text, &note->font_description,
                           &note->text_r, &note->text_g,
                           &note->text_b, &note->text_a,
                           model_element->text);
          break;
        }
        case ELEMENT_CONNECTION:
          // Connections typically don't have text
          break;
        case ELEMENT_FREEHAND_DRAWING:
          // Freehand drawings don't have text
          break;
        case ELEMENT_SHAPE: {
          Shape *shape = (Shape *)visual_element;
          update_text_base(&shape->text, &shape->font_description,
                           &shape->text_r, &shape->text_g,
                           &shape->text_b, &shape->text_a,
                           model_element->text);

          int new_stroke_width = model_element->stroke_width > 0 ? model_element->stroke_width : shape->stroke_width;
          if (shape->stroke_width != new_stroke_width) {
            shape->stroke_width = new_stroke_width;
          }
          shape->filled = model_element->filled;

          if (model_element->stroke_style >= STROKE_STYLE_SOLID && model_element->stroke_style <= STROKE_STYLE_DOTTED) {
            shape->stroke_style = model_element->stroke_style;
          } else {
            shape->stroke_style = STROKE_STYLE_SOLID;
          }

          if (model_element->fill_style >= FILL_STYLE_SOLID && model_element->fill_style <= FILL_STYLE_CROSS_HATCH) {
            shape->fill_style = model_element->fill_style;
          } else {
            shape->fill_style = FILL_STYLE_SOLID;
          }

          ElementColor stroke_color = { .r = shape->base.bg_r, .g = shape->base.bg_g, .b = shape->base.bg_b, .a = 1.0 };
          if (parse_hex_color_rgba(model_element->stroke_color, &stroke_color)) {
            shape->stroke_r = stroke_color.r;
            shape->stroke_g = stroke_color.g;
            shape->stroke_b = stroke_color.b;
            shape->stroke_a = stroke_color.a;
          } else {
            shape->stroke_r = stroke_color.r;
            shape->stroke_g = stroke_color.g;
            shape->stroke_b = stroke_color.b;
            shape->stroke_a = stroke_color.a;
          }
          break;
        }
        case ELEMENT_INLINE_TEXT: {
          InlineText *text = (InlineText *)visual_element;
          update_text_base(&text->text, &text->font_description,
                           &text->text_r, &text->text_g,
                           &text->text_b, &text->text_a,
                           model_element->text);
          break;
        }
        }
      }

    } else {
      // Create new visual element if it doesn't exist
      Element *visual_element = create_visual_element(model_element, data);
      if (visual_element) {
        model_element->visual_element = visual_element;
        visual_element->model_element = model_element;  // OPTIMIZATION: Set reverse pointer
      }
    }

    // Update z-index tracking
    if (model_element->position && model_element->position->z >= data->next_z_index) {
      data->next_z_index = model_element->position->z + 1;
    }

    iter = iter->next;
  }
}

void canvas_data_free(CanvasData *data) {
  if (data->default_cursor) g_object_unref(data->default_cursor);
  if (data->move_cursor) g_object_unref(data->move_cursor);
  if (data->resize_cursor) g_object_unref(data->resize_cursor);
  if (data->connect_cursor) g_object_unref(data->connect_cursor);

  g_list_free(data->selected_elements);

  if (data->draw_cursor) g_object_unref(data->draw_cursor);
  if (data->line_cursor) g_object_unref(data->line_cursor);
  if (data->current_drawing) element_free((Element*)data->current_drawing);

  // Clean up hidden elements tracking
  if (data->hidden_elements) g_hash_table_destroy(data->hidden_elements);
  // OPTIMIZATION: Clean up hidden children cache
  if (data->hidden_children_cache) g_hash_table_destroy(data->hidden_children_cache);

  // Clean up animation timer
  if (data->animation_timer_id > 0) {
    g_source_remove(data->animation_timer_id);
    data->animation_timer_id = 0;
  }

  // Clean up quadtree
  if (data->quadtree) quadtree_free(data->quadtree);

  // Don't free the model here - it's freed in canvas_on_app_shutdown

  g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  // Apply zoom and panning transformations
  cairo_scale(cr, data->zoom_scale, data->zoom_scale);
  cairo_translate(cr, data->offset_x, data->offset_y);

  // Canvas background
  if (data->model && data->model->current_space_background_color) {
    double r, g, b;
    if (parse_hex_color(data->model->current_space_background_color, &r, &g, &b)) {
      cairo_set_source_rgb(cr, r, g, b);
    } else {
      cairo_set_source_rgb(cr, 0.094, 0.094, 0.094); // fallback
    }
  } else {
    // Fallback to default color
    cairo_set_source_rgb(cr, 0.094, 0.094, 0.094);
  }
  cairo_paint(cr);

  // Draw grid if enabled
  if (data->model && data->model->current_space_show_grid) {
    cairo_save(cr);

    // Grid parameters
    const int major_grid_size = 80;  // Large cell size (4x4 subcells)
    const int minor_grid_size = 20;  // Small subcell size

    // Calculate visible area in canvas coordinates
    int start_x = (int)(-data->offset_x - 100);
    int start_y = (int)(-data->offset_y - 100);
    int end_x = (int)(-data->offset_x + width / data->zoom_scale + 100);
    int end_y = (int)(-data->offset_y + height / data->zoom_scale + 100);

    // Snap to grid
    start_x = (start_x / minor_grid_size) * minor_grid_size;
    start_y = (start_y / minor_grid_size) * minor_grid_size;

    // Draw minor grid lines (4x4 subcells) - lighter
    cairo_set_source_rgba(cr, data->model->current_space_grid_color.red, data->model->current_space_grid_color.green,
                         data->model->current_space_grid_color.blue, data->model->current_space_grid_color.alpha * 0.3);
    cairo_set_line_width(cr, 0.5 / data->zoom_scale);

    // Draw minor vertical lines
    for (int x = start_x; x <= end_x; x += minor_grid_size) {
      if (x % major_grid_size != 0) {  // Skip major grid lines
        cairo_move_to(cr, x, start_y);
        cairo_line_to(cr, x, end_y);
      }
    }

    // Draw minor horizontal lines
    for (int y = start_y; y <= end_y; y += minor_grid_size) {
      if (y % major_grid_size != 0) {  // Skip major grid lines
        cairo_move_to(cr, start_x, y);
        cairo_line_to(cr, end_x, y);
      }
    }
    cairo_stroke(cr);

    // Draw major grid lines (main cells) - darker
    cairo_set_source_rgba(cr, data->model->current_space_grid_color.red, data->model->current_space_grid_color.green,
                         data->model->current_space_grid_color.blue, data->model->current_space_grid_color.alpha);
    cairo_set_line_width(cr, 1.0 / data->zoom_scale);

    // Snap major grid to major grid size
    int major_start_x = (start_x / major_grid_size) * major_grid_size;
    int major_start_y = (start_y / major_grid_size) * major_grid_size;

    // Draw major vertical lines
    for (int x = major_start_x; x <= end_x; x += major_grid_size) {
      cairo_move_to(cr, x, start_y);
      cairo_line_to(cr, x, end_y);
    }

    // Draw major horizontal lines
    for (int y = major_start_y; y <= end_y; y += major_grid_size) {
      cairo_move_to(cr, start_x, y);
      cairo_line_to(cr, end_x, y);
    }
    cairo_stroke(cr);

    cairo_restore(cr);
  }

  // Draw current space name in the top-left corner
  if (data->show_space_name && data->model && data->model->current_space_name) {
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text

    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Ubuntu Mono 10");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    char space_info[100];
    snprintf(space_info, sizeof(space_info), "Space: %s", data->model->current_space_name);

    pango_layout_set_text(layout, space_info, -1);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    // Draw semi-transparent background for better readability
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.7);  // White with transparency
    cairo_rectangle(cr, 10, 10, text_width + 10, text_height + 6);
    cairo_fill(cr);

    // Draw the text
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text
    cairo_move_to(cr, 15, 13);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
  }

  // Calculate visible area for culling FIRST
  int visible_x = -data->offset_x;
  int visible_y = -data->offset_y;
  int visible_width = gtk_widget_get_width(data->drawing_area) / data->zoom_scale;
  int visible_height = gtk_widget_get_height(data->drawing_area) / data->zoom_scale;

  // OPTIMIZATION: Collect only visible elements first, then sort
  GList *visual_elements = canvas_get_visual_elements(data);
  GList *visible_elements = NULL;

  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;

    // Don't cull connections - their bounds are updated during draw and curves extend beyond bounding box
    if (element->type != ELEMENT_CONNECTION) {
      // View frustum culling - skip elements completely outside viewport
      // Note: using >= and <= to ensure partially visible elements are drawn
      if (element->x >= visible_x + visible_width ||
          element->y >= visible_y + visible_height ||
          element->x + element->width <= visible_x ||
          element->y + element->height <= visible_y) {
        continue;
      }
    }

    // Skip drawing hidden elements
    // OPTIMIZATION: Use cached reverse pointer instead of O(n) lookup
    if (element->model_element && canvas_is_element_hidden(data, element->model_element->uuid)) {
      continue;
    }

    // Add to visible elements list
    visible_elements = g_list_prepend(visible_elements, element);
  }

  // Sort only the visible elements by z-index
  visible_elements = g_list_sort(visible_elements, compare_elements_by_z_index);

  // Draw visible elements
  for (GList *l = visible_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;

    // Apply animation alpha if element is animating
    if (element->animating) {
      cairo_push_group(cr);
      element_draw(element, cr, canvas_is_element_selected(data, element));
      cairo_pop_group_to_source(cr);
      cairo_paint_with_alpha(cr, element->animation_alpha);
    } else {
      element_draw(element, cr, canvas_is_element_selected(data, element));
    }

    // Draw indicator if element has hidden children
    // OPTIMIZATION: Use cached reverse pointer instead of O(n) lookup
    if (element->model_element && canvas_has_hidden_children(data, element->model_element->uuid)) {
      // Draw a small triangle indicator in the bottom-right corner
      double indicator_size = 8.0;
      double x = (element->x + element->width - indicator_size - 3) * data->zoom_scale + data->offset_x * data->zoom_scale;
      double y = (element->y + element->height - indicator_size - 3) * data->zoom_scale + data->offset_y * data->zoom_scale;

      cairo_save(cr);
      cairo_set_source_rgba(cr, 1.0, 0.5, 0.0, 0.8); // Orange color
      cairo_move_to(cr, x, y + indicator_size);
      cairo_line_to(cr, x + indicator_size, y + indicator_size);
      cairo_line_to(cr, x + indicator_size / 2, y);
      cairo_close_path(cr);
      cairo_fill(cr);
      cairo_restore(cr);
    }
  }

  g_list_free(visible_elements);
  g_list_free(visual_elements);

  // Draw current drawing in progress
  if (data->current_drawing) {
    element_draw((Element*)data->current_drawing, cr, FALSE);
  }

  // Draw current shape in progress
  if (data->current_shape) {
    element_draw((Element*)data->current_shape, cr, FALSE);
  }

  // Draw guide lines in drawing mode
  if (data->drawing_mode) {
    int mouse_cx, mouse_cy;
    canvas_screen_to_canvas(data, data->last_mouse_x, data->last_mouse_y, &mouse_cx, &mouse_cy);

    // Calculate visible area in canvas coordinates
    int start_x = (int)(-data->offset_x - 100);
    int start_y = (int)(-data->offset_y - 100);
    int end_x = (int)(-data->offset_x + width / data->zoom_scale + 100);
    int end_y = (int)(-data->offset_y + height / data->zoom_scale + 100);

    cairo_save(cr);

    // Set dashed line pattern
    double dashes[] = {10.0 / data->zoom_scale, 5.0 / data->zoom_scale};
    cairo_set_dash(cr, dashes, 2, 0);
    cairo_set_line_width(cr, 1.0 / data->zoom_scale);
    cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 0.5); // Light gray with transparency

    // Draw vertical guide line
    cairo_move_to(cr, mouse_cx, start_y);
    cairo_line_to(cr, mouse_cx, end_y);
    cairo_stroke(cr);

    // Draw horizontal guide line
    cairo_move_to(cr, start_x, mouse_cy);
    cairo_line_to(cr, end_x, mouse_cy);
    cairo_stroke(cr);

    cairo_restore(cr);
  }

  if (data->selecting) {
    int start_cx, start_cy, current_cx, current_cy;
    canvas_screen_to_canvas(data, data->start_x, data->start_y, &start_cx, &start_cy);
    canvas_screen_to_canvas(data, data->current_x, data->current_y, &current_cx, &current_cy);

    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.3);
    cairo_rectangle(cr,
                    MIN(start_cx, current_cx),
                    MIN(start_cy, current_cy),
                    ABS(current_cx - start_cx),
                    ABS(current_cy - start_cy));
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  }
}

void canvas_clear_selection(CanvasData *data) {
  if (data->selected_elements) {
    g_list_free(data->selected_elements);
    data->selected_elements = NULL;
  }
}

gboolean canvas_is_element_selected(CanvasData *data, Element *element) {
  for (GList *l = data->selected_elements; l != NULL; l = l->next) {
    if (l->data == element) {
      return TRUE;
    }
  }
  return FALSE;
}

void canvas_on_app_shutdown(GApplication *app, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(app), "canvas_data");
  if (data) {
    // Save the model before freeing
    if (data->model) {
      model_save_elements(data->model);
      model_free(data->model);
    }
    canvas_data_free(data);
    g_object_set_data(G_OBJECT(app), "canvas_data", NULL);
  }
}

Element* create_visual_element(ModelElement *model_element, CanvasData *data) {
  if (!model_element || !model_element->type) {
    return NULL;
  }

  if (!model_element->position  || !model_element->size || !model_element->bg_color) {
    return NULL;
  }

  ElementPosition position = {
    .x = model_element->position->x,
    .y = model_element->position->y,
    .z = model_element->position->z,
  };
  ElementColor bg_color = {
    .r = model_element->bg_color->r,
    .g = model_element->bg_color->g,
    .b = model_element->bg_color->b,
    .a = model_element->bg_color->a,
  };
  ElementSize size = {
    .width = model_element->size->width,
    .height = model_element->size->height,
  };

  Element *visual_element = NULL;

  switch (model_element->type->type) {
  case ELEMENT_NOTE:
    if (model_element->text) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementText text = {
        .text = model_element->text->text,
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .strikethrough = model_element->text->strikethrough,
        .alignment = model_element->text->alignment,
      };

      visual_element = (Element*)note_create(position, bg_color, size, text, data);
    }
    break;

  case ELEMENT_PAPER_NOTE:
    if (model_element->text) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementText text = {
        .text = model_element->text->text,
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .alignment = model_element->text->alignment,
        .strikethrough = model_element->text->strikethrough,
      };


      visual_element = (Element*)paper_note_create(position, bg_color, size, text, data);
    }
    break;

  case ELEMENT_SPACE: {
    if (model_element->text) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementText text = {
        .text = model_element->text->text ? model_element->text->text : "Space",
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .alignment = model_element->text->alignment,
        .strikethrough = model_element->text->strikethrough,
      };

      visual_element = (Element*)space_element_create(position, bg_color, size, text, data);
    }
    break;
  }

  case ELEMENT_CONNECTION:
    if (model_element->from_element_uuid && model_element->to_element_uuid) {
      // Find the visual elements for the from and to elements
      Element *from_element = NULL;
      Element *to_element = NULL;

      // Search through all model elements to find the visual elements
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init(&iter, data->model->elements);

      while (g_hash_table_iter_next(&iter, &key, &value)) {
        ModelElement *current = (ModelElement*)value;
        if (current->visual_element) {
          if (g_strcmp0(current->uuid, model_element->from_element_uuid) == 0) {
            from_element = current->visual_element;
          }
          if (g_strcmp0(current->uuid, model_element->to_element_uuid) == 0) {
            to_element = current->visual_element;
          }

          // If we found both, break early
          if (from_element && to_element) {
            break;
          }
        }
      }

      if (from_element && to_element) {
        ElementConnection connection_config = {
          .from_element = from_element,
          .to_element = to_element,
          .from_element_uuid = model_element->from_element_uuid,
          .to_element_uuid = model_element->to_element_uuid,
          .from_point = model_element->from_point,
          .to_point = model_element->to_point,
          .connection_type = model_element->connection_type,
          .arrowhead_type = model_element->arrowhead_type
        };

        visual_element = (Element*)connection_create(connection_config, bg_color, position.z, data);

        if (visual_element && model_element->position &&
            model_element->position->z >= data->next_z_index) {
          data->next_z_index = model_element->position->z + 1;
        }
      } else {
        g_printerr("Failed to find visual elements for connection: from=%s, to=%s\n",
                   model_element->from_element_uuid, model_element->to_element_uuid);
      }
    }
    break;
  case ELEMENT_MEDIA_FILE:
    if (model_element->video && model_element->video->duration > 0) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementMedia media = {
        .type = MEDIA_TYPE_VIDEO,
        .image_data = model_element->video->thumbnail_data,
        .image_size = model_element->video->thumbnail_size,
        .video_data = model_element->video->video_data,
        .video_size = model_element->video->video_size,
        .duration = model_element->video->duration
      };
      ElementText text = {
        .text = model_element->text->text,
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .alignment = model_element->text->alignment,
        .strikethrough = model_element->text->strikethrough,
      };

      visual_element = (Element*)media_note_create(position, bg_color, size, media, text, data);
    } else if(model_element->image && model_element->image->image_data && model_element->image->image_size > 0) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementMedia media = {
        .type = MEDIA_TYPE_IMAGE,
        .image_data = model_element->image->image_data,
        .image_size = model_element->image->image_size,
        .video_data = NULL,
        .video_size = 0,
        .duration = 0
      };
      ElementText text = {
        .text = model_element->text->text,
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .alignment = model_element->text->alignment,
        .strikethrough = model_element->text->strikethrough,
      };
      visual_element = (Element*)media_note_create(position, bg_color, size, media, text, data);
    }
    break;
  case ELEMENT_FREEHAND_DRAWING:
    {
      ElementColor stroke_color = {
        .r = model_element->bg_color->r,
        .g = model_element->bg_color->g,
        .b = model_element->bg_color->b,
        .a = model_element->bg_color->a,
      };
      int stroke_width = model_element->stroke_width > 0 ? model_element->stroke_width : 3;

      visual_element = (Element*)freehand_drawing_create(position, stroke_color, stroke_width, data);

      // Add all the drawing points
      if (model_element->drawing_points) {
        for (guint i = 0; i < model_element->drawing_points->len; i++) {
          DrawingPoint *point = &g_array_index(model_element->drawing_points, DrawingPoint, i);
          freehand_drawing_add_point((FreehandDrawing*)visual_element, model_element->position->x + point->x, model_element->position->y + point->y);
        }
      }
    }
    break;

  case ELEMENT_SHAPE:
    {
      ElementColor shape_color = {
        .r = model_element->bg_color->r,
        .g = model_element->bg_color->g,
        .b = model_element->bg_color->b,
        .a = model_element->bg_color->a,
      };
      ElementText text = {
        .text = model_element->text ? model_element->text->text : "",
        .text_color = {
          .r = model_element->text ? model_element->text->r : 1.0,
          .g = model_element->text ? model_element->text->g : 1.0,
          .b = model_element->text ? model_element->text->b : 1.0,
          .a = model_element->text ? model_element->text->a : 1.0,
        },
        .font_description = model_element->text ? model_element->text->font_description : "Ubuntu Mono 12",
        .alignment = model_element->text ? model_element->text->alignment : "center",
        .strikethrough = model_element->text ? model_element->text->strikethrough : FALSE,
      };
      int stroke_width = model_element->stroke_width > 0 ? model_element->stroke_width : 3;
      ShapeType shape_type = model_element->shape_type >= 0 ? model_element->shape_type : SHAPE_CIRCLE;
      gboolean filled = model_element->filled;
      int stroke_style = (model_element->stroke_style >= STROKE_STYLE_SOLID && model_element->stroke_style <= STROKE_STYLE_DOTTED)
                           ? model_element->stroke_style
                           : STROKE_STYLE_SOLID;
      int fill_style = (model_element->fill_style >= FILL_STYLE_SOLID && model_element->fill_style <= FILL_STYLE_CROSS_HATCH)
                         ? model_element->fill_style
                         : FILL_STYLE_SOLID;
      ElementColor stroke_color = { .r = 0.0, .g = 0.0, .b = 0.0, .a = 1.0 };
      if (!parse_hex_color_rgba(model_element->stroke_color, &stroke_color)) {
        stroke_color.r = shape_color.r;
        stroke_color.g = shape_color.g;
        stroke_color.b = shape_color.b;
        stroke_color.a = 1.0;
      }
      ElementShape shape_config = {
        .shape_type = shape_type,
        .stroke_width = stroke_width,
        .filled = filled,
        .stroke_style = stroke_style,
        .fill_style = fill_style,
        .stroke_color = stroke_color
      };
      ElementDrawing drawing_config = {
        .drawing_points = model_element->drawing_points,
        .stroke_width = stroke_width,
      };
      visual_element = (Element*)shape_create(position, size, shape_color, stroke_width, shape_type, filled, text, shape_config,
                                              model_element->drawing_points ? &drawing_config : NULL,
                                              data);
    }
    break;

  case ELEMENT_INLINE_TEXT:
    if (model_element->text) {
      ElementColor text_color = { .r = model_element->text->r, .g = model_element->text->g, .b = model_element->text->b, .a = model_element->text->a };
      ElementText text = {
        .text = model_element->text->text,
        .text_color = text_color,
        .font_description = model_element->text->font_description,
        .alignment = model_element->text->alignment,
        .strikethrough = model_element->text->strikethrough,
      };

      visual_element = (Element*)inline_text_create(position, bg_color, size, text, data);
    }
    break;

  default:
    break;
  }

  // Initialize animation state for space loading
  if (visual_element) {
    if (data && data->is_loading_space) {
      visual_element->animating = TRUE;
      visual_element->animation_start_time = g_get_monotonic_time();
      visual_element->animation_alpha = 0.0;
    } else {
      visual_element->animating = FALSE;
      visual_element->animation_start_time = 0;
      visual_element->animation_alpha = 1.0;
    }
  }

  // Set rotation from model
  if (visual_element) {
    visual_element->rotation_degrees = model_element->rotation_degrees;
  }

  return visual_element;
}

GList* canvas_get_visual_elements(CanvasData *data) {
  if (!data || !data->model || !data->model->elements) {
    return NULL;
  }

  GList *visual_elements = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, data->model->elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_element = (ModelElement*)value;

    if (model_element->state == MODEL_STATE_DELETED) continue;

    // Only include elements that belong to the current space
    if (model_element->visual_element != NULL &&
        g_strcmp0(model_element->space_uuid, data->model->current_space_uuid) == 0) {
      // Use prepend (O(1)) instead of append (O(n)) for massive performance gain
      visual_elements = g_list_prepend(visual_elements, model_element->visual_element);
    }
  }

  // Note: Order doesn't matter here as we sort by z-index later in draw function
  return visual_elements;
}

// Animation functions
static gboolean update_element_animations(gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !data->model || !data->model->elements) {
    return G_SOURCE_REMOVE;
  }

  gint64 current_time = g_get_monotonic_time();
  const gint64 animation_duration = 300000; // 300ms in microseconds
  gboolean has_animating_elements = FALSE;

  // Iterate through all model elements and update their visual element animations
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, data->model->elements);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_element = (ModelElement*)value;
    if (model_element && model_element->visual_element) {
      Element *element = model_element->visual_element;

      if (element->animating) {
        gint64 elapsed = current_time - element->animation_start_time;

        if (elapsed >= animation_duration) {
          // Animation complete
          element->animating = FALSE;
          element->animation_alpha = 1.0;
        } else {
          // Calculate animation progress (ease-out)
          double progress = (double)elapsed / animation_duration;
          progress = 1.0 - (1.0 - progress) * (1.0 - progress); // ease-out
          element->animation_alpha = progress;
          has_animating_elements = TRUE;
        }
      }
    }
  }

  // Redraw the canvas if we have animations
  if (has_animating_elements || data->animation_timer_id > 0) {
    gtk_widget_queue_draw(data->drawing_area);
  }

  // Continue timer if we still have animating elements
  if (!has_animating_elements) {
    data->animation_timer_id = 0;
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

void canvas_rebuild_quadtree(CanvasData *canvas_data) {
  if (!canvas_data || !canvas_data->quadtree) return;

  quadtree_clear(canvas_data->quadtree);
  GList *visual_elements = canvas_get_visual_elements(canvas_data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    quadtree_insert(canvas_data->quadtree, element);
  }
}

void canvas_sync_with_model(CanvasData *canvas_data) {
  if (!canvas_data || !canvas_data->model || !canvas_data->model->elements) {
    return;
  }

  GList *sorted_elements = sort_model_elements_for_serialization(canvas_data->model->elements);
  create_or_update_visual_elements(sorted_elements, canvas_data);
  g_list_free(sorted_elements);

  // Rebuild quadtree with all visual elements
  canvas_rebuild_quadtree(canvas_data);

  // Start animation timer if elements were loaded and we're not already animating
  if (g_hash_table_size(canvas_data->model->elements) > 0 && canvas_data->animation_timer_id == 0) {
    canvas_data->animation_timer_id = g_timeout_add(16, update_element_animations, canvas_data); // ~60 FPS
  }
}

void canvas_screen_to_canvas(CanvasData *data, int screen_x, int screen_y, int *canvas_x, int *canvas_y) {
  *canvas_x = (int)((screen_x / data->zoom_scale) - data->offset_x);
  *canvas_y = (int)((screen_y / data->zoom_scale) - data->offset_y);
}

void canvas_canvas_to_screen(CanvasData *data, int canvas_x, int canvas_y, int *screen_x, int *screen_y) {
  *screen_x = (int)((canvas_x + data->offset_x) * data->zoom_scale);
  *screen_y = (int)((canvas_y + data->offset_y) * data->zoom_scale);
}

void canvas_update_zoom_entry(CanvasData *data) {
  if (data->zoom_entry) {
    char zoom_text[16];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", data->zoom_scale * 100);
    gtk_editable_set_text(GTK_EDITABLE(data->zoom_entry), zoom_text);
  }
}

// Hide/show children functionality
void canvas_hide_children(CanvasData *data, const char *parent_uuid) {
  if (!data || !parent_uuid || !data->hidden_elements || !data->model) return;

  GList *children = find_children_bfs(data->model, parent_uuid);
  gboolean has_children = (children != NULL);

  for (GList *iter = children; iter != NULL; iter = iter->next) {
    ModelElement *element = (ModelElement*)iter->data;
    if (element && element->uuid) {
      // Hide all children elements (parent is already excluded by find_children_bfs)
      g_hash_table_insert(data->hidden_elements, g_strdup(element->uuid), GINT_TO_POINTER(TRUE));
    }
  }
  g_list_free(children);

  // OPTIMIZATION: Update cache - mark parent as having hidden children
  if (has_children) {
    g_hash_table_insert(data->hidden_children_cache, g_strdup(parent_uuid), GINT_TO_POINTER(TRUE));
  }
}

void canvas_show_children(CanvasData *data, const char *parent_uuid) {
  if (!data || !parent_uuid || !data->hidden_elements || !data->model) return;

  GList *children = find_children_bfs(data->model, parent_uuid);
  for (GList *iter = children; iter != NULL; iter = iter->next) {
    ModelElement *element = (ModelElement*)iter->data;
    if (element && element->uuid) {
      // Show all children elements (parent is already excluded by find_children_bfs)
      g_hash_table_remove(data->hidden_elements, element->uuid);
    }
  }
  g_list_free(children);

  // OPTIMIZATION: Update cache - remove parent from having hidden children
  g_hash_table_remove(data->hidden_children_cache, parent_uuid);
}

gboolean canvas_is_element_hidden(CanvasData *data, const char *element_uuid) {
  if (!data || !element_uuid || !data->hidden_elements) return FALSE;
  return g_hash_table_contains(data->hidden_elements, element_uuid);
}

gboolean canvas_has_hidden_children(CanvasData *data, const char *parent_uuid) {
  if (!data || !parent_uuid || !data->hidden_children_cache) return FALSE;

  // OPTIMIZATION: Use cached result instead of expensive BFS search
  return g_hash_table_contains(data->hidden_children_cache, parent_uuid);
}

void canvas_toggle_space_name_visibility(GtkToggleButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data) return;
  if (!data->drawing_area) return;
  if (!GTK_IS_WIDGET(data->drawing_area)) return;

  data->show_space_name = !data->show_space_name;
  gtk_widget_queue_draw(data->drawing_area);
}

// Toolbar auto-hide functions
static gboolean hide_toolbar_timeout(gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (data->toolbar_auto_hide && data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), FALSE);
    data->toolbar_visible = FALSE;
  }

  data->toolbar_hide_timer_id = 0;
  return G_SOURCE_REMOVE;
}

void show_toolbar(CanvasData *data) {
  if (!data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), TRUE);
    data->toolbar_visible = TRUE;
  }

  // Reset hide timer
  if (data->toolbar_hide_timer_id > 0) {
    g_source_remove(data->toolbar_hide_timer_id);
  }

  if (data->toolbar_auto_hide) {
    data->toolbar_hide_timer_id = g_timeout_add(3000, hide_toolbar_timeout, data);
  }
}

void toggle_toolbar_visibility(CanvasData *data) {
  if (data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), FALSE);
    data->toolbar_visible = FALSE;
    if (data->toolbar_hide_timer_id > 0) {
      g_source_remove(data->toolbar_hide_timer_id);
      data->toolbar_hide_timer_id = 0;
    }
  } else {
    show_toolbar(data);
  }
}

void toggle_toolbar_auto_hide(CanvasData *data) {
  data->toolbar_auto_hide = !data->toolbar_auto_hide;

  if (data->toolbar_auto_hide) {
    // Start auto-hide timer
    if (data->toolbar_visible) {
      show_toolbar(data); // This will set the timer
    }
  } else {
    // Cancel auto-hide timer and ensure toolbar is visible
    if (data->toolbar_hide_timer_id > 0) {
      g_source_remove(data->toolbar_hide_timer_id);
      data->toolbar_hide_timer_id = 0;
    }
    show_toolbar(data);
  }
}

// Callback for zoom entry changes
void on_zoom_entry_activate(GtkEntry *entry, gpointer user_data) {
  CanvasData *data = (CanvasData *)user_data;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Parse the zoom percentage
  char *endptr;
  double zoom_percent = strtod(text, &endptr);

  // Check if we have a valid number
  if (endptr != text && zoom_percent > 0) {
    // Remove % sign if present
    if (*endptr == '%') {
      zoom_percent /= 100.0;
    } else if (zoom_percent > 1.0) {
      // Assume it's a percentage if > 1
      zoom_percent /= 100.0;
    }

    // Clamp to valid zoom range
    if (zoom_percent < 0.1) zoom_percent = 0.1;
    if (zoom_percent > 10.0) zoom_percent = 10.0;

    data->zoom_scale = zoom_percent;
    gtk_widget_queue_draw(data->drawing_area);

    // Update the entry to show the normalized value
    char zoom_text[16];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", zoom_percent * 100);
    gtk_editable_set_text(GTK_EDITABLE(entry), zoom_text);
  } else {
    // Invalid input, reset to current zoom
    char zoom_text[16];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", data->zoom_scale * 100);
    gtk_editable_set_text(GTK_EDITABLE(entry), zoom_text);
  }
}

static gboolean hide_notification(gpointer user_data) {
  GtkWidget *label = GTK_WIDGET(user_data);
  gtk_widget_set_visible(label, FALSE);
  return G_SOURCE_REMOVE;
}

void canvas_show_notification(CanvasData *data, const char *message) {
  if (!data || !data->overlay || !message) {
    return;
  }

  // Create a label for the notification
  GtkWidget *label = gtk_label_new(message);
  gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(label, GTK_ALIGN_START);
  gtk_widget_set_margin_top(label, 20);

  // Style the notification
  gtk_widget_add_css_class(label, "notification");

  // Add CSS for the notification style
  GtkCssProvider *provider = gtk_css_provider_new();
  const char *css =
    ".notification { "
    "  background-color: rgba(0, 0, 0, 0.8); "
    "  color: white; "
    "  padding: 12px 24px; "
    "  border-radius: 6px; "
    "  font-size: 14px; "
    "  font-weight: bold; "
    "}";
  gtk_css_provider_load_from_data(provider, css, -1);
  gtk_style_context_add_provider_for_display(
    gdk_display_get_default(),
    GTK_STYLE_PROVIDER(provider),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  // Add to overlay
  gtk_overlay_add_overlay(GTK_OVERLAY(data->overlay), label);

  // Auto-hide after 1.5 seconds
  g_timeout_add(1500, hide_notification, label);
}
