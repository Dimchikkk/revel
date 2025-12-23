#include "canvas_actions.h"
#include "canvas_core.h"
#include "canvas_spaces.h"
#include "canvas_space_tree.h"
#include "canvas_input.h"
#include "canvas_placement.h"
#include "../elements/element.h"
#include "../elements/paper_note.h"
#include "../elements/note.h"
#include "../elements/space.h"
#include "../elements/inline_text.h"
#include "../elements/shape.h"
#include "../elements/connection.h"
#include "../elements/media_note.h"
#include "../undo_manager.h"
#include "../model.h"

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
  // Use hardcoded default colors (not toolbar colors)
  ElementColor bg_color = {1.0, 1.0, 0.8, 1.0}; // Light yellow
  ElementColor text_color = {0.2, 0.2, 0.2, 1.0}; // Dark gray
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
    .font_description = g_strdup(PAPER_NOTE_DEFAULT_FONT),
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
  // Use hardcoded default colors (not toolbar colors)
  ElementColor bg_color = {1.0, 1.0, 1.0, 1.0}; // White
  ElementColor text_color = {0.2, 0.2, 0.2, 1.0}; // Dark gray
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
  // Inline text uses transparent background and white text by default (not toolbar colors)
  ElementColor bg_color = {0.0, 0.0, 0.0, 0.0};
  ElementColor text_color = {0.9, 0.9, 0.9, 1.0};
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

  // Update default for new drawings
  data->drawing_color.r = color.red;
  data->drawing_color.g = color.green;
  data->drawing_color.b = color.blue;
  data->drawing_color.a = color.alpha;

  // Apply to selected freehand drawings only
  if (data->selected_elements) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
      Element *el = (Element*)l->data;
      if (el->type == ELEMENT_FREEHAND_DRAWING) {
        el->bg_r = color.red;
        el->bg_g = color.green;
        el->bg_b = color.blue;
        el->bg_a = color.alpha;
        ModelElement *model_el = model_get_by_visual(data->model, el);
        if (model_el) {
          model_update_color(data->model, model_el, color.red, color.green, color.blue, color.alpha);
        }
      }
    }
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void on_drawing_width_changed(GtkSpinButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  data->drawing_stroke_width = gtk_spin_button_get_value_as_int(button);
}

// Forward declarations for helper functions
static void apply_stroke_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color);
static void apply_text_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color);
static void apply_background_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color);

void on_stroke_color_changed(GtkColorButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

  // Update default for new elements
  data->stroke_color.r = color.red;
  data->stroke_color.g = color.green;
  data->stroke_color.b = color.blue;
  data->stroke_color.a = color.alpha;

  // Apply to selected elements (live update)
  if (data->selected_elements) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
      Element *el = (Element*)l->data;
      apply_stroke_color_to_element(data, el, &color);
    }
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void on_text_color_changed(GtkColorButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

  // Update default for new elements
  data->text_color.r = color.red;
  data->text_color.g = color.green;
  data->text_color.b = color.blue;
  data->text_color.a = color.alpha;

  // Apply to selected elements (live update)
  if (data->selected_elements) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
      Element *el = (Element*)l->data;
      apply_text_color_to_element(data, el, &color);
    }
    gtk_widget_queue_draw(data->drawing_area);
  }
}

