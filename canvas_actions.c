#include "canvas_actions.h"
#include "canvas_core.h"
#include "canvas_spaces.h"
#include "canvas_space_tree.h"
#include "canvas_input.h"
#include "canvas_placement.h"
#include "element.h"
#include "paper_note.h"
#include "note.h"
#include "space.h"
#include "undo_manager.h"
#include "model.h"

static void on_space_entry_activate(GtkEntry *entry, gpointer user_data) {
  GtkDialog *dialog = GTK_DIALOG(user_data);
  gtk_dialog_response(dialog, GTK_RESPONSE_OK);
}

void canvas_on_add_paper_note(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  ElementSize size = {
    .width = 200,
    .height = 150,
  };

  // Find smart placement position
  int smart_x, smart_y;
  canvas_find_empty_position(data, size.width, size.height, &smart_x, &smart_y);

  ElementPosition position = {
    .x = smart_x,
    .y = smart_y,
    .z = data->next_z_index++,
  };
  ElementColor bg_color = {
    .r = 1.0,
    .g = 1.0,
    .b = 0.8,
    .a = 1.0,
  };
  ElementColor text_color = {
    .r = 0.2,
    .g = 0.2,
    .b = 0.2,
    .a = 1.0,
  };
  ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
  ElementConnection connection = {
    .from_element_uuid = NULL,
    .to_element_uuid = NULL,
    .from_point = -1,
    .to_point = -1,
  };
  ElementDrawing drawing = {
    .drawing_points = NULL,
    .stroke_width = 0,
  };
  ElementText text = {
    .text = "",
    .text_color = text_color,
    .font_description = g_strdup("Ubuntu Mono 16"),
  };
  ElementConfig config = {
    .type = ELEMENT_PAPER_NOTE,
    .bg_color = bg_color,
    .position = position,
    .size = size,
    .media = media,
    .drawing = drawing,
    .connection = connection,
    .text = text,
  };


  ModelElement *model_element = model_create_element(data->model, config);
  if (!model_element) {
    g_printerr("Failed to create paper note model element\n");
    return;
  }
  // Link model and visual elements
  model_element->visual_element = create_visual_element(model_element, data);
  undo_manager_push_create_action(data->undo_manager, model_element);

  // Start text editing immediately for new paper note
  element_start_editing(model_element->visual_element, data->overlay);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  ElementSize size = {
    .width = 200,
    .height = 150,
  };

  // Find smart placement position
  int smart_x, smart_y;
  canvas_find_empty_position(data, size.width, size.height, &smart_x, &smart_y);

  ElementPosition position = {
    .x = smart_x,
    .y = smart_y,
    .z = data->next_z_index++,
  };
  ElementColor bg_color = {
    .r = 1.0,
    .g = 1.0,
    .b = 1.0,
    .a = 1.0,
  };
  ElementColor text_color = {
    .r = 0.2,
    .g = 0.2,
    .b = 0.2,
    .a = 1.0,
  };
  ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
  ElementConnection connection = {
    .from_element_uuid = NULL,
    .to_element_uuid = NULL,
    .from_point = -1,
    .to_point = -1,
  };
  ElementDrawing drawing = {
    .drawing_points = NULL,
    .stroke_width = 0,
  };
  ElementText text = {
    .text = "",
    .text_color = text_color,
    .font_description = g_strdup("Ubuntu 16"),
  };
  ElementConfig config = {
    .type = ELEMENT_NOTE,
    .bg_color = bg_color,
    .position = position,
    .size = size,
    .media = media,
    .drawing = drawing,
    .connection = connection,
    .text = text,
  };


  ModelElement *model_element = model_create_element(data->model, config);

  if (!model_element) {
    g_printerr("Failed to create note model element\n");
    return;
  }

  // Link model and visual elements
  model_element->visual_element = create_visual_element(model_element, data);
  undo_manager_push_create_action(data->undo_manager, model_element);

  // Start text editing immediately for new note
  element_start_editing(model_element->visual_element, data->overlay);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_text(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  ElementSize size = {
    .width = 100,
    .height = 20,
  };

  // Find smart placement position
  int smart_x, smart_y;
  canvas_find_empty_position(data, size.width, size.height, &smart_x, &smart_y);

  ElementPosition position = {
    .x = smart_x,
    .y = smart_y,
    .z = data->next_z_index++,
  };
  ElementColor bg_color = {
    .r = 0.0,
    .g = 0.0,
    .b = 0.0,
    .a = 0.0,
  };
  ElementColor text_color = {
    .r = 0.6,
    .g = 0.6,
    .b = 0.6,
    .a = 1.0,
  };
  ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
  ElementConnection connection = {
    .from_element_uuid = NULL,
    .to_element_uuid = NULL,
    .from_point = -1,
    .to_point = -1,
  };
  ElementDrawing drawing = {
    .drawing_points = NULL,
    .stroke_width = 0,
  };
  ElementText text = {
    .text = "",
    .text_color = text_color,
    .font_description = g_strdup("Ubuntu Mono 14"),
  };
  ElementConfig config = {
    .type = ELEMENT_INLINE_TEXT,
    .bg_color = bg_color,
    .position = position,
    .size = size,
    .media = media,
    .drawing = drawing,
    .connection = connection,
    .text = text,
  };

  ModelElement *model_element = model_create_element(data->model, config);
  if (!model_element) {
    g_printerr("Failed to create inline text model element\n");
    return;
  }
  // Link model and visual elements
  model_element->visual_element = create_visual_element(model_element, data);
  undo_manager_push_create_action(data->undo_manager, model_element);

  // Start text editing immediately for new inline text
  element_start_editing(model_element->visual_element, data->overlay);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_space(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  GtkRoot *root = gtk_widget_get_root(data->drawing_area);
  GtkWindow *window = GTK_WINDOW(root);

  GtkWidget *dialog = gtk_dialog_new_with_buttons(
                                                  "Create New Space",
                                                  window,
                                                  GTK_DIALOG_MODAL,
                                                  "Create", GTK_RESPONSE_OK,
                                                  NULL,
                                                  NULL
                                                  );

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Create a grid for better layout
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

  // Label
  GtkWidget *label = gtk_label_new("Enter space name:");
  gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

  // Entry field
  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Space name");
  gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 1, 1);

  gtk_box_append(GTK_BOX(content_area), grid);

  // Store the entry widget in dialog data for easy access
  g_object_set_data(G_OBJECT(dialog), "space_name_entry", entry);
  g_object_set_data(G_OBJECT(dialog), "canvas_data", data);

  // Set focus on the entry field
  gtk_widget_grab_focus(entry);

  // Connect Enter key to trigger Create button
  g_signal_connect(entry, "activate", G_CALLBACK(on_space_entry_activate), dialog);

  g_signal_connect(dialog, "response", G_CALLBACK(space_creation_dialog_response), data);

  gtk_window_present(GTK_WINDOW(dialog));
}

void canvas_on_go_back(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  go_back_to_parent_space(data);
}

void canvas_toggle_drawing_mode(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  // Don't toggle drawing mode if we're currently in shape mode
  if (data->shape_mode) {
    return;
  }

  data->drawing_mode = !data->drawing_mode;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), data->drawing_mode);

  if (data->drawing_mode) {
    canvas_set_cursor(data, data->draw_cursor);
  } else {
    canvas_set_cursor(data, data->default_cursor);
    if (data->current_drawing) {
      data->current_drawing = NULL;
    }
    if (data->current_shape) {
      data->current_shape = NULL;
    }
  }

  gtk_widget_queue_draw(data->drawing_area);
}

