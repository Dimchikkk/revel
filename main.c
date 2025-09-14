#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_actions.h"
#include "canvas_spaces.h"
#include "canvas_search.h"
#include "canvas_drop.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(window), "revel");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child(GTK_WINDOW(window), vbox);

  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_append(GTK_BOX(vbox), toolbar);

  GtkWidget *add_paper_btn = gtk_button_new_with_label("New Paper Note");
  GtkWidget *add_note_btn = gtk_button_new_with_label("New Note");
  gtk_box_append(GTK_BOX(toolbar), add_paper_btn);
  gtk_box_append(GTK_BOX(toolbar), add_note_btn);

  // Add undo/redo buttons to toolbar
  /* GtkWidget *undo_btn = gtk_button_new_with_label("Undo"); */
  /* GtkWidget *redo_btn = gtk_button_new_with_label("Redo"); */
  /* GtkWidget *log_btn = gtk_button_new_with_label("Log"); */
  /* gtk_box_append(GTK_BOX(toolbar), undo_btn); */
  /* gtk_box_append(GTK_BOX(toolbar), redo_btn); */
  /* gtk_box_append(GTK_BOX(toolbar), log_btn); */

  GtkWidget *add_space_btn = gtk_button_new_with_label("New Space");
  gtk_box_append(GTK_BOX(toolbar), add_space_btn);

  // Add back button
  GtkWidget *back_btn = gtk_button_new_with_label("Back to Parent");
  gtk_box_append(GTK_BOX(toolbar), back_btn);

  GtkWidget *search_btn = gtk_button_new_with_label("Search");
  gtk_box_append(GTK_BOX(toolbar), search_btn);

  GtkWidget *overlay = gtk_overlay_new();
  gtk_widget_set_hexpand(overlay, TRUE);
  gtk_widget_set_vexpand(overlay, TRUE);
  gtk_box_append(GTK_BOX(vbox), overlay);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_widget_set_hexpand(drawing_area, TRUE);
  gtk_widget_set_vexpand(drawing_area, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);

  CanvasData *data = canvas_data_new(drawing_area, overlay);
  canvas_setup_drop_target(data);

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), canvas_on_draw, data, NULL);

  GtkEventController *paste_controller = gtk_event_controller_key_new();
  g_signal_connect(paste_controller, "key-pressed", G_CALLBACK(canvas_on_key_pressed), data);
  gtk_widget_add_controller(window, paste_controller);

  // Right-click controller
  GtkGesture *right_click_controller = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click_controller), GDK_BUTTON_SECONDARY);
  g_signal_connect(right_click_controller, "pressed", G_CALLBACK(canvas_on_right_click), data);
  g_signal_connect(right_click_controller, "released", G_CALLBACK(canvas_on_right_click_release), data);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(right_click_controller));

  // Left-click controller
  GtkGesture *left_click_controller = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(left_click_controller), GDK_BUTTON_PRIMARY);
  g_signal_connect(left_click_controller, "pressed", G_CALLBACK(canvas_on_left_click), data);
  g_signal_connect(left_click_controller, "released", G_CALLBACK(canvas_on_left_click_release), data);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(left_click_controller));

  GtkEventController *motion_controller = gtk_event_controller_motion_new();
  g_signal_connect(motion_controller, "motion", G_CALLBACK(canvas_on_motion), data);
  g_signal_connect(motion_controller, "leave", G_CALLBACK(canvas_on_leave), data);
  gtk_widget_add_controller(drawing_area, motion_controller);

  g_signal_connect(add_paper_btn, "clicked", G_CALLBACK(canvas_on_add_paper_note), data);
  g_signal_connect(add_note_btn, "clicked", G_CALLBACK(canvas_on_add_note), data);
  /* g_signal_connect(undo_btn, "clicked", G_CALLBACK(on_undo_clicked), data); */
  /* g_signal_connect(redo_btn, "clicked", G_CALLBACK(on_redo_clicked), data); */
  /* g_signal_connect(log_btn, "clicked", G_CALLBACK(on_log_clicked), data); */
  g_signal_connect(add_space_btn, "clicked", G_CALLBACK(canvas_on_add_space), data);
  g_signal_connect(back_btn, "clicked", G_CALLBACK(canvas_on_go_back), data);
  g_signal_connect(search_btn, "clicked", G_CALLBACK(canvas_show_search_dialog), data);

  g_object_set_data(G_OBJECT(app), "canvas_data", data);
  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("com.example.notecanvas", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  g_signal_connect(app, "shutdown", G_CALLBACK(canvas_on_app_shutdown), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
