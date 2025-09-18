#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_spaces.h"
#include "connection.h"
#include "element.h"
#include "paper_note.h"
#include "media_note.h"
#include "note.h"
#include <pango/pangocairo.h>
#include "model.h"
#include "undo_manager.h"

static gint compare_elements_by_z_index(gconstpointer a, gconstpointer b) {
  const Element *element_a = (const Element*)a;
  const Element *element_b = (const Element*)b;
  return element_a->z - element_b->z;
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


  data->model = model_new();
  data->undo_manager = undo_manager_new(data->model);
  data->drag_start_positions = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  data->drag_start_sizes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);


  if (data->model != NULL && data->model->db != NULL) canvas_sync_with_model(data);

  return data;
}

GList* sort_model_elements_for_serialization(GHashTable *elements_table) {
  GList *elements_list = g_hash_table_get_values(elements_table);
  return g_list_sort(elements_list, (GCompareFunc)model_compare_for_saving_loading);
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
          if (note->text == NULL || strcmp(note->text, model_element->text->text) != 0) {
            g_free(note->text);
            note->text = g_strdup(model_element->text->text);
          }
          break;
        }
        case ELEMENT_PAPER_NOTE: {
          PaperNote *paper_note = (PaperNote *)visual_element;
          if (paper_note->text == NULL || strcmp(paper_note->text, model_element->text->text) != 0) {
            g_free(paper_note->text);
            paper_note->text = g_strdup(model_element->text->text);
          }
          break;
        }
        case ELEMENT_MEDIA_FILE: {
          MediaNote *media_note = (MediaNote *)visual_element;
          if (media_note->text == NULL || strcmp(media_note->text, model_element->text->text) != 0) {
            g_free(media_note->text);
            media_note->text = g_strdup(model_element->text->text);
          }
          break;
        }
        case ELEMENT_SPACE: {
          SpaceElement *space = (SpaceElement *)visual_element;
          if (space->name == NULL || strcmp(space->name, model_element->text->text) != 0) {
            g_free(space->name);
            space->name = g_strdup(model_element->text->text);
          }
          break;
        }
        case ELEMENT_CONNECTION:
          // Connections typically don't have text
          break;
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

  // Don't free the model here - it's freed in canvas_on_app_shutdown

  g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  // Apply panning transformation
  cairo_translate(cr, data->offset_x, data->offset_y);

  // Canvas color
  cairo_set_source_rgb(cr, 0.094, 0.094, 0.094);
  cairo_paint(cr);

  // Draw current space name in the top-left corner
  if (data->model && data->model->current_space_uuid) {
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text

    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    char space_info[100];
    gchar *space_name = NULL;
    model_get_space_name(data->model, data->model->current_space_uuid, &space_name);
    snprintf(space_info, sizeof(space_info), "Space: %s", space_name);

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
      visual_element = (Element*)note_create(position, bg_color, size, model_element->text->text, data);
    }
    break;

  case ELEMENT_PAPER_NOTE:
    if (model_element->text) {
      visual_element = (Element*)paper_note_create(position, bg_color, size, model_element->text->text, data);
    }
    break;

  case ELEMENT_SPACE: {
    visual_element = (Element*)space_element_create(position, bg_color, size, model_element->text ? model_element->text->text : "Space", data);
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
        visual_element = (Element*)connection_create(from_element, model_element->from_point,
                                                     to_element, model_element->to_point,
                                                     bg_color, position.z, data);

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
      ElementMedia media = {
        .type = MEDIA_TYPE_VIDEO,
        .image_data = model_element->video->thumbnail_data,
        .image_size = model_element->video->thumbnail_size,
        .video_data = model_element->video->video_data,
        .video_size = model_element->video->video_size,
        .duration = model_element->video->duration
      };
      visual_element = (Element*)media_note_create(position, bg_color, size, media, model_element->text->text, data);
    } else if(model_element->image->image_data && model_element->image->image_size > 0) {
      ElementMedia media = {
        .type = MEDIA_TYPE_IMAGE,
        .image_data = model_element->image->image_data,
        .image_size = model_element->image->image_size,
        .video_data = NULL,
        .video_size = 0,
        .duration = 0
      };
      visual_element = (Element*)media_note_create(position, bg_color, size, media, model_element->text->text, data);
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
  *canvas_x = screen_x - data->offset_x;
  *canvas_y = screen_y - data->offset_y;
}

void canvas_canvas_to_screen(CanvasData *data, int canvas_x, int canvas_y, int *screen_x, int *screen_y) {
  *screen_x = canvas_x + data->offset_x;
  *screen_y = canvas_y + data->offset_y;
}
