#include <stdlib.h>
#include "canvas_core.h"
#include "canvas_input.h"
#include "ui_event_bus.h"
#include "canvas_actions.h"
#include "canvas_spaces.h"
#include "canvas_search.h"
#include "canvas_drop.h"
#include "canvas_space_tree.h"
#include "canvas_presentation.h"
#include "freehand_drawing.h"
#include "undo_manager.h"
#include "shape_dialog.h"
#include "database.h"
#include "dsl_executor.h"
#include "ui/dialogs/ai_chat_dialog.h"

// Global variables to store command line options
static char *g_database_filename = NULL;
static char *g_dsl_filename = NULL;

static int on_command_line(GtkApplication *app, GApplicationCommandLine *command_line, gpointer user_data) {
  gint argc;
  gchar **argv = g_application_command_line_get_arguments(command_line, &argc);

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], "--dsl") == 0 && i + 1 < argc) {
      // Set the DSL filename
      if (g_dsl_filename) {
        g_free(g_dsl_filename);
      }
      g_dsl_filename = g_strdup(argv[i + 1]);
      i++; // Skip next arg since we consumed it
    } else if (argv[i][0] != '-') {
      // If not a flag, treat as database filename
      if (g_database_filename) {
        g_free(g_database_filename);
      }
      g_database_filename = g_strdup(argv[i]);
    }
  }

  g_strfreev(argv);

  // Activate the application
  g_application_activate(G_APPLICATION(app));

  return 0;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
  gtk_widget_set_size_request(window, 200, 200);  // Set minimum window size
  gtk_window_set_title(GTK_WINDOW(window), "revel");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(window), vbox);

  // Create toolbar revealer first
  GtkWidget *toolbar_revealer = gtk_revealer_new();
  gtk_revealer_set_transition_type(GTK_REVEALER(toolbar_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration(GTK_REVEALER(toolbar_revealer), 300);
  gtk_revealer_set_reveal_child(GTK_REVEALER(toolbar_revealer), TRUE);

  // Wrap toolbar in a scrolled window to allow horizontal scrolling
  GtkWidget *toolbar_scroll = gtk_scrolled_window_new();
  gtk_widget_set_name(toolbar_scroll, "toolbar-scroll");
  gtk_widget_set_hexpand(toolbar_scroll, TRUE);
  gtk_widget_set_halign(toolbar_scroll, GTK_ALIGN_FILL);
  gtk_widget_set_margin_start(toolbar_scroll, 0);
  gtk_widget_set_margin_end(toolbar_scroll, 0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(toolbar_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(toolbar_scroll), FALSE);
  gtk_widget_set_vexpand(toolbar_scroll, FALSE);
  gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(toolbar_scroll), FALSE);

  // Create main toolbar with improved styling
  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_hexpand(toolbar, TRUE);
  gtk_widget_set_halign(toolbar, GTK_ALIGN_FILL);
  gtk_widget_set_margin_top(toolbar, 0);
  gtk_widget_set_margin_bottom(toolbar, 0);
  gtk_widget_set_margin_start(toolbar, 0);
  gtk_widget_set_margin_end(toolbar, 0);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(toolbar_scroll), toolbar);

  GtkWidget *toolbar_background = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name(toolbar_background, "toolbar-background");
  gtk_widget_set_hexpand(toolbar_background, TRUE);
  gtk_widget_set_halign(toolbar_background, GTK_ALIGN_FILL);
  gtk_widget_set_margin_top(toolbar_background, 0);
  gtk_widget_set_margin_bottom(toolbar_background, 0);
  gtk_widget_set_margin_start(toolbar_background, 0);
  gtk_widget_set_margin_end(toolbar_background, 0);

  gtk_box_append(GTK_BOX(toolbar_background), toolbar_scroll);
  gtk_revealer_set_child(GTK_REVEALER(toolbar_revealer), toolbar_background);

  // === AI ASSISTANT GROUP ===
  GtkWidget *ai_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_add_css_class(ai_group, "toolbar-group");
  gtk_widget_set_margin_end(ai_group, 4);

  GtkWidget *ai_btn = gtk_toggle_button_new_with_label("AI");
  gtk_widget_set_tooltip_text(ai_btn, "Open AI Assistant");
  gtk_box_append(GTK_BOX(ai_group), ai_btn);
  gtk_box_append(GTK_BOX(toolbar), ai_group);

  // === CONTENT CREATION GROUP ===
  GtkWidget *create_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_add_css_class(create_group, "toolbar-group");
  gtk_widget_set_margin_start(create_group, 0);
  gtk_widget_set_margin_end(create_group, 4);

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
  gtk_widget_set_tooltip_text(add_note_btn, "Create New Rich Note (Ctrl+Shift+N)");

  // Text button with icon
  GtkWidget *add_text_btn = gtk_button_new();
  GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *text_icon = gtk_image_new_from_icon_name("format-text-bold");
  GtkWidget *text_label = gtk_label_new("Text");
  gtk_box_append(GTK_BOX(text_box), text_icon);
  gtk_box_append(GTK_BOX(text_box), text_label);
  gtk_button_set_child(GTK_BUTTON(add_text_btn), text_box);
  gtk_widget_set_tooltip_text(add_text_btn, "Create New Text (Ctrl+N)");

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
  gtk_box_append(GTK_BOX(create_group), add_text_btn);
  gtk_box_append(GTK_BOX(create_group), add_space_btn);
  gtk_box_append(GTK_BOX(toolbar), create_group);

  // Group separator
  GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep1, 2);
  gtk_widget_set_margin_end(sep1, 2);
  gtk_box_append(GTK_BOX(toolbar), sep1);

  // === NAVIGATION GROUP ===
  GtkWidget *nav_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_add_css_class(nav_group, "toolbar-group");
  gtk_widget_set_margin_start(nav_group, 4);
  gtk_widget_set_margin_end(nav_group, 4);

  GtkWidget *back_btn = gtk_button_new();
  GtkWidget *back_icon = gtk_image_new_from_icon_name("go-previous");
  gtk_button_set_child(GTK_BUTTON(back_btn), back_icon);
  gtk_widget_set_tooltip_text(back_btn, "Back to Parent Space (Backspace)");

  GtkWidget *search_btn = gtk_button_new();
  GtkWidget *search_icon = gtk_image_new_from_icon_name("edit-find");
  gtk_button_set_child(GTK_BUTTON(search_btn), search_icon);
  gtk_widget_set_tooltip_text(search_btn, "Search Elements (Ctrl+S)");

  GtkWidget *tree_btn = gtk_toggle_button_new();
  GtkWidget *tree_icon = gtk_image_new_from_icon_name("view-list-tree");
  gtk_button_set_child(GTK_BUTTON(tree_btn), tree_icon);
  gtk_widget_set_tooltip_text(tree_btn, "Toggle Space Tree View (Ctrl+J)");

  gtk_box_append(GTK_BOX(nav_group), back_btn);
  gtk_box_append(GTK_BOX(nav_group), search_btn);
  gtk_box_append(GTK_BOX(nav_group), tree_btn);
  gtk_box_append(GTK_BOX(toolbar), nav_group);

  // Group separator
  GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep2, 2);
  gtk_widget_set_margin_end(sep2, 2);
  gtk_box_append(GTK_BOX(toolbar), sep2);

  // === DRAWING TOOLS GROUP ===
  GtkWidget *draw_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_add_css_class(draw_group, "toolbar-group");
  gtk_widget_set_margin_start(draw_group, 4);
  gtk_widget_set_margin_end(draw_group, 4);

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
  gtk_widget_set_margin_start(sep3, 2);
  gtk_widget_set_margin_end(sep3, 2);
  gtk_box_append(GTK_BOX(toolbar), sep3);

  // === VIEW CONTROLS GROUP ===
  GtkWidget *view_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class(view_group, "toolbar-group");
  gtk_widget_set_margin_start(view_group, 4);
  gtk_widget_set_margin_end(view_group, 4);

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

  // Reset view button with target/crosshair icon
  GtkWidget *reset_view_btn = gtk_button_new();
  GtkWidget *reset_view_icon = gtk_image_new_from_icon_name("find-location");
  gtk_button_set_child(GTK_BUTTON(reset_view_btn), reset_view_icon);
  gtk_widget_set_tooltip_text(reset_view_btn, "Reset View (Zoom 100%, Center to 0,0)");

  gtk_box_append(GTK_BOX(view_group), zoom_box);
  gtk_box_append(GTK_BOX(view_group), space_name_btn);
  gtk_box_append(GTK_BOX(view_group), reset_view_btn);
  gtk_box_append(GTK_BOX(view_group), background_btn);
  gtk_box_append(GTK_BOX(toolbar), view_group);

  // Group separator
  GtkWidget *sep4 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start(sep4, 2);
  gtk_widget_set_margin_end(sep4, 2);
  gtk_box_append(GTK_BOX(toolbar), sep4);

  // === UTILITIES GROUP ===
  GtkWidget *utils_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_widget_add_css_class(utils_group, "toolbar-group");
  gtk_widget_set_margin_start(utils_group, 4);
  gtk_widget_set_margin_end(utils_group, 0);

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

  // Create horizontal paned layout for main content and tree view
  GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_hexpand(main_paned, TRUE);
  gtk_widget_set_vexpand(main_paned, TRUE);
  gtk_box_append(GTK_BOX(vbox), main_paned);

  // Create tree view side panel with scrolled window
  GtkWidget *tree_scrolled = gtk_scrolled_window_new();
  gtk_widget_set_size_request(tree_scrolled, 250, -1);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_scrolled),
                                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  // Create overlay for main canvas
  GtkWidget *overlay = gtk_overlay_new();
  gtk_widget_set_hexpand(overlay, TRUE);
  gtk_widget_set_vexpand(overlay, TRUE);

  // Add to paned layout
  gtk_paned_set_start_child(GTK_PANED(main_paned), tree_scrolled);
  gtk_paned_set_end_child(GTK_PANED(main_paned), overlay);
  gtk_paned_set_resize_start_child(GTK_PANED(main_paned), FALSE);
  gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), FALSE);
  gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);
  gtk_paned_set_shrink_end_child(GTK_PANED(main_paned), FALSE);

  // Initially hide the tree view
  gtk_widget_set_visible(tree_scrolled, FALSE);

  // Add toolbar revealer at the bottom
  gtk_box_append(GTK_BOX(vbox), toolbar_revealer);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_widget_set_hexpand(drawing_area, TRUE);
  gtk_widget_set_vexpand(drawing_area, TRUE);
  gtk_widget_set_can_focus(drawing_area, TRUE);
  gtk_widget_set_focusable(drawing_area, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);

  // Use the database filename from command line args or default
  const char *db_filename = g_database_filename ? g_database_filename : "revel.db";

  CanvasData *data = canvas_data_new_with_db(drawing_area, overlay, db_filename);
  data->zoom_entry = zoom_entry;
  data->toolbar = toolbar;
  data->toolbar_revealer = toolbar_revealer;
  data->toolbar_visible = TRUE;
  data->toolbar_auto_hide = FALSE;
  data->toolbar_hide_timer_id = 0;
  data->ai_toggle_button = ai_btn;
  data->ai_dialog = NULL;

  canvas_input_register_event_handlers(data);

  // Initialize tree view
  data->tree_scrolled = tree_scrolled;
  data->tree_toggle_button = tree_btn;
  data->tree_view_visible = FALSE;
  data->space_tree_view = space_tree_view_new(data);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scrolled),
                               space_tree_view_get_widget(data->space_tree_view));

  canvas_setup_drop_target(data);

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), canvas_on_draw, data, NULL);

  // Add key controller to drawing area
  GtkEventController *key_controller = gtk_event_controller_key_new();
  g_signal_connect(key_controller, "key-pressed", G_CALLBACK(canvas_on_key_pressed), data);
  gtk_widget_add_controller(drawing_area, key_controller);

  // Add key controller to window for global shortcuts (works even when tree view has focus)
  GtkEventController *window_key_controller = gtk_event_controller_key_new();
  g_signal_connect(window_key_controller, "key-pressed", G_CALLBACK(canvas_on_key_pressed), data);
  gtk_widget_add_controller(window, window_key_controller);

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
  g_signal_connect(add_text_btn, "clicked", G_CALLBACK(canvas_on_add_text), data);
  g_signal_connect(ai_btn, "toggled", G_CALLBACK(ai_chat_dialog_toggle), data);
  g_signal_connect(log_btn, "clicked", G_CALLBACK(on_log_clicked), data);
  g_signal_connect(add_space_btn, "clicked", G_CALLBACK(canvas_on_add_space), data);
  g_signal_connect(back_btn, "clicked", G_CALLBACK(canvas_on_go_back), data);
  g_signal_connect(search_btn, "clicked", G_CALLBACK(canvas_show_search_dialog), data);
  g_signal_connect(tree_btn, "toggled", G_CALLBACK(canvas_toggle_tree_view), data);
  g_signal_connect(drawing_btn, "clicked", G_CALLBACK(canvas_toggle_drawing_mode), data);
  g_signal_connect(color_btn, "color-set", G_CALLBACK(on_drawing_color_changed), data);
  g_signal_connect(width_spin, "value-changed", G_CALLBACK(on_drawing_width_changed), data);
  g_signal_connect(shapes_btn, "clicked", G_CALLBACK(canvas_show_shape_selection_dialog), data);
  g_signal_connect(background_btn, "clicked", G_CALLBACK(canvas_show_background_dialog), data);
  g_signal_connect(space_name_btn, "toggled", G_CALLBACK(canvas_toggle_space_name_visibility), data);
  g_signal_connect(reset_view_btn, "clicked", G_CALLBACK(canvas_reset_view), data);
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
    "   background-color: rgba(255, 255, 255, 0.03);"
    "   border-radius: 10px;"
    "   padding: 2px 4px;"
    "   margin: 0;"
    "   border: 1px solid rgba(255, 255, 255, 0.08);"
    "   box-shadow: inset 0 0 1px rgba(255, 255, 255, 0.05);"
    "}"
    ".toolbar-group button {"
    "   border-radius: 7px;"
    "   margin: 0 1px;"
    "   padding: 3px 10px;"
    "   min-height: 26px;"
    "   background: rgba(255, 255, 255, 0.02);"
    "   border: 1px solid transparent;"
    "}"
    ".toolbar-group button:checked,"
    ".toolbar-group button:focus-visible {"
    "   background-color: rgba(255, 255, 255, 0.12);"
    "   border-color: rgba(255, 255, 255, 0.24);"
    "}"
    ".toolbar-group button:hover {"
    "   background-color: rgba(255, 255, 255, 0.12);"
    "}"
    ".toolbar-group button > * {"
    "   margin-left: 2px;"
    "   margin-right: 2px;"
    "}"
    "#toolbar-background {"
    "   background: rgba(18, 18, 18, 0.92);"
    "   padding: 8px 0;"
    "}"
    "scrolledwindow#toolbar-scroll {"
    "   background: transparent;"
    "   padding: 0;"
    "}"
    "scrolledwindow#toolbar-scroll viewport {"
    "   padding: 0;"
    "}"
    "scrolledwindow#toolbar-scroll scrollbar {"
    "   opacity: 0;"
    "   min-width: 0;"
    "   min-height: 0;"
    "}"
    "scrolledwindow#toolbar-scroll undershoot,"
    "scrolledwindow#toolbar-scroll overshoot {"
    "   background: transparent;"
    "   border: none;"
    "   box-shadow: none;"
    "}";

  gtk_css_provider_load_from_data(provider, css, -1);

  gtk_style_context_add_provider_for_display(
                                             gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                                             );

  g_object_unref(provider);

  gtk_window_present(GTK_WINDOW(window));

  // Execute DSL file if specified via command line
  if (g_dsl_filename) {
    GError *error = NULL;
    gchar *script_contents = NULL;
    gsize length = 0;

    if (g_file_get_contents(g_dsl_filename, &script_contents, &length, &error)) {
      g_print("Executing DSL file: %s\n", g_dsl_filename);
      canvas_execute_script_file(data, script_contents, g_dsl_filename);
      g_free(script_contents);
    } else {
      g_print("Failed to load DSL file %s: %s\n", g_dsl_filename, error ? error->message : "unknown error");
      if (error) g_error_free(error);
    }
  }
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("com.example.notecanvas", G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  g_signal_connect(app, "command-line", G_CALLBACK(on_command_line), NULL);
  g_signal_connect(app, "shutdown", G_CALLBACK(canvas_on_app_shutdown), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);

  // Cleanup
  if (g_database_filename) {
    g_free(g_database_filename);
  }
  if (g_dsl_filename) {
    g_free(g_dsl_filename);
  }

  g_object_unref(app);
  return status;
}
