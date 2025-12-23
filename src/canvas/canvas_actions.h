#ifndef CANVAS_ACTIONS_H
#define CANVAS_ACTIONS_H

#include "canvas.h"

void canvas_on_add_note(GtkButton *button, gpointer user_data);
void canvas_on_add_paper_note(GtkButton *button, gpointer user_data);
void canvas_on_add_text(GtkButton *button, gpointer user_data);
void canvas_on_add_inline_text(GtkButton *button, gpointer user_data);
void canvas_on_add_space(GtkButton *button, gpointer user_data);
void canvas_on_go_back(GtkButton *button, gpointer user_data);
void canvas_toggle_drawing_mode(GtkButton *button, gpointer user_data);
void canvas_toggle_tree_view(GtkToggleButton *button, gpointer user_data);
void on_drawing_color_changed(GtkColorButton *button, gpointer user_data);
void on_stroke_color_changed(GtkColorButton *button, gpointer user_data);
void on_text_color_changed(GtkColorButton *button, gpointer user_data);
void on_background_color_changed(GtkColorButton *button, gpointer user_data);
void on_drawing_width_changed(GtkSpinButton *button, gpointer user_data);
void canvas_show_background_dialog(GtkButton *button, gpointer user_data);
void canvas_update_toolbar_colors_from_selection(CanvasData *data);

#endif
