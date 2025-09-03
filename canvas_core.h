#ifndef CANVAS_CORE_H
#define CANVAS_CORE_H

#include "canvas.h"

CanvasData* canvas_data_new(GtkWidget *drawing_area, GtkWidget *overlay);
void canvas_data_free(CanvasData *data);
void canvas_on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data);
void canvas_delete_selected(CanvasData *data);
void canvas_clear_selection(CanvasData *data);
gboolean canvas_is_element_selected(CanvasData *data, Element *element);
void canvas_on_app_shutdown(GApplication *app, gpointer user_data);

#endif
