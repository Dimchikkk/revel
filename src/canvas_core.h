#ifndef CANVAS_CORE_H
#define CANVAS_CORE_H

#include "canvas.h"
#include "element.h"

CanvasData* canvas_data_new_with_db(GtkWidget *drawing_area, GtkWidget *overlay, const char *db_filename);
void canvas_data_free(CanvasData *data);
void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);
void canvas_clear_selection(CanvasData *data);
gboolean canvas_is_element_selected(CanvasData *data, Element *element);
void canvas_on_app_shutdown(GApplication *app, gpointer user_data);

// Expects connections to be last in elements array
Element* create_visual_element(ModelElement *model_element, CanvasData *canvas_data);
GList *canvas_get_visual_elements(CanvasData *data);
// Recreates all visual elements from the model, sorted for proper serialization
//
// This function sorts the model elements (with connections last) and creates
// visual elements from the sorted list. Useful for refreshing the canvas display.
void canvas_sync_with_model(CanvasData *canvas_data);

// Rebuild the spatial index (quadtree) with current visual elements
void canvas_rebuild_quadtree(CanvasData *canvas_data);

void canvas_screen_to_canvas(CanvasData *data, int screen_x, int screen_y,
                             int *canvas_x, int *canvas_y);
void canvas_canvas_to_screen(CanvasData *data, int canvas_x, int canvas_y, int *screen_x, int *screen_y);
void canvas_set_cursor(CanvasData *data, GdkCursor *cursor);
void canvas_update_zoom_entry(CanvasData *data);

// Hide/show children functionality
void canvas_hide_children(CanvasData *data, const char *parent_uuid);
void canvas_show_children(CanvasData *data, const char *parent_uuid);
gboolean canvas_is_element_hidden(CanvasData *data, const char *element_uuid);
gboolean canvas_has_hidden_children(CanvasData *data, const char *parent_uuid);

// Space name visibility toggle
void canvas_toggle_space_name_visibility(GtkToggleButton *button, gpointer user_data);

// Toolbar functions
void toggle_toolbar_visibility(CanvasData *data);
void toggle_toolbar_auto_hide(CanvasData *data);
void show_toolbar(CanvasData *data);
void on_zoom_entry_activate(GtkEntry *entry, gpointer user_data);

// Notification popup
void canvas_show_notification(CanvasData *data, const char *message);

#endif