void on_drawing_color_changed(GtkColorButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

  data->drawing_color.r = color.red;
  data->drawing_color.g = color.green;
  data->drawing_color.b = color.blue;
  data->drawing_color.a = color.alpha;
}

void on_drawing_width_changed(GtkSpinButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  data->drawing_stroke_width = gtk_spin_button_get_value_as_int(button);
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
void canvas_show_background_dialog(GtkButton *button, gpointer user_data) {
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

void canvas_on_add_inline_text(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  ElementSize size = {
    .width = 100,
    .height = 20,
  };

  // Find smart placement position
  int smart_x, smart_y;
  canvas_find_empty_position(data, size.width, size.height, &smart_x, &smart_y);

  ElementPosition position = {
    .x = smart_x,
    .y = smart_y,
    .z = data->next_z_index++,
  };
  ElementColor bg_color = {
    .r = 0.0,
    .g = 0.0,
    .b = 0.0,
    .a = 0.0,
  };
  ElementColor text_color = {
    .r = 0.6,
    .g = 0.6,
    .b = 0.6,
    .a = 1.0,
  };
  ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
  ElementConnection connection = {
    .from_element_uuid = NULL,
    .to_element_uuid = NULL,
    .from_point = -1,
    .to_point = -1,
  };
  ElementDrawing drawing = {
    .drawing_points = NULL,
    .stroke_width = 0,
  };
  ElementText text = {
    .text = "",
    .text_color = text_color,
    .font_description = g_strdup("Ubuntu Mono 14"),
  };
  ElementConfig config = {
    .type = ELEMENT_INLINE_TEXT,
    .bg_color = bg_color,
    .position = position,
    .size = size,
    .media = media,
    .drawing = drawing,
    .connection = connection,
    .text = text,
  };

  ModelElement *model_element = model_create_element(data->model, config);
  if (!model_element) {
    g_printerr("Failed to create inline text model element\n");
    return;
  }
  // Link model and visual elements
  model_element->visual_element = create_visual_element(model_element, data);
  undo_manager_push_create_action(data->undo_manager, model_element);

  // Start text editing immediately for new inline text
  element_start_editing(model_element->visual_element, data->overlay);

  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_toggle_tree_view(GtkToggleButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (!data || !data->tree_scrolled) return;

  gboolean is_active = gtk_toggle_button_get_active(button);

  if (is_active) {
    // Show tree view
    gtk_widget_set_visible(data->tree_scrolled, TRUE);
    data->tree_view_visible = TRUE;

    // Refresh tree view to show current state
    if (data->space_tree_view) {
      space_tree_view_schedule_refresh(data->space_tree_view);
    }
  } else {
    // Hide tree view
    gtk_widget_set_visible(data->tree_scrolled, FALSE);
    data->tree_view_visible = FALSE;
  }
}
