#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_actions.h"
#include "canvas_spaces.h"
#include "canvas_search.h"
#include "canvas_drop.h"
#include "freehand_drawing.h"
#include "undo_manager.h"
#include "shape_dialog.h"

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

  GtkWidget *add_space_btn = gtk_button_new_with_label("New Space");
  gtk_box_append(GTK_BOX(toolbar), add_space_btn);

  GtkWidget *back_btn = gtk_button_new_with_label("Back to Parent");
  gtk_box_append(GTK_BOX(toolbar), back_btn);

  GtkWidget *log_btn = gtk_button_new_with_label("Log");
  gtk_box_append(GTK_BOX(toolbar), log_btn);

  GtkWidget *search_btn = gtk_button_new_with_label("Search");
  gtk_box_append(GTK_BOX(toolbar), search_btn);

  GtkWidget *drawing_btn = gtk_toggle_button_new_with_label("Draw");
  GtkWidget *color_btn = gtk_color_button_new();
  GtkWidget *width_spin = gtk_spin_button_new_with_range(1, 1000, 1);
  GtkWidget *shapes_btn = gtk_button_new_with_label("Shapes");

  gtk_box_append(GTK_BOX(toolbar), drawing_btn);
  gtk_box_append(GTK_BOX(toolbar), color_btn);
  gtk_box_append(GTK_BOX(toolbar), width_spin);
  gtk_box_append(GTK_BOX(toolbar), shapes_btn);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(width_spin), 3);
  GdkRGBA initial_color = INITIAL_DRAWING_COLOR;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_btn), &initial_color);

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
  g_signal_connect(log_btn, "clicked", G_CALLBACK(on_log_clicked), data);
  g_signal_connect(add_space_btn, "clicked", G_CALLBACK(canvas_on_add_space), data);
  g_signal_connect(back_btn, "clicked", G_CALLBACK(canvas_on_go_back), data);
  g_signal_connect(search_btn, "clicked", G_CALLBACK(canvas_show_search_dialog), data);
  g_signal_connect(drawing_btn, "clicked", G_CALLBACK(canvas_toggle_drawing_mode), data);
  g_signal_connect(color_btn, "color-set", G_CALLBACK(on_drawing_color_changed), data);
  g_signal_connect(width_spin, "value-changed", G_CALLBACK(on_drawing_width_changed), data);
  g_signal_connect(shapes_btn, "clicked", G_CALLBACK(canvas_show_shape_selection_dialog), data);

  g_object_set_data(G_OBJECT(app), "canvas_data", data);

  GtkCssProvider *provider = gtk_css_provider_new();

  const char *css =
    "textview {"
    "   font-size: 20px;"
    "   font-family: Ubuntu Mono;"
    "   font-weight: normal;"
    "}";

  gtk_css_provider_load_from_data(provider, css, -1);

  gtk_style_context_add_provider_for_display(
                                             gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                                             );

  g_object_unref(provider);

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