void on_background_color_changed(GtkColorButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

  // Update default for new elements
  data->background_color.r = color.red;
  data->background_color.g = color.green;
  data->background_color.b = color.blue;
  data->background_color.a = color.alpha;

  // Apply to selected elements (live update)
  if (data->selected_elements) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
      Element *el = (Element*)l->data;
      apply_background_color_to_element(data, el, &color);
    }
    gtk_widget_queue_draw(data->drawing_area);
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
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
  gtk_widget_set_margin_start(grid, 18);
  gtk_widget_set_margin_end(grid, 18);
  gtk_widget_set_margin_top(grid, 18);
  gtk_widget_set_margin_bottom(grid, 18);
  gtk_box_append(GTK_BOX(content_area), grid);

  GtkWidget *color_label = gtk_label_new("Background Color:");
  gtk_label_set_xalign(GTK_LABEL(color_label), 0.0);
  GtkWidget *color_button = gtk_color_button_new();
  gtk_widget_set_hexpand(color_button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), color_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), color_button, 1, 0, 1, 1);

  GtkWidget *grid_checkbox = gtk_check_button_new_with_label("Show Grid");
  gtk_grid_attach(GTK_GRID(grid), grid_checkbox, 0, 1, 2, 1);

  GtkWidget *grid_color_label = gtk_label_new("Grid Color:");
  gtk_label_set_xalign(GTK_LABEL(grid_color_label), 0.0);
  GtkWidget *grid_color_button = gtk_color_button_new();
  gtk_widget_set_hexpand(grid_color_button, TRUE);
  gtk_grid_attach(GTK_GRID(grid), grid_color_label, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), grid_color_button, 1, 2, 1, 1);

  // Set default grid color
  GdkRGBA default_grid_color = {0.15, 0.15, 0.20, 0.4};
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
    .r = 1.0,
    .g = 1.0,
    .b = 1.0,
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

    // Build tree on first open (lazy initialization)
    if (data->space_tree_view && !data->space_tree_view->is_built) {
      space_tree_view_schedule_refresh(data->space_tree_view);
    } else if (data->space_tree_view) {
      // Refresh tree view to show current state
      space_tree_view_schedule_refresh(data->space_tree_view);
    }
  } else {
    // Hide tree view
    gtk_widget_set_visible(data->tree_scrolled, FALSE);
    data->tree_view_visible = FALSE;
  }
}

// Helper function to apply stroke color to selected element (shapes and connections only)
static void apply_stroke_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color) {
  ModelElement *model_el = model_get_by_visual(data->model, element);
  if (!model_el) return;

  switch (element->type) {
    case ELEMENT_SHAPE: {
      Shape *shape = (Shape*)element;
      shape->stroke_r = color->red;
      shape->stroke_g = color->green;
      shape->stroke_b = color->blue;
      shape->stroke_a = color->alpha;
      // Update model's stroke_color hex string
      g_free(model_el->stroke_color);
      model_el->stroke_color = g_strdup_printf("#%02x%02x%02x%02x",
        (int)(color->red * 255),
        (int)(color->green * 255),
        (int)(color->blue * 255),
        (int)(color->alpha * 255));
      break;
    }
    case ELEMENT_CONNECTION: {
      // Connections use base element bg color for line color
      element->bg_r = color->red;
      element->bg_g = color->green;
      element->bg_b = color->blue;
      element->bg_a = color->alpha;
      model_update_color(data->model, model_el,
                        color->red, color->green,
                        color->blue, color->alpha);
      break;
    }
    default:
      break;
  }
}

