#include "canvas_shape_dialog.h"
#include "canvas/canvas_core.h"
#include "elements/shape.h"
#include <math.h>

typedef struct {
  CanvasData *canvas_data;
  GtkWidget *dialog;
  ShapeType selected_shape;
  gboolean filled;
  StrokeStyle stroke_style;
  FillStyle fill_style;
  GPtrArray *icon_widgets;
  GtkWidget *circle_btn;
  GtkWidget *rectangle_btn;
  GtkWidget *triangle_btn;
  GtkWidget *vcylinder_btn;
  GtkWidget *hcylinder_btn;
  GtkWidget *diamond_btn;
  GtkWidget *rounded_rect_btn;
  GtkWidget *trapezoid_btn;
  GtkWidget *line_btn;
  GtkWidget *arrow_btn;
  GtkWidget *bezier_btn;
  GtkWidget *curved_arrow_btn;
  GtkWidget *cube_btn;
  GtkWidget *plot_btn;
  GtkWidget *oval_btn;
  GtkWidget *text_outline_btn;
  GtkWidget *ascii_art_btn;
} ShapeDialogData;

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

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
  case SHAPE_OVAL: {
    cairo_save(cr);
    cairo_translate(cr, width / 2.0, height / 2.0);
    cairo_scale(cr, draw_width / 2.0, draw_height / 2.0);
    cairo_arc(cr, 0, 0, 1, 0, 2 * G_PI);
    cairo_restore(cr);
    break;
  }
  case SHAPE_RECTANGLE:
    cairo_rectangle(cr, inset, inset, draw_width, draw_height);
    break;
  case SHAPE_ROUNDED_RECTANGLE: {
    double width_adjustment = draw_width * 0.2;
    double adjusted_width = draw_width - width_adjustment;
    double x = inset + width_adjustment / 2.0;
    double y = inset;
    double right = x + adjusted_width;
    double bottom = y + draw_height;
    double radius = MIN(adjusted_width, draw_height) * 0.25;

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
  case SHAPE_TRAPEZOID: {
    double top_inset = draw_width * 0.2;
    cairo_move_to(cr, inset + top_inset, inset);
    cairo_line_to(cr, width - inset - top_inset, inset);
    cairo_line_to(cr, width - inset, height - inset);
    cairo_line_to(cr, inset, height - inset);
    cairo_close_path(cr);
    break;
  }
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
  case SHAPE_CURVED_ARROW: {
    // Draw bezier curve with arrowhead - smooth upward curve
    double p0_x = inset + 4;
    double p0_y = height - inset - 4;
    double p1_x = width * 0.3;
    double p1_y = height * 0.25;
    double p2_x = width * 0.7;
    double p2_y = height * 0.25;
    double p3_x = width - inset - 4;
    double p3_y = inset + 4;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, 2.5);

    cairo_move_to(cr, p0_x, p0_y);
    cairo_curve_to(cr, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y);
    cairo_stroke(cr);

    // Draw arrowhead - more prominent
    double dx = p3_x - p2_x;
    double dy = p3_y - p2_y;
    double angle = atan2(dy, dx);
    double arrow_len = 14.0;
    double arrow_angle = 155.0 * G_PI / 180.0;

    double back_x = p3_x - arrow_len * cos(angle);
    double back_y = p3_y - arrow_len * sin(angle);

    double left_x = back_x + arrow_len * cos(angle - arrow_angle);
    double left_y = back_y + arrow_len * sin(angle - arrow_angle);
    double right_x = back_x + arrow_len * cos(angle + arrow_angle);
    double right_y = back_y + arrow_len * sin(angle + arrow_angle);

    cairo_set_line_width(cr, 2.5);
    cairo_move_to(cr, p3_x, p3_y);
    cairo_line_to(cr, left_x, left_y);
    cairo_move_to(cr, p3_x, p3_y);
    cairo_line_to(cr, right_x, right_y);
    cairo_stroke(cr);
    return;
  }
  case SHAPE_TEXT_OUTLINE: {
    shape_render_text_outline_sample(cr,
                                     "TXT",
                                     inset,
                                     inset,
                                     draw_width,
                                     draw_height,
                                     0.95,
                                     0.95,
                                     0.95,
                                     1.0);
    return;
  }
  case SHAPE_ASCII_ART: {
    // Draw simple ASCII art sample
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

    // Create pango layout for monospace text
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 5");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    // Simple ASCII art sample (letter "M" in banner font style)
    const char *sample =
      " __  __ \n"
      "|  \\/  |\n"
      "| |\\/| |\n"
      "|_|  |_|";
    pango_layout_set_text(layout, sample, -1);

    // Center the text
    int text_w, text_h;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);
    cairo_move_to(cr, (width - text_w) / 2.0, (height - text_h) / 2.0);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    return;
  }
  case SHAPE_CUBE: {
    double offset = MIN(draw_width, draw_height) * 0.35;
    // Front face
    cairo_rectangle(cr, inset, inset + offset, draw_width - offset, draw_height - offset);
    // Top face
    cairo_move_to(cr, inset, inset + offset);
    cairo_line_to(cr, inset + offset, inset);
    cairo_line_to(cr, inset + draw_width, inset);
    cairo_line_to(cr, inset + draw_width - offset, inset + offset);
    cairo_close_path(cr);
    // Side face
    cairo_move_to(cr, inset + draw_width - offset, inset + offset);
    cairo_line_to(cr, inset + draw_width, inset);
    cairo_line_to(cr, inset + draw_width, inset + draw_height - offset);
    cairo_line_to(cr, inset + draw_width - offset, inset + draw_height);
    cairo_close_path(cr);
    break;
  }
  case SHAPE_PLOT: {
    // Draw plot icon with simple line graph
    double margin = inset + 2;

    // Draw axes
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, margin, margin);
    cairo_line_to(cr, margin, height - margin);
    cairo_line_to(cr, width - margin, height - margin);
    cairo_stroke(cr);

    // Draw simple plot line
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, stroke_width);
    double points[][2] = {
      {0.1, 0.7},
      {0.3, 0.3},
      {0.5, 0.5},
      {0.7, 0.2},
      {0.9, 0.4}
    };

    for (int i = 0; i < 5; i++) {
      double x = margin + points[i][0] * (width - 2 * margin);
      double y = margin + points[i][1] * (height - 2 * margin);
      if (i == 0) {
        cairo_move_to(cr, x, y);
      } else {
        cairo_line_to(cr, x, y);
      }
    }
    cairo_stroke(cr);

    // Draw points
    for (int i = 0; i < 5; i++) {
      double x = margin + points[i][0] * (width - 2 * margin);
      double y = margin + points[i][1] * (height - 2 * margin);
      cairo_arc(cr, x, y, 2.0, 0, 2 * G_PI);
      cairo_fill(cr);
    }

    // Skip normal fill/stroke for plot
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

