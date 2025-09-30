#include "shape.h"
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>
#include "model.h"
#include "canvas_core.h"
#include "undo_manager.h"
#include <graphene.h>

gboolean shape_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  Shape *shape = (Shape*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) {
      // Ctrl+Enter inserts a newline
      GtkTextView *text_view = GTK_TEXT_VIEW(shape->text_view);
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_insert(buffer, &iter, "\n", 1);

      return TRUE; // Handled - prevent default behavior
    } else {
      // Enter finishes editing
      shape_finish_editing((Element*)shape);
      return TRUE;
    }
  }
  return FALSE;
}

static void shape_update_text_view_position(Shape *shape) {
  if (!shape->scrolled_window || !GTK_IS_WIDGET(shape->scrolled_window)) return;

  int screen_x, screen_y;
  canvas_canvas_to_screen(shape->base.canvas_data,
                          shape->base.x, shape->base.y,
                          &screen_x, &screen_y);
  gtk_widget_set_margin_start(shape->scrolled_window, screen_x - 10);
  gtk_widget_set_margin_top(shape->scrolled_window, screen_y - 10);
  gtk_widget_set_size_request(shape->scrolled_window,
                              shape->base.width + 20,
                              shape->base.height + 20);
}

static void draw_hatch_lines(cairo_t *cr, double cx, double cy, double span, double spacing, double angle) {
  double dir_x = cos(angle);
  double dir_y = sin(angle);
  double perp_x = -dir_y;
  double perp_y = dir_x;
  double half_span = span / 2.0;
  double max_offset = span;

  for (double offset = -max_offset; offset <= max_offset; offset += spacing) {
    double start_x = cx + perp_x * offset - dir_x * half_span;
    double start_y = cy + perp_y * offset - dir_y * half_span;
    double end_x = cx + perp_x * offset + dir_x * half_span;
    double end_y = cy + perp_y * offset + dir_y * half_span;
    cairo_move_to(cr, start_x, start_y);
    cairo_line_to(cr, end_x, end_y);
  }
}

static void build_vertical_cylinder_path(cairo_t *cr, double x, double y, double width, double height) {
  double ellipse_h = height * 0.15;
  double center_x = x + width / 2.0;
  double top_y = y + ellipse_h / 2.0;
  double bottom_y = y + height - ellipse_h / 2.0;

  cairo_rectangle(cr, x, top_y, width, bottom_y - top_y);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, center_x, top_y);
  cairo_scale(cr, width / 2.0, ellipse_h / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, center_x, bottom_y);
  cairo_scale(cr, width / 2.0, ellipse_h / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);
}

static void build_horizontal_cylinder_path(cairo_t *cr, double x, double y, double width, double height) {
  double ellipse_w = width * 0.15;
  double center_y = y + height / 2.0;
  double left_x = x + ellipse_w / 2.0;
  double right_x = x + width - ellipse_w / 2.0;

  cairo_rectangle(cr, left_x, y, right_x - left_x, height);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, left_x, center_y);
  cairo_scale(cr, ellipse_w / 2.0, height / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, right_x, center_y);
  cairo_scale(cr, ellipse_w / 2.0, height / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);
}

static void apply_fill(Shape *shape, cairo_t *cr) {
  if (!shape->filled) return;

  cairo_path_t *path = cairo_copy_path(cr);

  double x1, y1, x2, y2;
  cairo_path_extents(cr, &x1, &y1, &x2, &y2);
  double width = MAX(x2 - x1, 1.0);
  double height = MAX(y2 - y1, 1.0);

  if (shape->fill_style == FILL_STYLE_SOLID) {
    cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
    cairo_fill_preserve(cr);
    cairo_path_destroy(path);
    return;
  }

  cairo_save(cr);
  cairo_new_path(cr);
  cairo_append_path(cr, path);
  cairo_clip(cr);

  cairo_set_dash(cr, NULL, 0, 0);
  double spacing = MAX(4.0, shape->stroke_width * 2.0);
  double pattern_alpha = MIN(1.0, shape->base.bg_a);
  double line_width = MAX(1.0, shape->stroke_width * 0.35);
  cairo_set_line_width(cr, line_width);
  cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, pattern_alpha);

  double cx = (x1 + x2) / 2.0;
  double cy = (y1 + y2) / 2.0;
  double span = hypot(width, height) + spacing * 2.0;

  cairo_new_path(cr);
  draw_hatch_lines(cr, cx, cy, span, spacing, G_PI / 4.0);
  cairo_stroke(cr);

  if (shape->fill_style == FILL_STYLE_CROSS_HATCH) {
    cairo_new_path(cr);
    draw_hatch_lines(cr, cx, cy, span, spacing, -G_PI / 4.0);
    cairo_stroke(cr);
  }

  cairo_restore(cr);
  cairo_new_path(cr);
  cairo_append_path(cr, path);
  cairo_path_destroy(path);
}

