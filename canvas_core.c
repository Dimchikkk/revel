#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_spaces.h"
#include "paper_note.h"
#include "note.h"
#include <pango/pangocairo.h>
#include "undo_manager.h"

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static gint compare_elements_by_z_index(gconstpointer a, gconstpointer b) {
    const Element *element_a = (const Element*)a;
    const Element *element_b = (const Element*)b;
    return element_a->z_index - element_b->z_index;
}

CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay) {
    CanvasData *data = g_new0(CanvasData, 1);
    data->selected_elements = NULL;
    data->drawing_area = drawing_area;
    data->overlay = overlay;
    data->next_z_index = 1;
    data->selecting = FALSE;
    data->modifier_state = 0;

    data->default_cursor = gdk_cursor_new_from_name("default", NULL);
    data->move_cursor = gdk_cursor_new_from_name("move", NULL);
    data->resize_cursor = gdk_cursor_new_from_name("nwse-resize", NULL);
    data->connect_cursor = gdk_cursor_new_from_name("crosshair", NULL);
    data->current_cursor = NULL;
    data->undo_manager = undo_manager_new();

    // Initialize undo tracking fields
    data->undo_original_width = 0;
    data->undo_original_height = 0;
    data->undo_original_x = 0;
    data->undo_original_y = 0;
    data->undo_original_positions = NULL;

    data->current_space = space_new("root", NULL);
    data->space_history = NULL;

    return data;
}

void canvas_data_free(CanvasData *data) {
    if (data->default_cursor) g_object_unref(data->default_cursor);
    if (data->move_cursor) g_object_unref(data->move_cursor);
    if (data->resize_cursor) g_object_unref(data->resize_cursor);
    if (data->connect_cursor) g_object_unref(data->connect_cursor);

    if (data->current_space && data->current_space->parent == NULL) {
        space_free(data->current_space);
    }

    g_list_free(data->selected_elements);

    // Free undo position data
    if (data->undo_original_positions) {
        g_list_free_full(data->undo_original_positions, g_free);
    }

    if (data->undo_manager) undo_manager_free(data->undo_manager);
    g_free(data);
}

void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);

    // Draw current space name in the top-left corner
    if (data->current_space && data->current_space->name) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);  // Dark gray text

        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 10");
        pango_layout_set_font_description(layout, font_desc);
        pango_font_description_free(font_desc);

        char space_info[100];
        if (data->current_space->parent) {
            snprintf(space_info, sizeof(space_info), "Space: %s (in %s)",
                     data->current_space->name, data->current_space->parent->name);
        } else {
            snprintf(space_info, sizeof(space_info), "Space: %s", data->current_space->name);
        }

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

    GList *sorted_elements = g_list_copy(data->current_space->elements);
    sorted_elements = g_list_sort(sorted_elements, compare_elements_by_z_index);

    for (GList *l = sorted_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        if (!element->hidden) {
            element_draw(element, cr, canvas_is_element_selected(data, element));
        }
    }

    g_list_free(sorted_elements);

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

    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;

        // Push delete action before actually hiding
        undo_manager_push_delete_action(data->undo_manager, element);

        element->hidden = TRUE;
    }

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
        canvas_data_free(data);
        g_object_set_data(G_OBJECT(app), "canvas_data", NULL);
    }
}
