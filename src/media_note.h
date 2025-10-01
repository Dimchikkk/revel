#ifndef MEDIA_FILE_H
#define MEDIA_FILE_H

#include "element.h"
#include <gst/gst.h>

typedef struct _CanvasData CanvasData;

typedef struct {
  Element base;
  MediaType media_type;
  GdkPixbuf *pixbuf;
  char *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;
  char* alignment;
  GtkWidget *text_view;
  gboolean editing;

  // Video specific fields
  GstElement *video_pipeline;
  gboolean video_playing;
  GtkWidget *video_widget;
  unsigned char *video_data;  // Store video data in memory
  int video_size;
  gint duration;

  gboolean reset_video_data;
} MediaNote;

MediaNote* media_note_create(ElementPosition position,
                             ElementColor bg_color,
                             ElementSize size,
                             ElementMedia media,
                             ElementText text,
                             CanvasData *data);
void media_note_finish_editing(Element *element);
void media_note_draw(Element *element, cairo_t *cr, gboolean is_selected);
void media_note_get_connection_point(Element *element, int point, int *cx, int *cy);
int media_note_pick_resize_handle(Element *element, int x, int y);
int media_note_pick_connection_point(Element *element, int x, int y);
void media_note_start_editing(Element *element, GtkWidget *overlay);
void media_note_update_position(Element *element, int x, int y, int z);
void media_note_update_size(Element *element, int width, int height);
void media_note_free(Element *element);
void media_note_toggle_video_playback(Element *element);

#endif
