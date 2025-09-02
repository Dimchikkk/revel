#include "pango/pango-layout.h"
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <math.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

// Vector structure and arrow utilities
typedef struct {
  double x, y;
} Vec2;

typedef struct {
  int x, y, width, height;
  char *text;
  GtkWidget *text_view;
  gboolean editing;
  int z_index;

  gboolean dragging;
  int drag_offset_x;
  int drag_offset_y;

  gboolean resizing;
  int resize_edge;
  int resize_start_x, resize_start_y;
  int orig_x, orig_y, orig_width, orig_height;
} Note;

typedef struct {
  Note *from;
  int from_point;
  Note *to;
  int to_point;
} Connection;

typedef struct {
  GList *notes;
  GList *connections;
  GList *selected_notes;
  GtkWidget *drawing_area;
  GtkWidget *overlay;
  int next_z_index;

  gboolean selecting;
  int start_x, start_y;
  int current_x, current_y;
  GdkModifierType modifier_state;

  GdkCursor *default_cursor;
  GdkCursor *move_cursor;
  GdkCursor *resize_cursor;
  GdkCursor *connect_cursor;
  GdkCursor *current_cursor;
} CanvasData;

/* Forward declarations */
static Note* pick_note(CanvasData *data, int x, int y);
static int pick_resize_handle(Note *note, int x, int y);
static int pick_connection_point(Note *note, int x, int y);

/* Vector utility functions */
static Vec2 vec2_add(Vec2 a, Vec2 b) {
  return (Vec2){a.x + b.x, a.y + b.y};
}

static Vec2 vec2_div(Vec2 v, double scalar) {
  return (Vec2){v.x / scalar, v.y / scalar};
}

/* Parallel arrow mid-point calculation */
static void parallel_arrow_mid(Vec2 start, Vec2 end, int start_pos, int end_pos, Vec2 *mid1, Vec2 *mid2) {
  Vec2 mid = vec2_div(vec2_add(start, end), 2.0);

  switch (start_pos) {
  case 0: // TOP
    switch (end_pos) {
    case 2: // BOTTOM
      *mid1 = (Vec2){start.x, mid.y};
      *mid2 = (Vec2){end.x, mid.y};
      return;
    case 1: // RIGHT
    case 3: // LEFT
      *mid1 = (Vec2){start.x, end.y};
      *mid2 = (Vec2){start.x, end.y};
      return;
    default:
      break;
    }
    break;

  case 2: // BOTTOM
    switch (end_pos) {
    case 0: // TOP
      *mid1 = (Vec2){start.x, mid.y};
      *mid2 = (Vec2){end.x, mid.y};
      return;
    case 1: // RIGHT
    case 3: // LEFT
      *mid1 = (Vec2){start.x, end.y};
      *mid2 = (Vec2){start.x, end.y};
      return;
    default:
      break;
    }
    break;

  case 3: // LEFT
    switch (end_pos) {
    case 1: // RIGHT
      *mid1 = (Vec2){mid.x, start.y};
      *mid2 = (Vec2){mid.x, end.y};
      return;
    case 0: // TOP
    case 2: // BOTTOM
      *mid1 = (Vec2){end.x, start.y};
      *mid2 = (Vec2){end.x, start.y};
      return;
    default:
      break;
    }
    break;

  case 1: // RIGHT
    switch (end_pos) {
    case 3: // LEFT
      *mid1 = (Vec2){mid.x, start.y};
      *mid2 = (Vec2){mid.x, end.y};
      return;
    case 0: // TOP
    case 2: // BOTTOM
      *mid1 = (Vec2){end.x, start.y};
      *mid2 = (Vec2){end.x, start.y};
      return;
    default:
      break;
    }
    break;
  }

  *mid1 = mid;
  *mid2 = mid;
}

