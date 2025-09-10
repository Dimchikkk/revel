#ifndef IMAGE_NOTE_H
#define IMAGE_NOTE_H

#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
  Element base;
  GdkPixbuf *pixbuf;
  char *text;
  GtkWidget *text_view;
  gboolean editing;
} ImageNote;


ImageNote* image_note_create(int x, int y, int z, int width, int height,
                             const unsigned char *image_data, int image_size,
                             const char *text, CanvasData *data);
void image_note_finish_editing(Element *element);
void image_note_draw(Element *element, cairo_t *cr, gboolean is_selected);
void image_note_get_connection_point(Element *element, int point, int *cx, int *cy);
int image_note_pick_resize_handle(Element *element, int x, int y);
int image_note_pick_connection_point(Element *element, int x, int y);
void image_note_start_editing(Element *element, GtkWidget *overlay);
void image_note_update_position(Element *element, int x, int y, int z);
void image_note_update_size(Element *element, int width, int height);
void image_note_free(Element *element);

#endif
