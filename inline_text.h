#ifndef INLINE_TEXT_H
#define INLINE_TEXT_H

#include "element.h"
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

typedef struct {
  Element base;
  char *text;
  gboolean editing;
  double text_r, text_g, text_b, text_a;
  char *font_description;
  PangoLayout *layout;  // Layout for measuring text
  int min_width;    // Minimum width for the element

  // GTK TextView widgets for editing
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
} InlineText;

// Function declarations
InlineText* inline_text_create(ElementPosition position,
                              ElementColor bg_color,
                              ElementSize size,
                              ElementText text,
                              CanvasData *data);

void inline_text_draw(Element *element, cairo_t *cr, gboolean is_selected);
void inline_text_get_connection_point(Element *element, int point, int *cx, int *cy);
int inline_text_pick_resize_handle(Element *element, int x, int y);
int inline_text_pick_connection_point(Element *element, int x, int y);
void inline_text_start_editing(Element *element, GtkWidget *overlay);
void inline_text_finish_editing(Element *element);
void inline_text_update_position(Element *element, int x, int y, int z);
void inline_text_update_size(Element *element, int width, int height);
void inline_text_free(Element *element);

// Text editing functions
void inline_text_update_layout(InlineText *text);
gboolean inline_text_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

#endif