static void draw_arrow_head(cairo_t *cr, Vec2 base, Vec2 tip) {
  Vec2 direction = {tip.x - base.x, tip.y - base.y};
  double length = sqrt(direction.x * direction.x + direction.y * direction.y);
  if (length < 1e-6) return;

  // Normalize
  direction.x /= length;
  direction.y /= length;

  // Perpendicular vectors for arrow head
  Vec2 perp1 = {-direction.y * 8, direction.x * 8};
  Vec2 perp2 = {direction.y * 8, -direction.x * 8};

  // Arrow head points
  Vec2 back = {tip.x - direction.x * 12, tip.y - direction.y * 12};
  Vec2 head1 = {back.x + perp1.x, back.y + perp1.y};
  Vec2 head2 = {back.x + perp2.x, back.y + perp2.y};

  cairo_move_to(cr, tip.x, tip.y);
  cairo_line_to(cr, head1.x, head1.y);
  cairo_line_to(cr, head2.x, head2.y);
  cairo_close_path(cr);
  cairo_fill(cr);
}

/* Parallel arrow drawing */
static void draw_parallel_arrow(cairo_t *cr, Vec2 start, Vec2 end, int start_pos, int end_pos) {
  Vec2 mid1, mid2;
  parallel_arrow_mid(start, end, start_pos, end_pos, &mid1, &mid2);

  cairo_move_to(cr, start.x, start.y);
  cairo_line_to(cr, mid1.x, mid1.y);
  cairo_line_to(cr, mid2.x, mid2.y);
  cairo_line_to(cr, end.x, end.y);
  cairo_stroke(cr);

  draw_arrow_head(cr, mid2, end);
}

/* Utility to get connection point coordinates */
static void get_connection_point(Note *note, int point, int *cx, int *cy) {
  switch(point) {
  case 0: *cx = note->x + note->width/2; *cy = note->y; break;
  case 1: *cx = note->x + note->width; *cy = note->y + note->height/2; break;
  case 2: *cx = note->x + note->width/2; *cy = note->y + note->height; break;
  case 3: *cx = note->x; *cy = note->y + note->height/2; break;
  }
}

/* Set cursor for drawing area */
static void set_cursor(CanvasData *data, GdkCursor *cursor) {
  if (data->current_cursor != cursor) {
    gtk_widget_set_cursor(data->drawing_area, cursor);
    data->current_cursor = cursor;
  }
}

/* Update cursor based on mouse position */
static void update_cursor(CanvasData *data, int x, int y) {
  Note *note = pick_note(data, x, y);

  if (note) {
    int rh = pick_resize_handle(note, x, y);
    if (rh >= 0) {
      switch (rh) {
      case 0: case 2:
        set_cursor(data, gdk_cursor_new_from_name("nwse-resize", NULL));
        break;
      case 1: case 3:
        set_cursor(data, gdk_cursor_new_from_name("nesw-resize", NULL));
        break;
      }
      return;
    }

    int cp = pick_connection_point(note, x, y);
    if (cp >= 0) {
      set_cursor(data, gdk_cursor_new_from_name("crosshair", NULL));
      return;
    }

    set_cursor(data, gdk_cursor_new_from_name("move", NULL));
    return;
  }

  set_cursor(data, gdk_cursor_new_from_name("default", NULL));
}

/* Draw the note and text */
static void draw_note(cairo_t *cr, Note *note, gboolean is_selected) {
  cairo_rectangle(cr, note->x, note->y, note->width, note->height);
  cairo_clip(cr);

  if (is_selected) {
    cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
  } else {
    cairo_set_source_rgb(cr, 1, 1, 0.8);
  }
  cairo_rectangle(cr, note->x, note->y, note->width, note->height);
  cairo_fill_preserve(cr);

  cairo_set_source_rgb(cr, 0.5, 0.5, 0.3);
  cairo_set_line_width(cr, 1.5);
  cairo_stroke(cr);

  for (int i = 0; i < 4; i++) {
    int cx, cy;
    get_connection_point(note, i, &cx, &cy);
    cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3); // RGBA with alpha 0.3
    cairo_fill(cr);
  }

  if (!note->editing) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans 12");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, note->text, -1);
    pango_layout_set_width(layout, (note->width - 10) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    if (text_height <= note->height - 10) {
      cairo_move_to(cr, note->x + 5, note->y + 5);
      cairo_set_source_rgb(cr, 0, 0, 0);
      pango_cairo_show_layout(cr, layout);
    } else {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_height(layout, (note->height - 10) * PANGO_SCALE);
      cairo_move_to(cr, note->x + 5, note->y + 5);
      cairo_set_source_rgb(cr, 0, 0, 0);
      pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
  }

  cairo_reset_clip(cr);
}