static void shape_get_connection_point(Element *element, int point, int *cx, int *cy) {
  Shape *shape = (Shape*)element;
  int unrotated_x, unrotated_y;

  if (shape->has_line_points &&
      (shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW)) {
    double start_x = element->x + shape->line_start_u * element->width;
    double start_y = element->y + shape->line_start_v * element->height;
    double end_x = element->x + shape->line_end_u * element->width;
    double end_y = element->y + shape->line_end_v * element->height;
    double mid_x = (start_x + end_x) / 2.0;
    double mid_y = (start_y + end_y) / 2.0;

    switch (point) {
      case 0:
        unrotated_x = (int)round(start_x);
        unrotated_y = (int)round(start_y);
        break;
      case 1:
        unrotated_x = (int)round(end_x);
        unrotated_y = (int)round(end_y);
        break;
      case 2:
        unrotated_x = (int)round(mid_x);
        unrotated_y = (int)round(mid_y);
        break;
      case 3:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
        break;
      default:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
        break;
    }
  } else {
    switch (point) {
      case 0: // Top
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y;
        break;
      case 1: // Right
        unrotated_x = element->x + element->width;
        unrotated_y = element->y + element->height / 2;
        break;
      case 2: // Bottom
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height;
        break;
      case 3: // Left
        unrotated_x = element->x;
        unrotated_y = element->y + element->height / 2;
        break;
      default:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
    }
  }

  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = unrotated_x - center_x;
    double dy = unrotated_y - center_y;
    double angle_rad = element->rotation_degrees * M_PI / 180.0;
    *cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    *cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  } else {
    *cx = unrotated_x;
    *cy = unrotated_y;
  }
}

