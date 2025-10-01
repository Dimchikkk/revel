#include "shape_dialog.h"
#include "canvas_core.h"
#include "shape.h"
#include <math.h>

typedef struct {
  CanvasData *canvas_data;
  GtkWidget *dialog;
  ShapeType selected_shape;
  gboolean filled;
  StrokeStyle stroke_style;
  FillStyle fill_style;
  GtkWidget *stroke_combo;
  GtkWidget *fill_combo;
  GPtrArray *icon_widgets;
} ShapeDialogData;

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);

typedef struct {
  ShapeDialogData *dialog_data;
  ShapeType shape_type;
} ShapeIconData;

static void shape_dialog_data_free(ShapeDialogData *data) {
  if (!data) return;
  if (data->icon_widgets) {
    g_ptr_array_free(data->icon_widgets, TRUE);
  }
  g_free(data);
}

static void draw_shape_icon(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
  ShapeIconData *icon_data = (ShapeIconData*)user_data;
  if (!icon_data || !icon_data->dialog_data) return;

  gboolean filled = icon_data->dialog_data->filled;

  double inset = 6.0;
  double stroke_width = 2.0;
  double draw_width = width - inset * 2.0;
  double draw_height = height - inset * 2.0;

  cairo_set_line_width(cr, stroke_width);

  // Fill background transparent
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);

  switch (icon_data->shape_type) {
  case SHAPE_CIRCLE: {
    double radius = MIN(draw_width, draw_height) / 2.0;
    cairo_arc(cr, width / 2.0, height / 2.0, radius, 0, 2 * G_PI);
    break;
  }
  case SHAPE_RECTANGLE:
    cairo_rectangle(cr, inset, inset, draw_width, draw_height);
    break;
  case SHAPE_ROUNDED_RECTANGLE: {
    double radius = MIN(draw_width, draw_height) * 0.25;
    double x = inset;
    double y = inset;
    double right = x + draw_width;
    double bottom = y + draw_height;

    cairo_new_sub_path(cr);
    cairo_arc(cr, right - radius, y + radius, radius, -G_PI_2, 0);
    cairo_arc(cr, right - radius, bottom - radius, radius, 0, G_PI_2);
    cairo_arc(cr, x + radius, bottom - radius, radius, G_PI_2, G_PI);
    cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
    break;
  }
  case SHAPE_TRIANGLE:
    cairo_move_to(cr, width / 2.0, inset);
    cairo_line_to(cr, width - inset, height - inset);
    cairo_line_to(cr, inset, height - inset);
    cairo_close_path(cr);
    break;
  case SHAPE_DIAMOND:
    cairo_move_to(cr, width / 2.0, inset);
    cairo_line_to(cr, width - inset, height / 2.0);
    cairo_line_to(cr, width / 2.0, height - inset);
    cairo_line_to(cr, inset, height / 2.0);
    cairo_close_path(cr);
    break;
  case SHAPE_CYLINDER_VERTICAL: {
    double center_x = width / 2.0;
    double cylinder_width = draw_width;
    double ellipse_height = MIN(draw_height * 0.25, cylinder_width * 0.55);
    double half_ellipse = ellipse_height / 2.0;
    double top_center = inset + half_ellipse;
    double bottom_center = height - inset - half_ellipse;
    double body_height = bottom_center - top_center;

    if (filled) {
      cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
      cairo_rectangle(cr, center_x - cylinder_width / 2.0, top_center, cylinder_width, body_height);
      cairo_fill(cr);

      cairo_save(cr);
      cairo_translate(cr, center_x, top_center);
      cairo_scale(cr, cylinder_width / 2.0, half_ellipse);
      cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
      cairo_fill(cr);
      cairo_restore(cr);

      cairo_save(cr);
      cairo_translate(cr, center_x, bottom_center);
      cairo_scale(cr, cylinder_width / 2.0, half_ellipse);
      cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
      cairo_fill(cr);
      cairo_restore(cr);
    }

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);

    // Top ellipse outline
    cairo_save(cr);
    cairo_translate(cr, center_x, top_center);
    cairo_scale(cr, cylinder_width / 2.0, half_ellipse);
    cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    // Side lines
    cairo_new_path(cr);
    cairo_move_to(cr, center_x - cylinder_width / 2.0, top_center);
    cairo_line_to(cr, center_x - cylinder_width / 2.0, bottom_center);
    cairo_move_to(cr, center_x + cylinder_width / 2.0, top_center);
    cairo_line_to(cr, center_x + cylinder_width / 2.0, bottom_center);
    cairo_stroke(cr);

    // Bottom ellipse outline
    cairo_save(cr);
    cairo_translate(cr, center_x, bottom_center);
    cairo_scale(cr, cylinder_width / 2.0, half_ellipse);
    cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
    cairo_restore(cr);
    cairo_stroke(cr);
    break;
  }
  case SHAPE_CYLINDER_HORIZONTAL: {
    double center_y = height / 2.0;
    double cylinder_height = draw_height;
    double ellipse_width = MIN(draw_width * 0.25, cylinder_height * 0.55);
    double half_ellipse = ellipse_width / 2.0;
    double left_center = inset + half_ellipse;
    double right_center = width - inset - half_ellipse;
    double body_width = right_center - left_center;

    if (filled) {
      cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
      cairo_rectangle(cr, left_center, center_y - cylinder_height / 2.0, body_width, cylinder_height);
      cairo_fill(cr);

      cairo_save(cr);
      cairo_translate(cr, left_center, center_y);
      cairo_scale(cr, half_ellipse, cylinder_height / 2.0);
      cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
      cairo_fill(cr);
      cairo_restore(cr);

      cairo_save(cr);
      cairo_translate(cr, right_center, center_y);
      cairo_scale(cr, half_ellipse, cylinder_height / 2.0);
      cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
      cairo_fill(cr);
      cairo_restore(cr);
    }

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);

    // Left ellipse outline
    cairo_save(cr);
    cairo_translate(cr, left_center, center_y);
    cairo_scale(cr, half_ellipse, cylinder_height / 2.0);
    cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    // Right ellipse outline
    cairo_save(cr);
    cairo_translate(cr, right_center, center_y);
    cairo_scale(cr, half_ellipse, cylinder_height / 2.0);
    cairo_arc(cr, 0, 0, 1.0, 0.0, 2 * G_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    // Connecting lines
    cairo_new_path(cr);
    cairo_move_to(cr, left_center, center_y - cylinder_height / 2.0);
    cairo_line_to(cr, right_center, center_y - cylinder_height / 2.0);
    cairo_move_to(cr, right_center, center_y + cylinder_height / 2.0);
    cairo_line_to(cr, left_center, center_y + cylinder_height / 2.0);
    cairo_stroke(cr);
    break;
  }
  case SHAPE_LINE:
  case SHAPE_ARROW: {
    double start_x = inset;
    double start_y = height - inset;
    double end_x = width - inset;
    double end_y = inset;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, start_x, start_y);
    cairo_line_to(cr, end_x, end_y);
    cairo_stroke(cr);

    if (icon_data->shape_type == SHAPE_ARROW) {
      double angle = atan2(end_y - start_y, end_x - start_x);
      double arrow_len = 12.0;
      double arrow_angle = 160.0 * G_PI / 180.0; // 160 degrees

      double back_x = end_x - arrow_len * cos(angle);
      double back_y = end_y - arrow_len * sin(angle);

      double left_x = back_x + arrow_len * cos(angle - arrow_angle);
      double left_y = back_y + arrow_len * sin(angle - arrow_angle);
      double right_x = back_x + arrow_len * cos(angle + arrow_angle);
      double right_y = back_y + arrow_len * sin(angle + arrow_angle);

      cairo_move_to(cr, end_x, end_y);
      cairo_line_to(cr, left_x, left_y);
      cairo_move_to(cr, end_x, end_y);
      cairo_line_to(cr, right_x, right_y);
      cairo_stroke(cr);
    }
    return;
  }
  case SHAPE_BEZIER: {
    // Draw bezier curve
    double p0_x = inset;
    double p0_y = height / 2.0;
    double p1_x = width * 0.33;
    double p1_y = inset;
    double p2_x = width * 0.67;
    double p2_y = height - inset;
    double p3_x = width - inset;
    double p3_y = height / 2.0;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, p0_x, p0_y);
    cairo_curve_to(cr, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y);
    cairo_stroke(cr);
    return;
  }
  }

  if (icon_data->shape_type != SHAPE_CYLINDER_VERTICAL &&
      icon_data->shape_type != SHAPE_CYLINDER_HORIZONTAL) {
    if (filled) {
      cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
      cairo_fill_preserve(cr);
    }

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_stroke(cr);
  }
}