static void on_stroke_style_changed(GObject *dropdown, GParamSpec *pspec, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;
  if (!data || !GTK_IS_DROP_DOWN(dropdown) || g_strcmp0(pspec->name, "selected") != 0) {
    return;
  }

  guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
  switch (selected) {
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
  queue_icon_redraws(data);
}

static void on_fill_style_changed(GObject *dropdown, GParamSpec *pspec, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;
  if (!data || !GTK_IS_DROP_DOWN(dropdown) || g_strcmp0(pspec->name, "selected") != 0) {
    return;
  }

  guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
  switch (selected) {
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
    default: // Outline
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
  if (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW || shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW || shape_type == SHAPE_TEXT_OUTLINE || shape_type == SHAPE_ASCII_ART) {
    data->canvas_data->shape_filled = FALSE;
    data->filled = FALSE;
  } else {
    data->canvas_data->shape_filled = data->filled;
  }

  // Clear any existing drawing state to prevent interference
  data->canvas_data->current_drawing = NULL;

  // Update cursor
  canvas_set_cursor(data->canvas_data, data->canvas_data->draw_cursor);

}

static void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_window_destroy(GTK_WINDOW(dialog));
  }
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  ShapeDialogData *data = (ShapeDialogData*)user_data;

  // Convert to lowercase for case-insensitive matching
  guint lower_keyval = gdk_keyval_to_lower(keyval);

  GtkWidget *button = NULL;

  switch (lower_keyval) {
    case GDK_KEY_c:
      button = data->circle_btn;
      break;
    case GDK_KEY_r:
      button = data->rectangle_btn;
      break;
    case GDK_KEY_t:
      button = data->triangle_btn;
      break;
    case GDK_KEY_v:
      button = data->vcylinder_btn;
      break;
    case GDK_KEY_h:
      button = data->hcylinder_btn;
      break;
    case GDK_KEY_d:
      button = data->diamond_btn;
      break;
    case GDK_KEY_o:
      button = data->rounded_rect_btn;
      break;
    case GDK_KEY_p:
      button = data->trapezoid_btn;
      break;
    case GDK_KEY_l:
      button = data->line_btn;
      break;
    case GDK_KEY_a:
      button = data->arrow_btn;
      break;
    case GDK_KEY_b:
      button = data->bezier_btn;
      break;
    case GDK_KEY_u:
      button = data->curved_arrow_btn;
      break;
    case GDK_KEY_x:
      button = data->text_outline_btn;
      break;
    case GDK_KEY_m:
      button = data->ascii_art_btn;
      break;
    case GDK_KEY_k:
      button = data->cube_btn;
      break;
    case GDK_KEY_g:
      button = data->plot_btn;
      break;
    case GDK_KEY_e:
      button = data->oval_btn;
      break;
    default:
      return FALSE; // Let other handlers process this key
  }

  if (button && GTK_IS_BUTTON(button)) {
    g_signal_emit_by_name(button, "clicked");
    return TRUE; // Key was handled
  }

  return FALSE;
}