/* Check if note is selected */
static gboolean is_note_selected(CanvasData *data, Note *note) {
  for (GList *l = data->selected_notes; l != NULL; l = l->next) {
    if (l->data == note) {
      return TRUE;
    }
  }
  return FALSE;
}

/* Compare function for sorting notes by z-index */
static gint compare_notes_by_z_index(gconstpointer a, gconstpointer b) {
  const Note *note_a = (const Note*)a;
  const Note *note_b = (const Note*)b;
  return note_a->z_index - note_b->z_index;
}

/* Draw canvas */
static void on_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
  cairo_paint(cr);

  for (GList *l = data->connections; l != NULL; l = l->next) {
    Connection *conn = (Connection*)l->data;
    int x1, y1, x2, y2;
    get_connection_point(conn->from, conn->from_point, &x1, &y1);
    get_connection_point(conn->to, conn->to_point, &x2, &y2);

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 2);

    draw_parallel_arrow(cr, (Vec2){x1, y1}, (Vec2){x2, y2},
                        conn->from_point, conn->to_point);
  }

  GList *sorted_notes = g_list_copy(data->notes);
  sorted_notes = g_list_sort(sorted_notes, compare_notes_by_z_index);

  for (GList *l = sorted_notes; l != NULL; l = l->next) {
    Note *note = (Note*)l->data;
    draw_note(cr, note, is_note_selected(data, note));
  }

  g_list_free(sorted_notes);

  if (data->selecting) {
    cairo_set_source_rgba(cr, 0.5, 0.5, 1.0, 0.3);
    cairo_rectangle(cr,
                    MIN(data->start_x, data->current_x),
                    MIN(data->start_y, data->current_y),
                    ABS(data->current_x - data->start_x),
                    ABS(data->current_y - data->start_y));
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.2, 0.2, 1.0);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
  }
}

/* Resize support */
static int pick_resize_handle(Note *note, int x, int y) {
  int size = 8;
  struct { int px, py; } handles[4] = {
    {note->x, note->y},
    {note->x + note->width, note->y},
    {note->x + note->width, note->y + note->height},
    {note->x, note->y + note->height}
  };

  for (int i = 0; i < 4; i++) {
    if (abs(x - handles[i].px) <= size && abs(y - handles[i].py) <= size) {
      return i;
    }
  }
  return -1;
}

/* Finish editing */
static void finish_edit(GtkWidget *widget, gpointer user_data) {
  Note *note = (Note*)user_data;
  if (!note->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  g_free(note->text);
  note->text = new_text;

  note->editing = FALSE;
  gtk_widget_hide(note->text_view);

  CanvasData *data = g_object_get_data(G_OBJECT(note->text_view), "canvas_data");
  if (data && data->drawing_area) {
    gtk_widget_queue_draw(data->drawing_area);
  }
}

/* Focus leave handler */
static void on_text_view_focus_leave(GtkEventController *controller, gpointer user_data) {
  Note *note = (Note*)user_data;
  finish_edit(NULL, note);
}

/* Handle Enter key in TextView */
static gboolean on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  Note *note = (Note*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) return FALSE;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    finish_edit(widget, note);
    return TRUE;
  }
  return FALSE;
}

