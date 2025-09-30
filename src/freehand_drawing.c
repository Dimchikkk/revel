#include "freehand_drawing.h"
#include <cairo.h>
#include <graphene.h>
#include "model.h"

static void freehand_drawing_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  FreehandDrawing *drawing = (FreehandDrawing*)element;

  if (drawing->points->len < 2) return;

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  cairo_set_source_rgba(cr, drawing->base.bg_r, drawing->base.bg_g,
                        drawing->base.bg_b, drawing->base.bg_a);
  cairo_set_line_width(cr, drawing->stroke_width);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

  DrawingPoint *points = (DrawingPoint*)drawing->points->data;

  // Draw relative to element position
  cairo_move_to(cr, element->x + points[0].x, element->y + points[0].y);

  for (guint i = 1; i < drawing->points->len; i++) {
    cairo_line_to(cr, element->x + points[i].x, element->y + points[i].y);
  }

  cairo_stroke(cr);

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

    // Draw rotation handle
    element_draw_rotation_handle(element, cr);
  }
}

static void freehand_drawing_get_connection_point(Element *element, int point, int *cx, int *cy) {
  *cx = element->x + element->width / 2;
  *cy = element->y + element->height / 2;
}

static int freehand_drawing_pick_resize_handle(Element *element, int x, int y) {
  return -1;
}

static int freehand_drawing_pick_connection_point(Element *element, int x, int y) {
  return -1;
}

static void freehand_drawing_start_editing(Element *element, GtkWidget *overlay) {
  // Not editable via text
}

static void freehand_drawing_update_position(Element *element, int x, int y, int z) {
  element->x = x;
  element->y = y;
  element->z = z;
}

static void freehand_drawing_update_size(Element *element, int width, int height) {
  // Not supported
}

static void freehand_drawing_free(Element *element) {
  FreehandDrawing *drawing = (FreehandDrawing*)element;
  g_array_free(drawing->points, TRUE);
  g_free(element);
}

static ElementVTable freehand_drawing_vtable = {
  .draw = freehand_drawing_draw,
  .get_connection_point = freehand_drawing_get_connection_point,
  .pick_resize_handle = freehand_drawing_pick_resize_handle,
  .pick_connection_point = freehand_drawing_pick_connection_point,
  .start_editing = freehand_drawing_start_editing,
  .update_position = freehand_drawing_update_position,
  .update_size = freehand_drawing_update_size,
  .free = freehand_drawing_free
};

FreehandDrawing* freehand_drawing_create(ElementPosition position,
                                         ElementColor stroke_color,
                                         int stroke_width,
                                         CanvasData *data) {
  FreehandDrawing *drawing = g_new0(FreehandDrawing, 1);

  drawing->base.type = ELEMENT_FREEHAND_DRAWING;
  drawing->base.vtable = &freehand_drawing_vtable;
  drawing->base.x = position.x;
  drawing->base.y = position.y;
  drawing->base.z = position.z;
  drawing->base.width = 0;
  drawing->base.height = 0;
  drawing->base.canvas_data = data;

  drawing->base.bg_r = stroke_color.r;
  drawing->base.bg_g = stroke_color.g;
  drawing->base.bg_b = stroke_color.b;
  drawing->base.bg_a = stroke_color.a;
  drawing->stroke_width = stroke_width;

  drawing->points = g_array_new(FALSE, FALSE, sizeof(DrawingPoint));

  return drawing;
}

void freehand_drawing_add_point(FreehandDrawing *drawing, int x, int y) {
  // Store points relative to the element's position
  DrawingPoint point;
  graphene_point_init(&point, (float)(x - drawing->base.x), (float)(y - drawing->base.y));
  g_array_append_val(drawing->points, point);

  // Update bounding box based on relative points
  if (drawing->points->len == 1) {
    // First point - set initial bounds
    drawing->base.width = 1;
    drawing->base.height = 1;
  } else {
    // Update bounds to include all points
    DrawingPoint *points = (DrawingPoint*)drawing->points->data;
    float min_x = points[0].x;
    float min_y = points[0].y;
    float max_x = points[0].x;
    float max_y = points[0].y;

    for (guint i = 1; i < drawing->points->len; i++) {
      min_x = MIN(min_x, points[i].x);
      min_y = MIN(min_y, points[i].y);
      max_x = MAX(max_x, points[i].x);
      max_y = MAX(max_y, points[i].y);
    }

    // Add some padding for the stroke width
    float padding = drawing->stroke_width / 2.0f;
    drawing->base.width = (int)(max_x - min_x + padding * 2);
    drawing->base.height = (int)(max_y - min_y + padding * 2);

    // Adjust position if needed to maintain the relative points
    if (min_x < 0) {
      drawing->base.x += (int)min_x;
      // Adjust all points to be relative to new position
      for (guint i = 0; i < drawing->points->len; i++) {
        points[i].x -= min_x;
      }
      drawing->base.width += (int)(-min_x);
    }

    if (min_y < 0) {
      drawing->base.y += (int)min_y;
      for (guint i = 0; i < drawing->points->len; i++) {
        points[i].y -= min_y;
      }
      drawing->base.height += (int)(-min_y);
    }
  }
}
