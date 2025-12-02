#ifndef CANVAS_INPUT_H
#define CANVAS_INPUT_H

#include "canvas.h"

void canvas_on_left_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void canvas_on_left_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_right_click_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void canvas_on_leave(GtkEventControllerMotion *controller, gpointer user_data);
void canvas_update_cursor(CanvasData *data, int x, int y);
Element* canvas_pick_element(CanvasData *data, int x, int y);
Element* canvas_pick_element_including_locked(CanvasData *data, int x, int y);
void canvas_set_cursor(CanvasData *data, GdkCursor *cursor);
void canvas_on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
gboolean canvas_on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
void canvas_on_paste(GtkWidget *widget, CanvasData *data);
gboolean canvas_on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
gboolean on_window_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void canvas_input_register_event_handlers(CanvasData *data);
void canvas_input_unregister_event_handlers(CanvasData *data);

// Drag gesture handlers (for proper macOS support)
void canvas_on_drag_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data);
void canvas_on_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);
void canvas_on_drag_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);

// Right-click drag gesture handlers (for proper macOS panning support)
void canvas_on_right_drag_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data);
void canvas_on_right_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);
void canvas_on_right_drag_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data);

#endif
