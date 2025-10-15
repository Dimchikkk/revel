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
  gboolean strikethrough;
  char* alignment;
  GtkWidget *text_view;
  gboolean editing;

  // Media specific fields
  GstElement *media_pipeline;
  gboolean media_playing;
  GtkWidget *media_widget;
  guint bus_watch_id;
  unsigned char *media_data;  // Store media data in memory
  int media_size;
  gint duration;
  gboolean reset_media_data;
  gboolean has_thumbnail;

  // Fields for data feeding
  const guint8 *current_pos;
  guint remaining;
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
void media_note_toggle_audio_playback(Element *element);
void media_note_get_visible_bounds(MediaNote *media_note,
                                   int *out_x,
                                   int *out_y,
                                   int *out_width,
                                   int *out_height);

#endif