/* Pick note at position */
static Note* pick_note(CanvasData *data, int x, int y) {
  Note *selected_note = NULL;
  int highest_z_index = -1;

  for (GList *l = data->notes; l != NULL; l = l->next) {
    Note *note = (Note*)l->data;
    if (x >= note->x && x <= note->x + note->width &&
        y >= note->y && y <= note->y + note->height) {
      if (note->z_index > highest_z_index) {
        selected_note = note;
        highest_z_index = note->z_index;
      }
    }
  }
  return selected_note;
}

/* Pick connection point */
static int pick_connection_point(Note *note, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    get_connection_point(note, i, &cx, &cy);
    int dx = x - cx, dy = y - cy;
    if (dx * dx + dy * dy < 36) return i;
  }
  return -1;
}

/* Bring note to front */
static void bring_note_to_front(CanvasData *data, Note *note) {
  note->z_index = data->next_z_index++;
  gtk_widget_queue_draw(data->drawing_area);
}

/* Clear selection */
static void clear_selection(CanvasData *data) {
  if (data->selected_notes) {
    g_list_free(data->selected_notes);
    data->selected_notes = NULL;
  }
}

/* Mouse press handler */
static void on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  static Note *connection_start = NULL;
  static int connection_start_point = -1;

  GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);
  if (event) {
    data->modifier_state = gdk_event_get_modifier_state(event);
  }

  Note *note = pick_note(data, (int)x, (int)y);

  if (note) {
    int rh = pick_resize_handle(note, (int)x, (int)y);
    if (rh >= 0) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        clear_selection(data);
      }
      if (!is_note_selected(data, note)) {
        data->selected_notes = g_list_append(data->selected_notes, note);
      }

      bring_note_to_front(data, note);
      note->resizing = TRUE;
      note->resize_edge = rh;
      note->resize_start_x = (int)x;
      note->resize_start_y = (int)y;
      note->orig_x = note->x;
      note->orig_y = note->y;
      note->orig_width = note->width;
      note->orig_height = note->height;
      return;
    }

    int cp = pick_connection_point(note, (int)x, (int)y);
    if (cp >= 0) {
      if (!connection_start) {
        connection_start = note;
        connection_start_point = cp;
      } else {
        if (note != connection_start) {
          Connection *conn = g_new(Connection, 1);
          conn->from = connection_start;
          conn->from_point = connection_start_point;
          conn->to = note;
          conn->to_point = cp;
          data->connections = g_list_append(data->connections, conn);
        }
        connection_start = NULL;
        connection_start_point = -1;
      }
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    bring_note_to_front(data, note);

    if (n_press == 2) {
      note->editing = TRUE;

      if (!note->text_view) {
        note->text_view = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(note->text_view), GTK_WRAP_WORD);
        gtk_widget_set_size_request(note->text_view, note->width, note->height);

        gtk_overlay_add_overlay(GTK_OVERLAY(data->overlay), note->text_view);
        gtk_widget_set_halign(note->text_view, GTK_ALIGN_START);
        gtk_widget_set_valign(note->text_view, GTK_ALIGN_START);
        gtk_widget_set_margin_start(note->text_view, note->x);
        gtk_widget_set_margin_top(note->text_view, note->y);

        g_object_set_data(G_OBJECT(note->text_view), "canvas_data", data);

        GtkEventController *focus_controller = gtk_event_controller_focus_new();
        g_signal_connect(focus_controller, "leave", G_CALLBACK(on_text_view_focus_leave), note);
        gtk_widget_add_controller(note->text_view, focus_controller);

        GtkEventController *key_controller = gtk_event_controller_key_new();
        g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_textview_key_press), note);
        gtk_widget_add_controller(note->text_view, key_controller);
      }

      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(note->text_view));
      gtk_text_buffer_set_text(buffer, note->text, -1);

      gtk_widget_show(note->text_view);
      gtk_widget_grab_focus(note->text_view);
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    if (!note->editing) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        clear_selection(data);
      }
      if (!is_note_selected(data, note)) {
        data->selected_notes = g_list_append(data->selected_notes, note);
      }
      note->dragging = TRUE;
      note->drag_offset_x = (int)x - note->x;
      note->drag_offset_y = (int)y - note->y;
    }
  } else {
    connection_start = NULL;
    connection_start_point = -1;

    if (!(data->modifier_state & GDK_SHIFT_MASK)) {
      clear_selection(data);
    }

    data->selecting = TRUE;
    data->start_x = (int)x;
    data->start_y = (int)y;
    data->current_x = (int)x;
    data->current_y = (int)y;
  }

  gtk_widget_queue_draw(data->drawing_area);
}

