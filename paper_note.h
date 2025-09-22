#ifndef PAPER_NOTE_H
#define PAPER_NOTE_H

#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  Element base;
  char *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  gboolean editing;
} PaperNote;

PaperNote* paper_note_create(ElementPosition position,
                             ElementColor bg_color,
                             ElementSize size,
                             ElementText text,
                             CanvasData *data);
void paper_note_draw(Element *element, cairo_t *cr, gboolean is_selected);
void paper_note_get_connection_point(Element *element, int point, int *cx, int *cy);
int paper_note_pick_resize_handle(Element *element, int x, int y);
int paper_note_pick_connection_point(Element *element, int x, int y);
void paper_note_start_editing(Element *element, GtkWidget *overlay);
void paper_note_finish_editing(Element *element);
void paper_note_update_position(Element *element, int x, int y, int z);
void paper_note_update_size(Element *element, int width, int height);
void paper_note_free(Element *element);

#endif