static void shape_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  Shape *shape = (Shape*)element;

  if (shape->editing) {
    shape_update_text_view_position(shape);
  }

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Set stroke style (dashed, dotted, or solid)
  if (shape->stroke_style == STROKE_STYLE_DASHED) {
    double dashes[] = {10.0, 5.0};
    cairo_set_dash(cr, dashes, 2, 0);
  } else if (shape->stroke_style == STROKE_STYLE_DOTTED) {
    double dashes[] = {2.0, 3.0};
    cairo_set_dash(cr, dashes, 2, 0);
  } else {
    cairo_set_dash(cr, NULL, 0, 0);  // Solid line
  }

  cairo_set_line_width(cr, shape->stroke_width);
  cairo_new_path(cr);

  switch (shape->shape_type) {
    case SHAPE_CIRCLE:
      {
        // Draw circle centered in the element's bounding box
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;
        double radius = MIN(element->width, element->height) / 2.0;

        cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
        apply_fill(shape, cr);

        // Handle stroke
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_RECTANGLE:
      {
        cairo_rectangle(cr, element->x, element->y, element->width, element->height);
        apply_fill(shape, cr);

        // Handle stroke
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_ROUNDED_RECTANGLE:
      {
        double radius = MIN(element->width, element->height) * 0.2;
        if (radius < 8.0) radius = 8.0;
        double x = element->x;
        double y = element->y;
        double width = element->width;
        double height = element->height;

        double right = x + width;
        double bottom = y + height;

        cairo_new_sub_path(cr);
        cairo_arc(cr, right - radius, y + radius, radius, -G_PI_2, 0);
        cairo_arc(cr, right - radius, bottom - radius, radius, 0, G_PI_2);
        cairo_arc(cr, x + radius, bottom - radius, radius, G_PI_2, G_PI);
        cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI_2);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_TRIANGLE:
      {
        // Draw equilateral triangle
        double center_x = element->x + element->width / 2.0;
        double top_y = element->y;
        double bottom_y = element->y + element->height;

        cairo_move_to(cr, center_x, top_y);
        cairo_line_to(cr, element->x, bottom_y);
        cairo_line_to(cr, element->x + element->width, bottom_y);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_CYLINDER_VERTICAL:
      {
        // Draw vertical cylinder (complete ellipses at top and bottom, connected by lines)
        double ellipse_w = element->width;
        double ellipse_h = element->height * 0.15; // 15% of height for ellipse
        double center_x = element->x + element->width / 2.0;
        double top_y = element->y + ellipse_h / 2.0;
        double bottom_y = element->y + element->height - ellipse_h / 2.0;

        // Fill the cylinder body if filled
        if (shape->filled) {
          if (shape->fill_style == FILL_STYLE_SOLID) {
            cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
            cairo_rectangle(cr, element->x, top_y, element->width, bottom_y - top_y);
            cairo_fill(cr);
          } else {
            cairo_new_path(cr);
            build_vertical_cylinder_path(cr, element->x, element->y, element->width, element->height);
            apply_fill(shape, cr);
            cairo_new_path(cr);
          }
        }

        // Draw top ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, center_x, top_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);

        // Draw side lines
        cairo_move_to(cr, element->x, top_y);
        cairo_line_to(cr, element->x, bottom_y);
        cairo_move_to(cr, element->x + element->width, top_y);
        cairo_line_to(cr, element->x + element->width, bottom_y);
        cairo_stroke(cr);

        // Draw bottom ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, center_x, bottom_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_CYLINDER_HORIZONTAL:
      {
        // Draw horizontal cylinder (complete ellipses at left and right, connected by lines)
        double ellipse_w = element->width * 0.15; // 15% of width for ellipse
        double ellipse_h = element->height;
        double center_y = element->y + element->height / 2.0;
        double left_x = element->x + ellipse_w / 2.0;
        double right_x = element->x + element->width - ellipse_w / 2.0;

        // Fill the cylinder body if filled
        if (shape->filled) {
          if (shape->fill_style == FILL_STYLE_SOLID) {
            cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
            cairo_rectangle(cr, left_x, element->y, right_x - left_x, element->height);
            cairo_fill(cr);
          } else {
            cairo_new_path(cr);
            build_horizontal_cylinder_path(cr, element->x, element->y, element->width, element->height);
            apply_fill(shape, cr);
            cairo_new_path(cr);
          }
        }

        // Draw left ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, left_x, center_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);

        // Draw top and bottom lines
        cairo_move_to(cr, left_x, element->y);
        cairo_line_to(cr, right_x, element->y);
        cairo_move_to(cr, left_x, element->y + element->height);
        cairo_line_to(cr, right_x, element->y + element->height);
        cairo_stroke(cr);

        // Draw right ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, right_x, center_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_DIAMOND:
      {
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;

        cairo_move_to(cr, center_x, element->y);
        cairo_line_to(cr, element->x + element->width, center_y);
        cairo_line_to(cr, center_x, element->y + element->height);
        cairo_line_to(cr, element->x, center_y);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_LINE:
    case SHAPE_ARROW:
      {
        double width = MAX(element->width, 1);
        double height = MAX(element->height, 1);
        double start_u = shape->has_line_points ? shape->line_start_u : 0.0;
        double start_v = shape->has_line_points ? shape->line_start_v : 0.0;
        double end_u = shape->has_line_points ? shape->line_end_u : 1.0;
        double end_v = shape->has_line_points ? shape->line_end_v : 1.0;

        double start_x = element->x + start_u * width;
        double start_y = element->y + start_v * height;
        double end_x = element->x + end_u * width;
        double end_y = element->y + end_v * height;

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);

        cairo_move_to(cr, start_x, start_y);
        cairo_line_to(cr, end_x, end_y);
        cairo_stroke(cr);

        if (shape->shape_type == SHAPE_ARROW) {
          double angle = atan2(end_y - start_y, end_x - start_x);
          double arrow_length = MAX(shape->stroke_width * 3.0, 12.0);
          double arrow_angle = 160.0 * G_PI / 180.0; // 160 degrees

          double back_x = end_x - arrow_length * cos(angle);
          double back_y = end_y - arrow_length * sin(angle);

          double left_x = back_x + arrow_length * cos(angle - arrow_angle);
          double left_y = back_y + arrow_length * sin(angle - arrow_angle);
          double right_x = back_x + arrow_length * cos(angle + arrow_angle);
          double right_y = back_y + arrow_length * sin(angle + arrow_angle);

          cairo_move_to(cr, end_x, end_y);
          cairo_line_to(cr, left_x, left_y);
          cairo_move_to(cr, end_x, end_y);
          cairo_line_to(cr, right_x, right_y);
          cairo_stroke(cr);
        }
      }
      break;
  }

  // Restore cairo state before drawing selection UI
  cairo_restore(cr);

  if (is_selected) {
    // Draw selection outline (with rotation)
    cairo_save(cr);
    if (element->rotation_degrees != 0.0) {
      double center_x = element->x + element->width / 2.0;
      double center_y = element->y + element->height / 2.0;
      cairo_translate(cr, center_x, center_y);
      cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
      cairo_translate(cr, -center_x, -center_y);
    }
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_stroke(cr);
    cairo_restore(cr);

    // Draw connection points (with rotation applied)
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      shape_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 7, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.2, 0.2, 0.9, 0.6);
      cairo_fill(cr);
      cairo_arc(cr, cx, cy, 7, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.1, 0.1, 0.7, 0.8);
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }

    // Draw rotation handle (without rotation)
    element_draw_rotation_handle(element, cr);
  }

  // Save state again for text
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw text if not editing and text exists
  if (!shape->editing && shape->text && strlen(shape->text) > 0) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string(shape->font_description);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, shape->text, -1);
    pango_layout_set_width(layout, (element->width - 20) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    cairo_set_source_rgba(cr, shape->text_r, shape->text_g, shape->text_b, shape->text_a);

    // Center text both horizontally and vertically with proper padding
    int padding = 10;
    int available_height = element->height - (2 * padding);

    int text_x = element->x + padding;
    int text_y = element->y + padding + (available_height - text_height) / 2;

    // Ensure text doesn't go outside bounds
    if (text_y < element->y + padding) {
      text_y = element->y + padding;
    }

    if (text_height <= available_height) {
      cairo_move_to(cr, text_x, text_y);
      pango_cairo_show_layout(cr, layout);
    } else {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_height(layout, available_height * PANGO_SCALE);
      cairo_move_to(cr, text_x, element->y + padding);
      pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
  }

  // Restore cairo state at end
  cairo_restore(cr);
}


static int shape_pick_resize_handle(Element *element, int x, int y) {
  // Apply inverse rotation to mouse coordinates if element is rotated
  double rotated_cx = x;
  double rotated_cy = y;
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = x - center_x;
    double dy = y - center_y;
    double angle_rad = -element->rotation_degrees * M_PI / 180.0;
    rotated_cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    rotated_cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  }

  int size = 8;
  struct { int px, py; } handles[4] = {
    {element->x, element->y},
    {element->x + element->width, element->y},
    {element->x + element->width, element->y + element->height},
    {element->x, element->y + element->height}
  };

  for (int i = 0; i < 4; i++) {
    if (abs(rotated_cx - handles[i].px) <= size && abs(rotated_cy - handles[i].py) <= size) {
      return i;
    }
  }
  return -1;
}

static int shape_pick_connection_point(Element *element, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int px, py;
    shape_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    int dist_sq = dx * dx + dy * dy;
    if (dist_sq < 100) {
      return i;
    }
  }
  return -1;
}



