#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_spaces.h"
#include "connection.h"
#include "element.h"
#include "paper_note.h"
#include "image_note.h"
#include "note.h"
#include <pango/pangocairo.h>
#include "model.h"

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

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

  data->model = model_new();

  if (data->model != NULL && data->model->db != NULL) canvas_recreate_visual_elements(data);

  return data;
}

GList* sort_model_elements_for_serialization(GHashTable *elements_table) {
    GList *elements_list = g_hash_table_get_values(elements_table);
    return g_list_sort(elements_list, (GCompareFunc)model_compare_for_saving_loading);
}

void create_visual_elements_from_sorted_list(GList *sorted_elements, CanvasData *data) {
    GList *iter = sorted_elements;
    while (iter != NULL) {
        ModelElement *model_element = (ModelElement*)iter->data;

        // Create visual element from model element
        Element *visual_element = create_visual_element(model_element, data);
        if (visual_element) {
            model_element->visual_element = visual_element;

            if (model_element->position && model_element->position->z >= data->next_z_index) {
                data->next_z_index = model_element->position->z + 1;
            }
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

static gint compare_model_elements_by_z(const ModelElement *a, const ModelElement *b) {
    if (!a || !a->position) return 1;
    if (!b || !b->position) return -1;
    return a->position->z - b->position->z;
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    cairo_set_source_rgb(cr, 0.32, 0.32, 0.36);
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

    GList *model_elements_list = g_hash_table_get_values(data->model->elements);
    GList *sorted_model_elements = g_list_sort(model_elements_list, (GCompareFunc)compare_model_elements_by_z);

    for (GList *l = sorted_model_elements; l != NULL; l = l->next) {
      ModelElement *model_element = (ModelElement*)l->data;

      // Skip deleted elements and elements without visual representation
      if (model_element->state == MODEL_STATE_DELETED || !model_element->visual_element) {
        continue;
      }

      // Draw the visual element
      element_draw(model_element->visual_element, cr, canvas_is_element_selected(data, model_element->visual_element));
    }

    g_list_free(sorted_model_elements);

    if (data->selecting) {
        cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.3);
        cairo_rectangle(cr,
                        MIN(data->start_x, data->current_x),
                        MIN(data->start_y, data->current_y),
                        ABS(data->current_x - data->start_x),
                        ABS(data->current_y - data->start_y));
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    }
}

void canvas_delete_selected(CanvasData *data) {
    if (!data->selected_elements) return;
    canvas_clear_selection(data);
    gtk_widget_queue_draw(data->drawing_area);
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

    Element *visual_element = NULL;

    switch (model_element->type->type) {
        case ELEMENT_NOTE:
            if (model_element->text && model_element->position && model_element->size) {
                visual_element = (Element*)note_create(
                    model_element->position->x,
                    model_element->position->y,
                    model_element->position->z,
                    model_element->size->width,
                    model_element->size->height,
                    model_element->text->text,
                    data
                );
            }
            break;

        case ELEMENT_PAPER_NOTE:
            if (model_element->text && model_element->position && model_element->size) {
                visual_element = (Element*)paper_note_create(
                    model_element->position->x,
                    model_element->position->y,
                    model_element->position->z,
                    model_element->size->width,
                    model_element->size->height,
                    model_element->text->text,
                    data
                );
            }
            break;

        case ELEMENT_SPACE:
            if (model_element->position && model_element->size) {
                visual_element = (Element*)space_element_create(
                    model_element->position->x,
                    model_element->position->y,
                    model_element->position->z,
                    model_element->size->width,
                    model_element->size->height,
                    model_element->text ? model_element->text->text : "Space",
                    data
                );
            }
            break;

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
                    visual_element = (Element*)connection_create(
                        from_element,
                        model_element->from_point,
                        to_element,
                        model_element->to_point,
                        model_element->position ? model_element->position->z : data->next_z_index++,
                        data
                    );

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
        case ELEMENT_IMAGE_NOTE:
            if (model_element->position && model_element->size &&
                model_element->image_data && model_element->image_size > 0) {
                visual_element = (Element*)image_note_create(
                    model_element->position->x,
                    model_element->position->y,
                    model_element->position->z,
                    model_element->size->width,
                    model_element->size->height,
                    model_element->image_data,
                    model_element->image_size,
                    model_element->text->text,
                    data
                );
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

        // Only include elements that belong to the current space
        if (model_element->visual_element != NULL &&
            g_strcmp0(model_element->space_uuid, data->model->current_space_uuid) == 0) {
            visual_elements = g_list_append(visual_elements, model_element->visual_element);
        }
    }

    return visual_elements;
}

void canvas_recreate_visual_elements(CanvasData *canvas_data) {
  if (!canvas_data || !canvas_data->model || !canvas_data->model->elements) {
    return;
  }

  GList *sorted_elements = sort_model_elements_for_serialization(canvas_data->model->elements);
  create_visual_elements_from_sorted_list(sorted_elements, canvas_data);
  g_list_free(sorted_elements);
}
