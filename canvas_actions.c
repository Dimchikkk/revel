#include "canvas_actions.h"
#include "canvas_core.h"
#include "canvas_spaces.h"
#include "canvas_input.h"
#include "element.h"
#include "paper_note.h"
#include "note.h"
#include "space.h"
#include "undo_manager.h"

void canvas_on_add_paper_note(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  ElementPosition position = {
    .x = 100,
    .y = 100,
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
  ElementSize size = {
    .width = 200,
    .height = 150,
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
    .font_description = g_strdup("Ubuntu 18"),
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
  gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  ElementPosition position = {
    .x = 100,
    .y = 100,
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
  ElementSize size = {
    .width = 200,
    .height = 150,
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
    .font_description = g_strdup("Ubuntu Mono 18"),
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

  g_signal_connect(dialog, "response", G_CALLBACK(space_creation_dialog_response), data);

  gtk_window_present(GTK_WINDOW(dialog));
}

void canvas_on_go_back(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  go_back_to_parent_space(data);
}

void canvas_toggle_drawing_mode(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  data->drawing_mode = !data->drawing_mode;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), data->drawing_mode);

  if (data->drawing_mode) {
    canvas_set_cursor(data, data->draw_cursor);
  } else {
    canvas_set_cursor(data, data->default_cursor);
    // Cancel any current drawing
    if (data->current_drawing) {
      data->current_drawing = NULL;
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
