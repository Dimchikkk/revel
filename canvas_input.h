#ifndef CANVAS_INPUT_H
#define CANVAS_INPUT_H

#include "canvas.h"

void canvas_on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void canvas_on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data);
void canvas_update_cursor(CanvasData *data, int x, int y);
Element* canvas_pick_element(CanvasData *data, int x, int y);
void canvas_set_cursor(CanvasData *data, GdkCursor *cursor);

#endif