/* Mouse motion handler */
static void on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  update_cursor(data, (int)x, (int)y);

  for (GList *l = data->notes; l != NULL; l = l->next) {
    Note *note = (Note*)l->data;

    if (note->resizing) {
      int dx = (int)x - note->resize_start_x;
      int dy = (int)y - note->resize_start_y;
      switch (note->resize_edge) {
      case 0:
        note->x = note->orig_x + dx;
        note->y = note->orig_y + dy;
        note->width = note->orig_width - dx;
        note->height = note->orig_height - dy;
        break;
      case 1:
        note->y = note->orig_y + dy;
        note->width = note->orig_width + dx;
        note->height = note->orig_height - dy;
        break;
      case 2:
        note->width = note->orig_width + dx;
        note->height = note->orig_height + dy;
        break;
      case 3:
        note->x = note->orig_x + dx;
        note->width = note->orig_width - dx;
        note->height = note->orig_height + dy;
        break;
      }
      if (note->width < 50) note->width = 50;
      if (note->height < 30) note->height = 30;

      if (note->text_view) {
        gtk_widget_set_margin_start(note->text_view, note->x);
        gtk_widget_set_margin_top(note->text_view, note->y);
        gtk_widget_set_size_request(note->text_view, note->width, note->height);
      }
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    if (note->dragging) {
      int dx = (int)x - note->x - note->drag_offset_x;
      int dy = (int)y - note->y - note->drag_offset_y;

      for (GList *sel = data->selected_notes; sel != NULL; sel = sel->next) {
        Note *selected_note = (Note*)sel->data;
        selected_note->x += dx;
        selected_note->y += dy;

        if (selected_note->text_view) {
          gtk_widget_set_margin_start(selected_note->text_view, selected_note->x);
          gtk_widget_set_margin_top(selected_note->text_view, selected_note->y);
        }
      }

      gtk_widget_queue_draw(data->drawing_area);
      return;
    }
  }

  if (data->selecting) {
    data->current_x = (int)x;
    data->current_y = (int)y;
    gtk_widget_queue_draw(data->drawing_area);
  }
}

/* Mouse release handler */
static void on_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (data->selecting) {
    data->selecting = FALSE;
    int sel_x = MIN(data->start_x, data->current_x);
    int sel_y = MIN(data->start_y, data->current_y);
    int sel_width = ABS(data->current_x - data->start_x);
    int sel_height = ABS(data->current_y - data->start_y);

    for (GList *iter = data->notes; iter != NULL; iter = iter->next) {
      Note *note = (Note*)iter->data;
      if (note->x + note->width >= sel_x &&
          note->x <= sel_x + sel_width &&
          note->y + note->height >= sel_y &&
          note->y <= sel_y + sel_height) {
        if (!is_note_selected(data, note)) {
          data->selected_notes = g_list_append(data->selected_notes, note);
        }
      }
    }
  }

  for (GList *l = data->notes; l != NULL; l = l->next) {
    Note *note = (Note*)l->data;
    note->dragging = FALSE;
    note->resizing = FALSE;
  }

  gtk_widget_queue_draw(data->drawing_area);
}

