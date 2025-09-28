#ifndef CANVAS_ACTIONS_H
#define CANVAS_ACTIONS_H

#include "canvas.h"

void canvas_on_add_note(GtkButton *button, gpointer user_data);
void canvas_on_add_paper_note(GtkButton *button, gpointer user_data);
void canvas_on_add_space(GtkButton *button, gpointer user_data);
void canvas_on_go_back(GtkButton *button, gpointer user_data);
void canvas_toggle_drawing_mode(GtkButton *button, gpointer user_data);
void on_drawing_color_changed(GtkColorButton *button, gpointer user_data);
void on_drawing_width_changed(GtkSpinButton *button, gpointer user_data);
void canvas_show_background_dialog(GtkButton *button, gpointer user_data);

#endif
