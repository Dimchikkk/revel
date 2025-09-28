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
#include "database.h"

// Toolbar auto-hide functions
static gboolean hide_toolbar_timeout(gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (data->toolbar_auto_hide && data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), FALSE);
    data->toolbar_visible = FALSE;
  }

  data->toolbar_hide_timer_id = 0;
  return G_SOURCE_REMOVE;
}

static void show_toolbar(CanvasData *data) {
  if (!data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), TRUE);
    data->toolbar_visible = TRUE;
  }

  // Reset hide timer
  if (data->toolbar_hide_timer_id > 0) {
    g_source_remove(data->toolbar_hide_timer_id);
  }

  if (data->toolbar_auto_hide) {
    data->toolbar_hide_timer_id = g_timeout_add(3000, hide_toolbar_timeout, data);
  }
}

void toggle_toolbar_visibility(CanvasData *data) {
  if (data->toolbar_visible) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(data->toolbar_revealer), FALSE);
    data->toolbar_visible = FALSE;
    if (data->toolbar_hide_timer_id > 0) {
      g_source_remove(data->toolbar_hide_timer_id);
      data->toolbar_hide_timer_id = 0;
    }
  } else {
    show_toolbar(data);
  }
}

void toggle_toolbar_auto_hide(CanvasData *data) {
  data->toolbar_auto_hide = !data->toolbar_auto_hide;

  if (data->toolbar_auto_hide) {
    // Start auto-hide timer
    if (data->toolbar_visible) {
      show_toolbar(data); // This will set the timer
    }
  } else {
    // Cancel auto-hide timer and ensure toolbar is visible
    if (data->toolbar_hide_timer_id > 0) {
      g_source_remove(data->toolbar_hide_timer_id);
      data->toolbar_hide_timer_id = 0;
    }
    show_toolbar(data);
  }
}

static gboolean on_window_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Get window height to detect bottom edge
  GtkRoot *root = gtk_widget_get_root(data->drawing_area);
  GtkWidget *window = GTK_WIDGET(root);
  int window_height = gtk_widget_get_height(window);

  // Show toolbar when mouse is near the bottom edge (within 5 pixels)
  if (data->toolbar_auto_hide && y >= (window_height - 5)) {
    show_toolbar(data);
  }

  return FALSE;
}

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


// Callback for background dialog response
static void background_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (response_id == GTK_RESPONSE_OK) {
    GtkWidget *color_button = g_object_get_data(G_OBJECT(dialog), "color_button");
    GtkWidget *grid_checkbox = g_object_get_data(G_OBJECT(dialog), "grid_checkbox");
    GtkWidget *grid_color_button = g_object_get_data(G_OBJECT(dialog), "grid_color_button");

    if (data->model && data->model->current_space_uuid && data->model->db) {
      // Set background color
      GdkRGBA color;
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), &color);

      // Convert to hex string
      char hex_color[8];
      snprintf(hex_color, sizeof(hex_color), "#%02x%02x%02x",
              (int)(color.red * 255), (int)(color.green * 255), (int)(color.blue * 255));

      model_set_space_background_color(data->model, data->model->current_space_uuid, hex_color);

      // Save grid settings to database
      gboolean grid_enabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(grid_checkbox));
      GdkRGBA grid_color;
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(grid_color_button), &grid_color);

      char grid_color_hex[8];
      snprintf(grid_color_hex, sizeof(grid_color_hex), "#%02x%02x%02x",
              (int)(grid_color.red * 255), (int)(grid_color.green * 255), (int)(grid_color.blue * 255));

      model_set_space_grid_settings(data->model, data->model->current_space_uuid,
                                      grid_enabled, grid_color_hex);
      gtk_widget_queue_draw(data->drawing_area);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

