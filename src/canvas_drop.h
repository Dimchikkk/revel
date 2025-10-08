#ifndef CANVAS_VIDEO_DROP_H
#define CANVAS_VIDEO_DROP_H

#include "canvas_core.h"
#include <gst/gst.h>

void canvas_setup_drop_target(CanvasData *data);
gboolean canvas_on_drop(GtkDropTarget *target, const GValue *value,
                       double x, double y, gpointer user_data);
GstSample* generate_video_thumbnail(const gchar *file_path);
GdkPixbuf* sample_to_pixbuf(GstSample *sample);
gint64 get_mp4_duration(const gchar *file_path);
gint64 get_mp3_duration(const gchar *file_path);
GdkPixbuf* extract_mp3_album_art(const gchar *file_path);

#endif