// Helper function to apply text color to selected element
static void apply_text_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color) {
  ModelElement *model_el = model_get_by_visual(data->model, element);
  if (!model_el) return;

  switch (element->type) {
    case ELEMENT_NOTE: {
      Note *note = (Note*)element;
      note->text_r = color->red;
      note->text_g = color->green;
      note->text_b = color->blue;
      note->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    case ELEMENT_PAPER_NOTE: {
      PaperNote *pn = (PaperNote*)element;
      pn->text_r = color->red;
      pn->text_g = color->green;
      pn->text_b = color->blue;
      pn->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    case ELEMENT_INLINE_TEXT: {
      InlineText *it = (InlineText*)element;
      it->text_r = color->red;
      it->text_g = color->green;
      it->text_b = color->blue;
      it->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    case ELEMENT_SPACE: {
      SpaceElement *space = (SpaceElement*)element;
      space->text_r = color->red;
      space->text_g = color->green;
      space->text_b = color->blue;
      space->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    case ELEMENT_MEDIA_FILE: {
      MediaNote *media = (MediaNote*)element;
      media->text_r = color->red;
      media->text_g = color->green;
      media->text_b = color->blue;
      media->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    case ELEMENT_SHAPE: {
      // Shape label text color
      Shape *shape = (Shape*)element;
      shape->text_r = color->red;
      shape->text_g = color->green;
      shape->text_b = color->blue;
      shape->text_a = color->alpha;
      model_update_text_color(data->model, model_el,
                              color->red, color->green, color->blue, color->alpha);
      break;
    }
    default:
      break;
  }
}

// Helper function to apply background color to selected element
static void apply_background_color_to_element(CanvasData *data, Element *element, const GdkRGBA *color) {
  ModelElement *model_el = model_get_by_visual(data->model, element);
  if (!model_el) return;

  switch (element->type) {
    case ELEMENT_NOTE:
    case ELEMENT_PAPER_NOTE:
    case ELEMENT_INLINE_TEXT:
    case ELEMENT_SPACE:
    case ELEMENT_MEDIA_FILE:
    case ELEMENT_SHAPE:
    case ELEMENT_CONNECTION: {
      // All element types use base element bg color
      element->bg_r = color->red;
      element->bg_g = color->green;
      element->bg_b = color->blue;
      element->bg_a = color->alpha;
      // Update model
      model_update_color(data->model, model_el,
                        color->red, color->green,
                        color->blue, color->alpha);
      break;
    }
    default:
      break;
  }
}

// Update toolbar color buttons based on selected element
void canvas_update_toolbar_colors_from_selection(CanvasData *data) {
  if (!data->selected_elements || g_list_length(data->selected_elements) != 1) {
    return; // Only sync for single selection
  }

  Element *el = (Element*)data->selected_elements->data;

  // Update drawing color only for freehand drawings
  if (el->type == ELEMENT_FREEHAND_DRAWING) {
    GdkRGBA color = {el->bg_r, el->bg_g, el->bg_b, el->bg_a};
    g_signal_handlers_block_by_func(data->drawing_color_button,
                                    on_drawing_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->drawing_color_button), &color);
    g_signal_handlers_unblock_by_func(data->drawing_color_button,
                                      on_drawing_color_changed, data);
  }

  // Update stroke color for shapes and connections
  if (el->type == ELEMENT_SHAPE) {
    Shape *shape = (Shape*)el;
    GdkRGBA stroke_color = {shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a};
    g_signal_handlers_block_by_func(data->stroke_color_button,
                                    on_stroke_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->stroke_color_button), &stroke_color);
    g_signal_handlers_unblock_by_func(data->stroke_color_button,
                                      on_stroke_color_changed, data);
  } else if (el->type == ELEMENT_CONNECTION) {
    // Connections use base element bg color for line color
    GdkRGBA line_color = {el->bg_r, el->bg_g, el->bg_b, el->bg_a};
    g_signal_handlers_block_by_func(data->stroke_color_button,
                                    on_stroke_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->stroke_color_button), &line_color);
    g_signal_handlers_unblock_by_func(data->stroke_color_button,
                                      on_stroke_color_changed, data);
  }

  // Update text color for text elements
  if (el->type == ELEMENT_NOTE) {
    Note *note = (Note*)el;
    GdkRGBA text_color = {note->text_r, note->text_g, note->text_b, note->text_a};
    g_signal_handlers_block_by_func(data->text_color_button,
                                    on_text_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->text_color_button), &text_color);
    g_signal_handlers_unblock_by_func(data->text_color_button,
                                      on_text_color_changed, data);
  } else if (el->type == ELEMENT_PAPER_NOTE) {
    PaperNote *pn = (PaperNote*)el;
    GdkRGBA text_color = {pn->text_r, pn->text_g, pn->text_b, pn->text_a};
    g_signal_handlers_block_by_func(data->text_color_button,
                                    on_text_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->text_color_button), &text_color);
    g_signal_handlers_unblock_by_func(data->text_color_button,
                                      on_text_color_changed, data);
  } else if (el->type == ELEMENT_INLINE_TEXT) {
    InlineText *it = (InlineText*)el;
    GdkRGBA text_color = {it->text_r, it->text_g, it->text_b, it->text_a};
    g_signal_handlers_block_by_func(data->text_color_button,
                                    on_text_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->text_color_button), &text_color);
    g_signal_handlers_unblock_by_func(data->text_color_button,
                                      on_text_color_changed, data);
  } else if (el->type == ELEMENT_SHAPE) {
    // Shape label text color
    Shape *shape = (Shape*)el;
    GdkRGBA text_color = {shape->text_r, shape->text_g, shape->text_b, shape->text_a};
    g_signal_handlers_block_by_func(data->text_color_button,
                                    on_text_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->text_color_button), &text_color);
    g_signal_handlers_unblock_by_func(data->text_color_button,
                                      on_text_color_changed, data);
  }

  // Update background color for all element types
  if (el->type != ELEMENT_FREEHAND_DRAWING && el->type != ELEMENT_CONNECTION) {
    GdkRGBA bg_color = {el->bg_r, el->bg_g, el->bg_b, el->bg_a};
    g_signal_handlers_block_by_func(data->bg_color_button,
                                    on_background_color_changed, data);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->bg_color_button), &bg_color);
    g_signal_handlers_unblock_by_func(data->bg_color_button,
                                      on_background_color_changed, data);
  }
}