// Callback for background button click
static void canvas_show_background_dialog(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Canvas Background",
    NULL,
    GTK_DIALOG_MODAL,
    "_Cancel", GTK_RESPONSE_CANCEL,
    "_OK", GTK_RESPONSE_OK,
    NULL
  );

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append(GTK_BOX(content_area), vbox);
  gtk_widget_set_margin_start(vbox, 12);
  gtk_widget_set_margin_end(vbox, 12);
  gtk_widget_set_margin_top(vbox, 12);
  gtk_widget_set_margin_bottom(vbox, 12);

  // Color option
  GtkWidget *color_label = gtk_label_new("Background Color:");
  gtk_widget_set_halign(color_label, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(vbox), color_label);

  GtkWidget *color_button = gtk_color_button_new();
  gtk_widget_set_margin_start(color_button, 20);
  gtk_box_append(GTK_BOX(vbox), color_button);

  // Grid option
  GtkWidget *grid_checkbox = gtk_check_button_new_with_label("Show Grid");
  gtk_box_append(GTK_BOX(vbox), grid_checkbox);

  // Grid color option
  GtkWidget *grid_color_label = gtk_label_new("Grid Color:");
  gtk_widget_set_halign(grid_color_label, GTK_ALIGN_START);
  gtk_widget_set_margin_start(grid_color_label, 20);
  gtk_box_append(GTK_BOX(vbox), grid_color_label);

  GtkWidget *grid_color_button = gtk_color_button_new();
  gtk_widget_set_margin_start(grid_color_button, 20);
  gtk_box_append(GTK_BOX(vbox), grid_color_button);

  // Set default grid color to light gray
  GdkRGBA default_grid_color = {0.8, 0.8, 0.8, 1.0};
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(grid_color_button), &default_grid_color);

  // Load current background settings
  if (data->model && data->model->current_space_uuid) {
    if (data->model->current_space_background_color) {
      GdkRGBA color;
      if (gdk_rgba_parse(&color, data->model->current_space_background_color)) {
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button), &color);
      }
    }

    // Load grid settings from model
    gtk_check_button_set_active(GTK_CHECK_BUTTON(grid_checkbox), data->model->current_space_show_grid);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(grid_color_button), &data->model->current_space_grid_color);
  }


  // Store widgets for response callback
  g_object_set_data(G_OBJECT(dialog), "color_button", color_button);
  g_object_set_data(G_OBJECT(dialog), "grid_checkbox", grid_checkbox);
  g_object_set_data(G_OBJECT(dialog), "grid_color_button", grid_color_button);

  g_signal_connect(dialog, "response", G_CALLBACK(background_dialog_response), data);

  gtk_widget_set_visible(dialog, TRUE);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(window), "revel");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child(GTK_WINDOW(window), vbox);

  // Create toolbar revealer first
  GtkWidget *toolbar_revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(toolbar_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration(GTK_REVEALER(toolbar_revealer), 300);
  gtk_revealer_set_reveal_child(GTK_REVEALER(toolbar_revealer), TRUE);

  // Create main toolbar with improved styling
  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(toolbar, 8);
  gtk_widget_set_margin_end(toolbar, 8);
  gtk_widget_set_margin_top(toolbar, 4);
  gtk_widget_set_margin_bottom(toolbar, 4);
  gtk_revealer_set_child(GTK_REVEALER(toolbar_revealer), toolbar);

  // === CONTENT CREATION GROUP ===
  GtkWidget *create_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(create_group, "toolbar-group");

  // Paper Note button with icon
  GtkWidget *add_paper_btn = gtk_button_new();
  GtkWidget *paper_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *paper_icon = gtk_image_new_from_icon_name("accessories-text-editor");
  GtkWidget *paper_label = gtk_label_new("Paper");
  gtk_box_append(GTK_BOX(paper_box), paper_icon);
  gtk_box_append(GTK_BOX(paper_box), paper_label);
  gtk_button_set_child(GTK_BUTTON(add_paper_btn), paper_box);
  gtk_widget_set_tooltip_text(add_paper_btn, "Create New Paper Note (Ctrl+Shift+P)");

  // Rich Note button with icon
  GtkWidget *add_note_btn = gtk_button_new();
  GtkWidget *note_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *note_icon = gtk_image_new_from_icon_name("text-x-generic");
  GtkWidget *note_label = gtk_label_new("Note");
  gtk_box_append(GTK_BOX(note_box), note_icon);
  gtk_box_append(GTK_BOX(note_box), note_label);
  gtk_button_set_child(GTK_BUTTON(add_note_btn), note_box);
  gtk_widget_set_tooltip_text(add_note_btn, "Create New Rich Note (Ctrl+N)");

  // Space button with icon
  GtkWidget *add_space_btn = gtk_button_new();
  GtkWidget *space_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *space_icon = gtk_image_new_from_icon_name("folder-new");
  GtkWidget *space_label = gtk_label_new("Space");
  gtk_box_append(GTK_BOX(space_box), space_icon);
  gtk_box_append(GTK_BOX(space_box), space_label);
  gtk_button_set_child(GTK_BUTTON(add_space_btn), space_box);
  gtk_widget_set_tooltip_text(add_space_btn, "Create New Space (Ctrl+Shift+S)");

  gtk_box_append(GTK_BOX(create_group), add_paper_btn);
  gtk_box_append(GTK_BOX(create_group), add_note_btn);
  gtk_box_append(GTK_BOX(create_group), add_space_btn);
  gtk_box_append(GTK_BOX(toolbar), create_group);

  // Group separator
  GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep1, 4);
  gtk_widget_set_margin_end(sep1, 4);
  gtk_box_append(GTK_BOX(toolbar), sep1);

  // === NAVIGATION GROUP ===
  GtkWidget *nav_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(nav_group, "toolbar-group");

  GtkWidget *back_btn = gtk_button_new();
  GtkWidget *back_icon = gtk_image_new_from_icon_name("go-previous");
  gtk_button_set_child(GTK_BUTTON(back_btn), back_icon);
  gtk_widget_set_tooltip_text(back_btn, "Back to Parent Space (Backspace)");

  GtkWidget *search_btn = gtk_button_new();
  GtkWidget *search_icon = gtk_image_new_from_icon_name("edit-find");
  gtk_button_set_child(GTK_BUTTON(search_btn), search_icon);
  gtk_widget_set_tooltip_text(search_btn, "Search Elements (Ctrl+S)");

  gtk_box_append(GTK_BOX(nav_group), back_btn);
  gtk_box_append(GTK_BOX(nav_group), search_btn);
  gtk_box_append(GTK_BOX(toolbar), nav_group);

  // Group separator
  GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep2, 4);
  gtk_widget_set_margin_end(sep2, 4);
  gtk_box_append(GTK_BOX(toolbar), sep2);

  // === DRAWING TOOLS GROUP ===
  GtkWidget *draw_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(draw_group, "toolbar-group");

  // Drawing toggle button with icon
  GtkWidget *drawing_btn = gtk_toggle_button_new();
  GtkWidget *draw_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *draw_icon = gtk_image_new_from_icon_name("applications-graphics");
  GtkWidget *draw_label = gtk_label_new("Draw");
  gtk_box_append(GTK_BOX(draw_box), draw_icon);
  gtk_box_append(GTK_BOX(draw_box), draw_label);
  gtk_button_set_child(GTK_BUTTON(drawing_btn), draw_box);
  gtk_widget_set_tooltip_text(drawing_btn, "Toggle Drawing Mode (Ctrl+D)");

  // Color picker
  GtkWidget *color_btn = gtk_color_button_new();
  gtk_widget_set_tooltip_text(color_btn, "Drawing Color");

  // Stroke width with label
  GtkWidget *width_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  GtkWidget *width_label = gtk_label_new("Width:");
  gtk_label_set_attributes(GTK_LABEL(width_label), NULL);
  GtkWidget *width_spin = gtk_spin_button_new_with_range(1, 100, 1);
  gtk_editable_set_width_chars(GTK_EDITABLE(width_spin), 3);
  gtk_widget_set_tooltip_text(width_spin, "Stroke Width");
  gtk_box_append(GTK_BOX(width_box), width_label);
  gtk_box_append(GTK_BOX(width_box), width_spin);

  // Shapes button with icon
  GtkWidget *shapes_btn = gtk_button_new();
  GtkWidget *shapes_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *shapes_icon = gtk_image_new_from_icon_name("insert-object");
  GtkWidget *shapes_label = gtk_label_new("Shapes");
  gtk_box_append(GTK_BOX(shapes_box), shapes_icon);
  gtk_box_append(GTK_BOX(shapes_box), shapes_label);
  gtk_button_set_child(GTK_BUTTON(shapes_btn), shapes_box);
  gtk_widget_set_tooltip_text(shapes_btn, "Insert Shapes");

  gtk_box_append(GTK_BOX(draw_group), drawing_btn);
  gtk_box_append(GTK_BOX(draw_group), color_btn);
  gtk_box_append(GTK_BOX(draw_group), width_box);
  gtk_box_append(GTK_BOX(draw_group), shapes_btn);
  gtk_box_append(GTK_BOX(toolbar), draw_group);

  // Group separator
  GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep3, 4);
  gtk_widget_set_margin_end(sep3, 4);
  gtk_box_append(GTK_BOX(toolbar), sep3);

  // === VIEW CONTROLS GROUP ===
  GtkWidget *view_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_add_css_class(view_group, "toolbar-group");

  // Zoom controls
  GtkWidget *zoom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  GtkWidget *zoom_icon = gtk_image_new_from_icon_name("zoom-in");
  GtkWidget *zoom_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(zoom_entry), "100%");
  gtk_editable_set_width_chars(GTK_EDITABLE(zoom_entry), 5);
  gtk_widget_set_hexpand(zoom_entry, FALSE);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(zoom_entry), 5);
  gtk_widget_set_tooltip_text(zoom_entry, "Zoom Level");
  gtk_box_append(GTK_BOX(zoom_box), zoom_icon);
  gtk_box_append(GTK_BOX(zoom_box), zoom_entry);

  // Background button with icon
  GtkWidget *background_btn = gtk_button_new();
  GtkWidget *bg_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *bg_icon = gtk_image_new_from_icon_name("preferences-desktop-wallpaper");
  GtkWidget *bg_label = gtk_label_new("Background");
  gtk_box_append(GTK_BOX(bg_box), bg_icon);
  gtk_box_append(GTK_BOX(bg_box), bg_label);
  gtk_button_set_child(GTK_BUTTON(background_btn), bg_box);
  gtk_widget_set_tooltip_text(background_btn, "Change Canvas Background & Grid");

  // Space name toggle button with icon only
  GtkWidget *space_name_btn = gtk_toggle_button_new();
  GtkWidget *space_name_icon = gtk_image_new_from_icon_name("text-x-generic");
  gtk_button_set_child(GTK_BUTTON(space_name_btn), space_name_icon);
  gtk_widget_set_tooltip_text(space_name_btn, "Toggle Space Name Display");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(space_name_btn), TRUE); // Default to shown

  gtk_box_append(GTK_BOX(view_group), zoom_box);
  gtk_box_append(GTK_BOX(view_group), space_name_btn);
  gtk_box_append(GTK_BOX(view_group), background_btn);
  gtk_box_append(GTK_BOX(toolbar), view_group);

  // Group separator
  GtkWidget *sep4 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep4, 4);
  gtk_widget_set_margin_end(sep4, 4);
  gtk_box_append(GTK_BOX(toolbar), sep4);

  // === UTILITIES GROUP ===
  GtkWidget *utils_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(utils_group, "toolbar-group");

  GtkWidget *log_btn = gtk_button_new();
  GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *log_icon = gtk_image_new_from_icon_name("utilities-terminal");
  GtkWidget *log_label = gtk_label_new("Log");
  gtk_box_append(GTK_BOX(log_box), log_icon);
  gtk_box_append(GTK_BOX(log_box), log_label);
  gtk_button_set_child(GTK_BUTTON(log_btn), log_box);
  gtk_widget_set_tooltip_text(log_btn, "View Action Log");

  gtk_box_append(GTK_BOX(utils_group), log_btn);
  gtk_box_append(GTK_BOX(toolbar), utils_group);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(width_spin), 3);
  GdkRGBA initial_color = INITIAL_DRAWING_COLOR;
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_btn), &initial_color);

  GtkWidget *overlay = gtk_overlay_new();
  gtk_widget_set_hexpand(overlay, TRUE);
  gtk_widget_set_vexpand(overlay, TRUE);
  gtk_box_append(GTK_BOX(vbox), overlay);

  // Add toolbar revealer at the bottom
  gtk_box_append(GTK_BOX(vbox), toolbar_revealer);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_widget_set_hexpand(drawing_area, TRUE);
  gtk_widget_set_vexpand(drawing_area, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);

  CanvasData *data = canvas_data_new(drawing_area, overlay);
  data->zoom_entry = zoom_entry;
  data->toolbar = toolbar;
  data->toolbar_revealer = toolbar_revealer;
  data->toolbar_visible = TRUE;
  data->toolbar_auto_hide = FALSE;
  data->toolbar_hide_timer_id = 0;
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

  // Window motion controller for toolbar auto-show
  GtkEventController *window_motion_controller = gtk_event_controller_motion_new();
  g_signal_connect(window_motion_controller, "motion", G_CALLBACK(on_window_motion), data);
  gtk_widget_add_controller(window, window_motion_controller);

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
  g_signal_connect(background_btn, "clicked", G_CALLBACK(canvas_show_background_dialog), data);
  g_signal_connect(space_name_btn, "toggled", G_CALLBACK(canvas_toggle_space_name_visibility), data);
  g_signal_connect(zoom_entry, "activate", G_CALLBACK(on_zoom_entry_activate), data);

  g_object_set_data(G_OBJECT(app), "canvas_data", data);

  GtkCssProvider *provider = gtk_css_provider_new();

  const char *css =
    "textview {"
    "   font-size: 20px;"
    "   font-family: Ubuntu Mono;"
    "   font-weight: normal;"
    "}"
    ".toolbar-group {"
    "   background-color: rgba(255, 255, 255, 0.05);"
    "   border-radius: 8px;"
    "   padding: 4px;"
    "   margin: 2px;"
    "   border: 1px solid rgba(255, 255, 255, 0.1);"
    "}"
    ".toolbar-group button {"
    "   border-radius: 6px;"
    "   margin: 1px;"
    "   padding: 6px 8px;"
    "}"
    ".toolbar-group button:hover {"
    "   background-color: rgba(255, 255, 255, 0.1);"
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