/* Mouse leave handler */
static void on_leave(GtkEventControllerMotion *controller, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  set_cursor(data, data->default_cursor);
}

/* Add a new note */
static void on_add_note(GtkButton *button, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  Note *note = g_new0(Note, 1);
  note->x = 50;
  note->y = 50;
  note->width = 200;
  note->height = 150;
  note->text = g_strdup("Note text");
  note->text_view = NULL;
  note->editing = FALSE;
  note->dragging = FALSE;
  note->resizing = FALSE;
  note->z_index = data->next_z_index++;

  data->notes = g_list_append(data->notes, note);
  gtk_widget_queue_draw(data->drawing_area);
}

/* Cleanup on app shutdown */
static void on_app_shutdown(GApplication *app, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(app), "canvas_data");
  if (data) {
    if (data->default_cursor) g_object_unref(data->default_cursor);
    if (data->move_cursor) g_object_unref(data->move_cursor);
    if (data->resize_cursor) g_object_unref(data->resize_cursor);
    if (data->connect_cursor) g_object_unref(data->connect_cursor);

    for (GList *l = data->notes; l != NULL; l = l->next) {
      Note *note = (Note*)l->data;
      if (note->text) g_free(note->text);
      if (note->text_view && GTK_IS_WIDGET(note->text_view) && gtk_widget_get_parent(note->text_view)) {
        gtk_widget_unparent(note->text_view);
      }
      g_free(note);
    }
    g_list_free(data->notes);

    for (GList *l = data->connections; l != NULL; l = l->next) {
      g_free(l->data);
    }
    g_list_free(data->connections);

    g_list_free(data->selected_notes);
    g_free(data);
    g_object_set_data(G_OBJECT(app), "canvas_data", NULL);
  }
}

/* App activation */
static void on_activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(window), "velo2");

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child(GTK_WINDOW(window), vbox);

  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_append(GTK_BOX(vbox), toolbar);

  GtkWidget *add_btn = gtk_button_new_with_label("New Note");
  gtk_box_append(GTK_BOX(toolbar), add_btn);

  GtkWidget *overlay = gtk_overlay_new();
  gtk_widget_set_hexpand(overlay, TRUE);
  gtk_widget_set_vexpand(overlay, TRUE);
  gtk_box_append(GTK_BOX(vbox), overlay);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_widget_set_hexpand(drawing_area, TRUE);
  gtk_widget_set_vexpand(drawing_area, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);

  CanvasData *data = g_new0(CanvasData, 1);
  data->notes = NULL;
  data->connections = NULL;
  data->selected_notes = NULL;
  data->drawing_area = drawing_area;
  data->overlay = overlay;
  data->next_z_index = 1;
  data->selecting = FALSE;
  data->modifier_state = 0;

  data->default_cursor = gdk_cursor_new_from_name("default", NULL);
  data->move_cursor = gdk_cursor_new_from_name("move", NULL);
  data->resize_cursor = gdk_cursor_new_from_name("nwse-resize", NULL);
  data->connect_cursor = gdk_cursor_new_from_name("crosshair", NULL);
  data->current_cursor = NULL;

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), on_draw, data, NULL);

  GtkGesture *click_controller = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_controller), GDK_BUTTON_PRIMARY);
  g_signal_connect(click_controller, "pressed", G_CALLBACK(on_button_press), data);
  g_signal_connect(click_controller, "released", G_CALLBACK(on_release), data);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(click_controller));

  GtkEventController *motion_controller = gtk_event_controller_motion_new();
  g_signal_connect(motion_controller, "motion", G_CALLBACK(on_motion), data);
  g_signal_connect(motion_controller, "leave", G_CALLBACK(on_leave), data);
  gtk_widget_add_controller(drawing_area, motion_controller);

  g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_note), data);
  g_object_set_data(G_OBJECT(app), "canvas_data", data);
  gtk_window_present(GTK_WINDOW(window));
}

/* Main function */
int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("com.example.notecanvas", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
