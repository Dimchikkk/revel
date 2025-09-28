#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_spaces.h"
#include "connection.h"
#include "element.h"
#include "freehand_drawing.h"
#include "paper_note.h"
#include "media_note.h"
#include "note.h"
#include <pango/pangocairo.h>
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

CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay) {
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

  data->model = model_new();
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
  data->current_shape = NULL;
  data->shape_start_x = 0;
  data->shape_start_y = 0;

  // Initialize grid settings
  data->show_grid = FALSE;
  data->grid_color = (GdkRGBA){0.8, 0.8, 0.8, 1.0}; // Default light gray

  if (data->model != NULL && data->model->db != NULL) canvas_sync_with_model(data);

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
          break;
        }
        }
      }

    } else {
      // Create new visual element if it doesn't exist
      Element *visual_element = create_visual_element(model_element, data);
      if (visual_element) {
        model_element->visual_element = visual_element;
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
  if (data->model && data->model->current_space_name) {
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

  GList *visual_elements = canvas_get_visual_elements(data);
  GList *sorted_elements = g_list_copy(visual_elements);

  sorted_elements = g_list_sort(sorted_elements, compare_elements_by_z_index);

  for (GList *l = sorted_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    element_draw(element, cr, canvas_is_element_selected(data, element));
  }

  g_list_free(sorted_elements);
  g_list_free(visual_elements);

  // Draw current drawing in progress
  if (data->current_drawing) {
    element_draw((Element*)data->current_drawing, cr, FALSE);
  }

  // Draw current shape in progress
  if (data->current_shape) {
    element_draw((Element*)data->current_shape, cr, FALSE);
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
      };

      visual_element = (Element*)media_note_create(position, bg_color, size, media, text, data);
    } else if(model_element->image->image_data && model_element->image->image_size > 0) {
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
        .font_description = model_element->text ? model_element->text->font_description : "Ubuntu Mono 12"
      };
      int stroke_width = model_element->stroke_width > 0 ? model_element->stroke_width : 3;
      ShapeType shape_type = model_element->shape_type >= 0 ? model_element->shape_type : SHAPE_CIRCLE;
      gboolean filled = model_element->filled;
      ElementShape shape_config = {
        .shape_type = shape_type,
        .stroke_width = stroke_width,
        .filled = filled
      };
      visual_element = (Element*)shape_create(position, size, shape_color, stroke_width, shape_type, filled, text, shape_config, data);
    }
    break;
  default:
    break;
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
      visual_elements = g_list_append(visual_elements, model_element->visual_element);
    }
  }

  return visual_elements;
}

void canvas_sync_with_model(CanvasData *canvas_data) {
  if (!canvas_data || !canvas_data->model || !canvas_data->model->elements) {
    return;
  }

  GList *sorted_elements = sort_model_elements_for_serialization(canvas_data->model->elements);
  create_or_update_visual_elements(sorted_elements, canvas_data);
  g_list_free(sorted_elements);
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
