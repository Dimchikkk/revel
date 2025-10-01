#ifndef NOTE_H
#define NOTE_H

#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  Element base;
  char *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;
  char* alignment;
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  gboolean editing;
} Note;

Note* note_create(ElementPosition position,
                  ElementColor bg_color,
                  ElementSize size,
                  ElementText text,
                  CanvasData *data);
void note_draw(Element *element, cairo_t *cr, gboolean is_selected);
void note_get_connection_point(Element *element, int point, int *cx, int *cy);
int note_pick_resize_handle(Element *element, int x, int y);
int note_pick_connection_point(Element *element, int x, int y);
void note_start_editing(Element *element, GtkWidget *overlay);
void note_finish_editing(Element *element);
void note_update_position(Element *element, int x, int y, int z);
void note_update_size(Element *element, int width, int height);
void note_free(Element *element);

#endif
