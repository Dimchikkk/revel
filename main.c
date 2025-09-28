#include <stdlib.h>
#include "canvas_core.h"
#include "canvas_input.h"
#include "canvas_actions.h"
#include "canvas_spaces.h"
#include "canvas_search.h"
#include "canvas_drop.h"
#include "freehand_drawing.h"
#include "undo_manager.h"
#include "shape_dialog.h"

// Callback for zoom entry changes
static void on_zoom_entry_activate(GtkEntry *entry, gpointer user_data) {
  CanvasData *data = (CanvasData *)user_data;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Parse the zoom percentage
  char *endptr;
  double zoom_percent = strtod(text, &endptr);

  // Check if we have a valid number
  if (endptr != text && zoom_percent > 0) {
    // Remove % sign if present
    if (*endptr == '%') {
      zoom_percent /= 100.0;
    } else if (zoom_percent > 1.0) {
      // Assume it's a percentage if > 1
      zoom_percent /= 100.0;
    }

    // Clamp to valid zoom range
    if (zoom_percent < 0.1) zoom_percent = 0.1;
    if (zoom_percent > 10.0) zoom_percent = 10.0;

    data->zoom_scale = zoom_percent;
    gtk_widget_queue_draw(data->drawing_area);

    // Update the entry to show the normalized value
    char zoom_text[16];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", zoom_percent * 100);
    gtk_editable_set_text(GTK_EDITABLE(entry), zoom_text);
  } else {
    // Invalid input, reset to current zoom
    char zoom_text[16];
    snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", data->zoom_scale * 100);
    gtk_editable_set_text(GTK_EDITABLE(entry), zoom_text);
  }
}

static void on_activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(window), "revel");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child(GTK_WINDOW(window), vbox);

  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_append(GTK_BOX(vbox), toolbar);

  // === CONTENT CREATION GROUP ===
  GtkWidget *add_paper_btn = gtk_button_new_with_label("New Paper Note");
  GtkWidget *add_note_btn = gtk_button_new_with_label("New Note");
  GtkWidget *add_space_btn = gtk_button_new_with_label("New Space");

  gtk_box_append(GTK_BOX(toolbar), add_paper_btn);
  gtk_box_append(GTK_BOX(toolbar), add_note_btn);
  gtk_box_append(GTK_BOX(toolbar), add_space_btn);

  // Separator after content creation
  GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_box_append(GTK_BOX(toolbar), sep1);

  // === NAVIGATION GROUP ===
  GtkWidget *back_btn = gtk_button_new();
  GtkWidget *back_icon = gtk_image_new_from_icon_name("go-previous");
  gtk_button_set_child(GTK_BUTTON(back_btn), back_icon);
  gtk_widget_set_tooltip_text(back_btn, "Back to Parent");

  GtkWidget *search_btn = gtk_button_new();
  GtkWidget *search_icon = gtk_image_new_from_icon_name("edit-find");
  gtk_button_set_child(GTK_BUTTON(search_btn), search_icon);
  gtk_widget_set_tooltip_text(search_btn, "Search");

  gtk_box_append(GTK_BOX(toolbar), back_btn);
  gtk_box_append(GTK_BOX(toolbar), search_btn);

  // Separator after navigation
  GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_box_append(GTK_BOX(toolbar), sep2);

  // === DRAWING TOOLS GROUP ===
  GtkWidget *drawing_btn = gtk_toggle_button_new_with_label("Draw");
  GtkWidget *color_btn = gtk_color_button_new();
  GtkWidget *width_spin = gtk_spin_button_new_with_range(1, 1000, 1);
  GtkWidget *shapes_btn = gtk_button_new_with_label("Shapes");

  gtk_box_append(GTK_BOX(toolbar), drawing_btn);
  gtk_box_append(GTK_BOX(toolbar), color_btn);
  gtk_box_append(GTK_BOX(toolbar), width_spin);
  gtk_box_append(GTK_BOX(toolbar), shapes_btn);

  // Separator after drawing tools
  GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_box_append(GTK_BOX(toolbar), sep3);

  // === VIEW CONTROLS GROUP ===
  GtkWidget *zoom_label = gtk_label_new("Zoom:");
  GtkWidget *zoom_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(zoom_entry), "100%");
  gtk_editable_set_width_chars(GTK_EDITABLE(zoom_entry), 5);
  gtk_widget_set_hexpand(zoom_entry, FALSE);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(zoom_entry), 5);

  gtk_box_append(GTK_BOX(toolbar), zoom_label);
  gtk_box_append(GTK_BOX(toolbar), zoom_entry);

  // Separator after view controls
  GtkWidget *sep4 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_box_append(GTK_BOX(toolbar), sep4);

  // === UTILITIES GROUP ===
  GtkWidget *log_btn = gtk_button_new_with_label("Log");
  gtk_box_append(GTK_BOX(toolbar), log_btn);

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
  data->zoom_entry = zoom_entry;
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

  // Scroll controller for zoom
  GtkEventController *scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  g_signal_connect(scroll_controller, "scroll", G_CALLBACK(canvas_on_scroll), data);
  gtk_widget_add_controller(drawing_area, scroll_controller);

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
  g_signal_connect(zoom_entry, "activate", G_CALLBACK(on_zoom_entry_activate), data);

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
