#include "shape_dialog.h"
#include "canvas_core.h"
#include "shape.h"

typedef struct {
  CanvasData *canvas_data;
  GtkWidget *dialog;
  ShapeType selected_shape;
  gboolean filled;
} ShapeDialogData;

static void on_shape_button_clicked(GtkButton *button, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;

  ShapeType shape_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "shape_type"));
  data->selected_shape = shape_type;

  // Close dialog and proceed with shape creation
  gtk_window_destroy(GTK_WINDOW(data->dialog));

  // Set the canvas to shape drawing mode (shape mode only, not drawing mode)
  data->canvas_data->drawing_mode = FALSE;  // Ensure drawing mode is OFF
  data->canvas_data->shape_mode = TRUE;
  data->canvas_data->selected_shape_type = shape_type;
  data->canvas_data->shape_filled = data->filled;

  // Clear any existing drawing state to prevent interference
  data->canvas_data->current_drawing = NULL;

  // Update cursor
  canvas_set_cursor(data->canvas_data, data->canvas_data->draw_cursor);

  g_free(data);
}

static void on_filled_toggle(GtkToggleButton *toggle, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;
  data->filled = gtk_toggle_button_get_active(toggle);
}

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(data);
  }
}

void canvas_show_shape_selection_dialog(GtkButton *button, gpointer user_data) {
  CanvasData *canvas_data = (CanvasData*)user_data;

  GtkRoot *root = gtk_widget_get_root(canvas_data->drawing_area);
  GtkWidget *window = GTK_WIDGET(root);
  if (!GTK_IS_WINDOW(window)) {
    return;
  }

  ShapeDialogData *data = g_new0(ShapeDialogData, 1);
  data->canvas_data = canvas_data;
  data->filled = FALSE;

  GtkWidget *dialog = gtk_dialog_new();
  data->dialog = dialog;

  gtk_window_set_title(GTK_WINDOW(dialog), "Select Shape");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_set_margin_top(content_area, 10);
  gtk_widget_set_margin_bottom(content_area, 10);
  gtk_widget_set_margin_start(content_area, 10);
  gtk_widget_set_margin_end(content_area, 10);

  // Create main container
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append(GTK_BOX(content_area), vbox);

  // Create filled/stroke toggle
  GtkWidget *filled_toggle = gtk_toggle_button_new_with_label("Filled");
  gtk_box_append(GTK_BOX(vbox), filled_toggle);
  g_signal_connect(filled_toggle, "toggled", G_CALLBACK(on_filled_toggle), data);

  // Create shape buttons container
  GtkWidget *shapes_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign(shapes_box, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), shapes_box);

  // Circle button
  GtkWidget *circle_btn = gtk_button_new_with_label("Circle");
  g_object_set_data(G_OBJECT(circle_btn), "shape_type", GINT_TO_POINTER(SHAPE_CIRCLE));
  g_signal_connect(circle_btn, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box), circle_btn);

  // Rectangle button
  GtkWidget *rectangle_btn = gtk_button_new_with_label("Rectangle");
  g_object_set_data(G_OBJECT(rectangle_btn), "shape_type", GINT_TO_POINTER(SHAPE_RECTANGLE));
  g_signal_connect(rectangle_btn, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box), rectangle_btn);

  // Triangle button
  GtkWidget *triangle_btn = gtk_button_new_with_label("Triangle");
  g_object_set_data(G_OBJECT(triangle_btn), "shape_type", GINT_TO_POINTER(SHAPE_TRIANGLE));
  g_signal_connect(triangle_btn, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box), triangle_btn);

  // Create second row of shape buttons
  GtkWidget *shapes_box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign(shapes_box2, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), shapes_box2);

  // Vertical Cylinder button
  GtkWidget *vcylinder_btn = gtk_button_new_with_label("V-Cylinder");
  g_object_set_data(G_OBJECT(vcylinder_btn), "shape_type", GINT_TO_POINTER(SHAPE_CYLINDER_VERTICAL));
  g_signal_connect(vcylinder_btn, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box2), vcylinder_btn);

  // Horizontal Cylinder button
  GtkWidget *hcylinder_btn = gtk_button_new_with_label("H-Cylinder");
  g_object_set_data(G_OBJECT(hcylinder_btn), "shape_type", GINT_TO_POINTER(SHAPE_CYLINDER_HORIZONTAL));
  g_signal_connect(hcylinder_btn, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box2), hcylinder_btn);

  GtkWidget *diamond_button = gtk_button_new_with_label("Diamond");
  g_object_set_data(G_OBJECT(diamond_button), "shape_type", GINT_TO_POINTER(SHAPE_DIAMOND));
  g_signal_connect(diamond_button, "clicked", G_CALLBACK(on_shape_button_clicked), data);
  gtk_box_append(GTK_BOX(shapes_box2), diamond_button);


  // Add Cancel button
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);

  g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), data);

  gtk_window_present(GTK_WINDOW(dialog));
}