static GtkWidget* create_shape_button(const char *tooltip, const char *shortcut, ShapeType shape_type, ShapeDialogData *data) {
  GtkWidget *button = gtk_button_new();
  gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
  gtk_widget_set_tooltip_text(button, tooltip);
  gtk_widget_add_css_class(button, "flat");
  gtk_widget_add_css_class(button, "shape-tile");

  GtkWidget *icon = gtk_drawing_area_new();
  gtk_widget_set_size_request(icon, 64, 48);
  ShapeIconData *icon_data = g_new0(ShapeIconData, 1);
  icon_data->dialog_data = data;
  icon_data->shape_type = shape_type;
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(icon), draw_shape_icon, icon_data, g_free);

  if (!data->icon_widgets) {
    data->icon_widgets = g_ptr_array_new();
  }
  g_ptr_array_add(data->icon_widgets, icon);

  GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(content, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(content), icon);

  if (shortcut && *shortcut) {
    GtkWidget *badge = gtk_label_new(NULL);
    gchar *markup = g_markup_printf_escaped("<small>%s</small>", shortcut);
    gtk_label_set_markup(GTK_LABEL(badge), markup);
    g_free(markup);
    gtk_widget_add_css_class(badge, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(badge), 0.5);
    gtk_box_append(GTK_BOX(content), badge);
  }

  gtk_button_set_child(GTK_BUTTON(button), content);

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
  g_object_set_data_full(G_OBJECT(dialog), "shape-dialog-data", data, (GDestroyNotify)shape_dialog_data_free);

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

  GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_bottom(header, 4);
  gtk_widget_set_margin_start(header, 4);
  gtk_widget_set_margin_end(header, 4);
  GtkWidget *title_label = gtk_label_new("Choose a shape");
  gtk_widget_add_css_class(title_label, "title-3");
  gtk_label_set_xalign(GTK_LABEL(title_label), 0.0);
  gtk_box_append(GTK_BOX(header), title_label);

  GtkWidget *subtitle_label = gtk_label_new("Pick a base and fine-tune stroke and fill styles before drawing.");
  gtk_widget_add_css_class(subtitle_label, "dim-label");
  gtk_label_set_wrap(GTK_LABEL(subtitle_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(subtitle_label), 0.0);
  gtk_box_append(GTK_BOX(header), subtitle_label);
  gtk_box_append(GTK_BOX(vbox), header);

  GtkWidget *divider = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_append(GTK_BOX(vbox), divider);

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

  const char *fill_options[] = {"Outline", "Solid", "Hachure", "Cross Hatch", NULL};
  GtkStringList *fill_model = gtk_string_list_new(fill_options);
  GtkWidget *fill_combo = gtk_drop_down_new(G_LIST_MODEL(fill_model), NULL);
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
  gtk_drop_down_set_selected(GTK_DROP_DOWN(fill_combo), fill_index);
  gtk_grid_attach(GTK_GRID(style_grid), fill_combo, 1, 0, 1, 1);
  g_signal_connect(fill_combo, "notify::selected", G_CALLBACK(on_fill_style_changed), data);

  GtkWidget *stroke_label = gtk_label_new("Stroke Style");
  gtk_widget_set_halign(stroke_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(style_grid), stroke_label, 0, 1, 1, 1);

  const char *stroke_options[] = {"Solid", "Dashed", "Dotted", NULL};
  GtkStringList *stroke_model = gtk_string_list_new(stroke_options);
  GtkWidget *stroke_combo = gtk_drop_down_new(G_LIST_MODEL(stroke_model), NULL);
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
  gtk_drop_down_set_selected(GTK_DROP_DOWN(stroke_combo), stroke_index);
  gtk_grid_attach(GTK_GRID(style_grid), stroke_combo, 1, 1, 1, 1);
  g_signal_connect(stroke_combo, "notify::selected", G_CALLBACK(on_stroke_style_changed), data);

  GtkWidget *shapes_flowbox = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(shapes_flowbox), GTK_SELECTION_NONE);
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(shapes_flowbox), 4);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(shapes_flowbox), 10);
  gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(shapes_flowbox), 10);
  gtk_widget_set_halign(shapes_flowbox, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), shapes_flowbox);

  data->rectangle_btn = create_shape_button("Rectangle (R)", "R", SHAPE_RECTANGLE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->rectangle_btn, -1);

  data->rounded_rect_btn = create_shape_button("Rounded Rect (O)", "O", SHAPE_ROUNDED_RECTANGLE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->rounded_rect_btn, -1);

  data->oval_btn = create_shape_button("Oval (E)", "E", SHAPE_OVAL, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->oval_btn, -1);

  data->circle_btn = create_shape_button("Circle (C)", "C", SHAPE_CIRCLE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->circle_btn, -1);

  data->triangle_btn = create_shape_button("Triangle (T)", "T", SHAPE_TRIANGLE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->triangle_btn, -1);

  data->diamond_btn = create_shape_button("Diamond (D)", "D", SHAPE_DIAMOND, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->diamond_btn, -1);

  data->trapezoid_btn = create_shape_button("Trapezoid (P)", "P", SHAPE_TRAPEZOID, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->trapezoid_btn, -1);

  data->line_btn = create_shape_button("Line (L)", "L", SHAPE_LINE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->line_btn, -1);

  data->arrow_btn = create_shape_button("Arrow (A)", "A", SHAPE_ARROW, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->arrow_btn, -1);

  data->bezier_btn = create_shape_button("Bezier (B)", "B", SHAPE_BEZIER, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->bezier_btn, -1);

  data->curved_arrow_btn = create_shape_button("Curved Arrow (U)", "U", SHAPE_CURVED_ARROW, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->curved_arrow_btn, -1);

  data->text_outline_btn = create_shape_button("Outline Text (X)", "X", SHAPE_TEXT_OUTLINE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->text_outline_btn, -1);

  data->ascii_art_btn = create_shape_button("ASCII Art (M)", "M", SHAPE_ASCII_ART, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->ascii_art_btn, -1);

  data->vcylinder_btn = create_shape_button("V-Cylinder (V)", "V", SHAPE_CYLINDER_VERTICAL, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->vcylinder_btn, -1);

  data->hcylinder_btn = create_shape_button("H-Cylinder (H)", "H", SHAPE_CYLINDER_HORIZONTAL, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->hcylinder_btn, -1);

  data->cube_btn = create_shape_button("Cube (K)", "K", SHAPE_CUBE, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->cube_btn, -1);

  data->plot_btn = create_shape_button("Plot (G)", "G", SHAPE_PLOT, data);
  gtk_flow_box_insert(GTK_FLOW_BOX(shapes_flowbox), data->plot_btn, -1);

  // Add Cancel button
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);

  g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), data);

  // Add keyboard event controller
  GtkEventController *key_controller = gtk_event_controller_key_new();
  g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), data);
  gtk_widget_add_controller(GTK_WIDGET(dialog), key_controller);

  gtk_window_present(GTK_WINDOW(dialog));
}