static void queue_icon_redraws(ShapeDialogData *data) {
  if (!data || !data->icon_widgets) return;
  for (guint i = 0; i < data->icon_widgets->len; i++) {
    GtkWidget *icon = g_ptr_array_index(data->icon_widgets, i);
    if (GTK_IS_WIDGET(icon)) {
      gtk_widget_queue_draw(icon);
    }
  }
}

static void on_stroke_style_changed(GtkComboBox *combo, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;
  if (!data) return;

  int active = gtk_combo_box_get_active(combo);
  switch (active) {
    case 0:
      data->stroke_style = STROKE_STYLE_SOLID;
      break;
    case 1:
      data->stroke_style = STROKE_STYLE_DASHED;
      break;
    case 2:
      data->stroke_style = STROKE_STYLE_DOTTED;
      break;
    default:
      data->stroke_style = STROKE_STYLE_SOLID;
      break;
  }
}

static void on_fill_style_changed(GtkComboBox *combo, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;
  if (!data) return;

  int active = gtk_combo_box_get_active(combo);
  switch (active) {
    case 0: // Outline
      data->filled = FALSE;
      data->fill_style = FILL_STYLE_SOLID;
      break;
    case 1: // Solid
      data->filled = TRUE;
      data->fill_style = FILL_STYLE_SOLID;
      break;
    case 2: // Hachure
      data->filled = TRUE;
      data->fill_style = FILL_STYLE_HACHURE;
      break;
    case 3: // Cross Hatch
      data->filled = TRUE;
      data->fill_style = FILL_STYLE_CROSS_HATCH;
      break;
    default:
      data->filled = FALSE;
      data->fill_style = FILL_STYLE_SOLID;
      break;
  }
  queue_icon_redraws(data);
}