static void shape_start_editing(Element *element, GtkWidget *overlay) {
  Shape *shape = (Shape*)element;
  shape->editing = TRUE;

  if (!shape->text_view) {
    // Create scrolled window
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    // Create text view
    shape->text_view = gtk_text_view_new();

    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), shape->text_view);

    // Set size with some padding for scrollbars
    gtk_widget_set_size_request(scrolled_window, element->width + 20, element->height + 20);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), scrolled_window);
    gtk_widget_set_halign(scrolled_window, GTK_ALIGN_START);
    gtk_widget_set_valign(scrolled_window, GTK_ALIGN_START);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, element->x, element->y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(scrolled_window, screen_x - 10); // Adjust for padding
    gtk_widget_set_margin_top(scrolled_window, screen_y - 10);   // Adjust for padding

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(shape_on_textview_key_press), shape);
    gtk_widget_add_controller(shape->text_view, key_controller);

    // Store the scrolled window reference if needed for later access
    shape->scrolled_window = scrolled_window;
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(shape->text_view));
  gtk_text_buffer_set_text(buffer, shape->text, -1);

  gtk_widget_show(shape->scrolled_window ? shape->scrolled_window : shape->text_view);
  gtk_widget_grab_focus(shape->text_view);
}

void shape_finish_editing(Element *element) {
  Shape *shape = (Shape*)element;
  if (!shape->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(shape->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  char* old_text = g_strdup(shape->text);
  g_free(shape->text);
  shape->text = new_text;

  Model* model = shape->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  undo_manager_push_text_action(shape->base.canvas_data->undo_manager, model_element, old_text, new_text);
  model_update_text(model, model_element, new_text);

  shape->editing = FALSE;

  // Hide the scrolled window instead of the text view
  if (shape->scrolled_window) {
    gtk_widget_hide(shape->scrolled_window);
  } else {
    gtk_widget_hide(shape->text_view);
  }

  // Queue redraw using the stored canvas data
  if (shape->base.canvas_data && shape->base.canvas_data->drawing_area) {
    canvas_sync_with_model(shape->base.canvas_data);
    gtk_widget_queue_draw(shape->base.canvas_data->drawing_area);
    gtk_widget_grab_focus(shape->base.canvas_data->drawing_area);
  }
}

static void shape_update_position(Element *element, int x, int y, int z) {
  Shape *shape = (Shape*)element;
  element->x = x;
  element->y = y;
  element->z = z;
  if (shape->scrolled_window) {
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, x, y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(shape->scrolled_window, screen_x - 10);
    gtk_widget_set_margin_top(shape->scrolled_window, screen_y - 10);
  }
  if (shape->editing) {
    shape_update_text_view_position(shape);
  }
}

static void shape_update_size(Element *element, int width, int height) {
  Shape *shape = (Shape*)element;
  element->width = width;
  element->height = height;
  if (shape->scrolled_window) {
    gtk_widget_set_size_request(shape->scrolled_window, width + 20, height + 20);
  }
  if (shape->editing) {
    shape_update_text_view_position(shape);
  }
}

void shape_free(Element *element) {
  Shape *shape = (Shape*)element;
  if (shape->text) g_free(shape->text);
  if (shape->font_description) g_free(shape->font_description);
  if (shape->scrolled_window && GTK_IS_WIDGET(shape->scrolled_window) &&
      gtk_widget_get_parent(shape->scrolled_window)) {
    gtk_widget_unparent(shape->scrolled_window);
  }
  g_free(element);
}

static ElementVTable shape_vtable = {
  .draw = shape_draw,
  .get_connection_point = shape_get_connection_point,
  .pick_resize_handle = shape_pick_resize_handle,
  .pick_connection_point = shape_pick_connection_point,
  .start_editing = shape_start_editing,
  .update_position = shape_update_position,
  .update_size = shape_update_size,
  .free = shape_free,
};

Shape* shape_create(ElementPosition position,
                   ElementSize size,
                   ElementColor color,
                   int stroke_width,
                   ShapeType shape_type,
                   gboolean filled,
                   ElementText text,
                   ElementShape shape_config,
                   const ElementDrawing *drawing_config,
                   CanvasData *data) {
  Shape *shape = g_new0(Shape, 1);

  shape->base.type = ELEMENT_SHAPE;
  shape->base.vtable = &shape_vtable;
  shape->base.x = position.x;
  shape->base.y = position.y;
  shape->base.z = position.z;
  shape->base.width = size.width;
  shape->base.height = size.height;
  shape->base.bg_r = color.r;
  shape->base.bg_g = color.g;
  shape->base.bg_b = color.b;
  shape->base.bg_a = color.a;

  shape->shape_type = shape_config.shape_type;
  shape->stroke_width = shape_config.stroke_width;
  shape->filled = shape_config.filled;
  shape->stroke_style = shape_config.stroke_style;
  shape->fill_style = shape_config.fill_style;
  shape->stroke_r = shape_config.stroke_color.r;
  shape->stroke_g = shape_config.stroke_color.g;
  shape->stroke_b = shape_config.stroke_color.b;
  shape->stroke_a = shape_config.stroke_color.a;
  shape->base.canvas_data = data;

  shape->text = g_strdup(text.text);
  shape->text_r = text.text_color.r;
  shape->text_g = text.text_color.g;
  shape->text_b = text.text_color.b;
  shape->text_a = text.text_color.a;
  shape->font_description = g_strdup(text.font_description);
  shape->text_view = NULL;
  shape->scrolled_window = NULL;
  shape->editing = FALSE;
  shape->has_line_points = FALSE;
  shape->line_start_u = 0.0;
  shape->line_start_v = 0.0;
  shape->line_end_u = 1.0;
  shape->line_end_v = 1.0;

  if (drawing_config && drawing_config->drawing_points && drawing_config->drawing_points->len >= 2) {
    DrawingPoint *points = (DrawingPoint*)drawing_config->drawing_points->data;
    shape->line_start_u = points[0].x;
    shape->line_start_v = points[0].y;
    shape->line_end_u = points[1].x;
    shape->line_end_v = points[1].y;
    shape->has_line_points = TRUE;
  }

  return shape;
}
