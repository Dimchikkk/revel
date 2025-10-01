#include "clone_dialog.h"
#include "canvas_core.h"
#include "model.h"
#include "undo_manager.h"
#include <gtk/gtk.h>

typedef struct {
  CanvasData *canvas_data;
  ModelElement *element;
  GtkCheckButton *text_check;
  GtkCheckButton *size_check;
  GtkCheckButton *position_check;
  GtkCheckButton *color_check;
} CloneDialogData;

static void on_clone_dialog_response(GtkDialog *dialog, int response, gpointer user_data) {
  CloneDialogData *dialog_data = (CloneDialogData*)user_data;

  if (response == GTK_RESPONSE_OK) {
    // Determine which flags to use based on checkbox states
    CloneFlags flags = CLONE_FLAG_NONE;

    if (gtk_check_button_get_active(dialog_data->text_check)) {
      flags |= CLONE_FLAG_TEXT;
    }
    if (gtk_check_button_get_active(dialog_data->size_check)) {
      flags |= CLONE_FLAG_SIZE;
    }
    if (gtk_check_button_get_active(dialog_data->position_check)) {
      flags |= CLONE_FLAG_POSITION;
    }
    if (gtk_check_button_get_active(dialog_data->color_check)) {
      flags |= CLONE_FLAG_COLOR;
    }

    // Create the clone with the specified flags
    ModelElement *clone = model_element_clone(dialog_data->canvas_data->model,
                                               dialog_data->element,
                                               flags);

    if (clone) {
      // Create visual element
      clone->visual_element = create_visual_element(clone, dialog_data->canvas_data);

      // Push undo action
      undo_manager_push_create_action(dialog_data->canvas_data->undo_manager, clone);

      // Update display
      canvas_sync_with_model(dialog_data->canvas_data);
      gtk_widget_queue_draw(dialog_data->canvas_data->drawing_area);
    }
  }

  g_free(dialog_data);
  gtk_window_destroy(GTK_WINDOW(dialog));
}

void clone_dialog_open(CanvasData *canvas_data, ModelElement *element) {
  if (!canvas_data || !element) {
    return;
  }

  // Get the root window
  GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(canvas_data->drawing_area));

  // Create dialog
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
    "Clone Element",
    GTK_WINDOW(root),
    GTK_DIALOG_MODAL,
    "Cancel",
    GTK_RESPONSE_CANCEL,
    "Clone",
    GTK_RESPONSE_OK,
    NULL
  );

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(root));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 320, 240);

  // Create dialog data
  CloneDialogData *dialog_data = g_new0(CloneDialogData, 1);
  dialog_data->canvas_data = canvas_data;
  dialog_data->element = element;

  // Get content area
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_set_margin_start(content, 20);
  gtk_widget_set_margin_end(content, 20);
  gtk_widget_set_margin_top(content, 20);
  gtk_widget_set_margin_bottom(content, 20);

  // Add header label
  GtkWidget *header = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(header),
    "<span size='large' weight='bold'>Select Properties to Clone</span>");
  gtk_label_set_xalign(GTK_LABEL(header), 0.0);
  gtk_widget_set_margin_bottom(header, 16);
  gtk_box_append(GTK_BOX(content), header);

  // Add description
  GtkWidget *desc = gtk_label_new(
    "Choose which properties to share with the cloned element.\n"
    "If no properties are selected, an independent copy will be created.");
  gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
  gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
  gtk_widget_set_margin_bottom(desc, 16);
  gtk_box_append(GTK_BOX(content), desc);

  // Create checkboxes for each clonable property

  // Text checkbox
  dialog_data->text_check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Clone by Text"));
  gtk_widget_set_sensitive(GTK_WIDGET(dialog_data->text_check), element->text != NULL);
  gtk_widget_set_margin_bottom(GTK_WIDGET(dialog_data->text_check), 8);
  gtk_box_append(GTK_BOX(content), GTK_WIDGET(dialog_data->text_check));

  // Size checkbox
  dialog_data->size_check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Clone by Size"));
  gtk_widget_set_sensitive(GTK_WIDGET(dialog_data->size_check), element->size != NULL);
  gtk_widget_set_margin_bottom(GTK_WIDGET(dialog_data->size_check), 8);
  gtk_box_append(GTK_BOX(content), GTK_WIDGET(dialog_data->size_check));

  // Position checkbox
  dialog_data->position_check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Clone by Position"));
  gtk_widget_set_sensitive(GTK_WIDGET(dialog_data->position_check), element->position != NULL);
  gtk_widget_set_margin_bottom(GTK_WIDGET(dialog_data->position_check), 8);
  gtk_box_append(GTK_BOX(content), GTK_WIDGET(dialog_data->position_check));

  // Color checkbox
  dialog_data->color_check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Clone by Background Color"));
  gtk_widget_set_sensitive(GTK_WIDGET(dialog_data->color_check), element->bg_color != NULL);
  gtk_widget_set_margin_bottom(GTK_WIDGET(dialog_data->color_check), 8);
  gtk_box_append(GTK_BOX(content), GTK_WIDGET(dialog_data->color_check));

  // Connect response signal
  g_signal_connect(dialog, "response", G_CALLBACK(on_clone_dialog_response), dialog_data);

  // Show dialog
  gtk_widget_set_visible(dialog, TRUE);
}