static void on_shape_button_clicked(GtkButton *button, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;

  ShapeType shape_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "shape_type"));
  data->selected_shape = shape_type;

  g_signal_handlers_disconnect_by_func(data->dialog, G_CALLBACK(on_dialog_response), data);

  // Close dialog and proceed with shape creation
  gtk_window_destroy(GTK_WINDOW(data->dialog));

  // Set the canvas to shape drawing mode (shape mode only, not drawing mode)
  data->canvas_data->drawing_mode = FALSE;  // Ensure drawing mode is OFF
  data->canvas_data->shape_mode = TRUE;
  data->canvas_data->shape_stroke_style = data->stroke_style;
  data->canvas_data->shape_fill_style = data->fill_style;
  data->canvas_data->selected_shape_type = shape_type;
  if (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW || shape_type == SHAPE_BEZIER) {
    data->canvas_data->shape_filled = FALSE;
    data->filled = FALSE;
  } else {
    data->canvas_data->shape_filled = data->filled;
  }

  // Clear any existing drawing state to prevent interference
  data->canvas_data->current_drawing = NULL;

  // Update cursor
  canvas_set_cursor(data->canvas_data, data->canvas_data->draw_cursor);

  shape_dialog_data_free(data);
}

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_window_destroy(GTK_WINDOW(dialog));
    shape_dialog_data_free(data);
  }
}

static GtkWidget* create_shape_button(const char *label, ShapeType shape_type, ShapeDialogData *data) {
  GtkWidget *button = gtk_button_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(box, 6);
  gtk_widget_set_margin_end(box, 6);
  gtk_widget_set_margin_top(box, 6);
  gtk_widget_set_margin_bottom(box, 6);

  GtkWidget *icon = gtk_drawing_area_new();
  gtk_widget_set_size_request(icon, 56, 36);
  ShapeIconData *icon_data = g_new0(ShapeIconData, 1);
  icon_data->dialog_data = data;
  icon_data->shape_type = shape_type;
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(icon), draw_shape_icon, icon_data, g_free);

  if (!data->icon_widgets) {
    data->icon_widgets = g_ptr_array_new();
  }
  g_ptr_array_add(data->icon_widgets, icon);

  GtkWidget *label_widget = gtk_label_new(label);
  gtk_widget_set_halign(label_widget, GTK_ALIGN_CENTER);

  gtk_box_append(GTK_BOX(box), icon);
  gtk_box_append(GTK_BOX(box), label_widget);

  gtk_button_set_child(GTK_BUTTON(button), box);

  g_object_set_data(G_OBJECT(button), "shape_type", GINT_TO_POINTER(shape_type));
  g_signal_connect(button, "clicked", G_CALLBACK(on_shape_button_clicked), data);

  return button;
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
  data->filled = canvas_data->shape_filled;
  data->stroke_style = canvas_data->shape_stroke_style;
  data->fill_style = canvas_data->shape_fill_style;

  if (!data->filled) {
    data->fill_style = FILL_STYLE_SOLID;
  }

  if (data->stroke_style < STROKE_STYLE_SOLID || data->stroke_style > STROKE_STYLE_DOTTED) {
    data->stroke_style = STROKE_STYLE_SOLID;
  }
  if (data->fill_style < FILL_STYLE_SOLID || data->fill_style > FILL_STYLE_CROSS_HATCH) {
    data->fill_style = FILL_STYLE_SOLID;
  }

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

  GtkWidget *style_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(style_grid), 8);
  gtk_grid_set_column_spacing(GTK_GRID(style_grid), 12);
  gtk_widget_set_margin_start(style_grid, 8);
  gtk_widget_set_margin_end(style_grid, 8);
  gtk_widget_set_margin_top(style_grid, 6);
  gtk_box_append(GTK_BOX(vbox), style_grid);

  GtkWidget *fill_label = gtk_label_new("Fill Style");
  gtk_widget_set_halign(fill_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(style_grid), fill_label, 0, 0, 1, 1);

  GtkWidget *fill_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Outline");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Solid");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Hachure");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(fill_combo), NULL, "Cross Hatch");
  gtk_widget_set_hexpand(fill_combo, TRUE);
  int fill_index = 0;
  if (data->filled) {
    switch (data->fill_style) {
      case FILL_STYLE_HACHURE:
        fill_index = 2;
        break;
      case FILL_STYLE_CROSS_HATCH:
        fill_index = 3;
        break;
      default:
        fill_index = 1;
        break;
    }
  } else {
    fill_index = 0;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(fill_combo), fill_index);
  gtk_grid_attach(GTK_GRID(style_grid), fill_combo, 1, 0, 1, 1);
  data->fill_combo = fill_combo;
  g_signal_connect(fill_combo, "changed", G_CALLBACK(on_fill_style_changed), data);

  GtkWidget *stroke_label = gtk_label_new("Stroke Style");
  gtk_widget_set_halign(stroke_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(style_grid), stroke_label, 0, 1, 1, 1);

  GtkWidget *stroke_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Solid");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Dashed");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(stroke_combo), NULL, "Dotted");
  gtk_widget_set_hexpand(stroke_combo, TRUE);
  int stroke_index = 0;
  switch (data->stroke_style) {
    case STROKE_STYLE_DASHED:
      stroke_index = 1;
      break;
    case STROKE_STYLE_DOTTED:
      stroke_index = 2;
      break;
    default:
      stroke_index = 0;
      break;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(stroke_combo), stroke_index);
  gtk_grid_attach(GTK_GRID(style_grid), stroke_combo, 1, 1, 1, 1);
  data->stroke_combo = stroke_combo;
  g_signal_connect(stroke_combo, "changed", G_CALLBACK(on_stroke_style_changed), data);

  GtkWidget *shapes_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(shapes_grid), 10);
  gtk_grid_set_column_spacing(GTK_GRID(shapes_grid), 12);
  gtk_widget_set_halign(shapes_grid, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), shapes_grid);

  GtkWidget *circle_btn = create_shape_button("Circle", SHAPE_CIRCLE, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), circle_btn, 0, 0, 1, 1);

  GtkWidget *rectangle_btn = create_shape_button("Rectangle", SHAPE_RECTANGLE, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), rectangle_btn, 1, 0, 1, 1);

  GtkWidget *triangle_btn = create_shape_button("Triangle", SHAPE_TRIANGLE, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), triangle_btn, 2, 0, 1, 1);

  GtkWidget *vcylinder_btn = create_shape_button("V-Cylinder", SHAPE_CYLINDER_VERTICAL, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), vcylinder_btn, 0, 1, 1, 1);

  GtkWidget *hcylinder_btn = create_shape_button("H-Cylinder", SHAPE_CYLINDER_HORIZONTAL, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), hcylinder_btn, 1, 1, 1, 1);

  GtkWidget *diamond_button = create_shape_button("Diamond", SHAPE_DIAMOND, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), diamond_button, 2, 1, 1, 1);

  GtkWidget *rounded_rect_button = create_shape_button("Rounded Rect", SHAPE_ROUNDED_RECTANGLE, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), rounded_rect_button, 0, 2, 1, 1);

  GtkWidget *line_button = create_shape_button("Line", SHAPE_LINE, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), line_button, 1, 2, 1, 1);

  GtkWidget *arrow_button = create_shape_button("Arrow", SHAPE_ARROW, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), arrow_button, 2, 2, 1, 1);

  GtkWidget *bezier_button = create_shape_button("Bezier", SHAPE_BEZIER, data);
  gtk_grid_attach(GTK_GRID(shapes_grid), bezier_button, 0, 3, 1, 1);


  // Add Cancel button
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);

  g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), data);

  gtk_window_present(GTK_WINDOW(dialog));
}
