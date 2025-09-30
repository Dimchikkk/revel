#include "media_note.h"
#include "canvas.h"
#include "canvas_core.h"
#include "element.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>
#include <math.h>
#include <glib.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "undo_manager.h"

gboolean gst_initialized = FALSE;

static ElementVTable media_note_vtable = {
  .draw = media_note_draw,
  .get_connection_point = media_note_get_connection_point,
  .pick_resize_handle = media_note_pick_resize_handle,
  .pick_connection_point = media_note_pick_connection_point,
  .start_editing = media_note_start_editing,
  .update_position = media_note_update_position,
  .update_size = media_note_update_size,
  .free = media_note_free
};


static void need_data_callback(GstElement *appsrc, guint size, gpointer user_data) {
  MediaNote *media_note = (MediaNote*)user_data;
  static guint8 *current_pos = NULL;
  static guint remaining = 0;

  // Initialize on first call or if reset flag is set
  if (current_pos == NULL || media_note->reset_video_data) {
    current_pos = media_note->video_data;
    remaining = media_note->video_size;
    media_note->reset_video_data = FALSE; // Reset the flag
  }

  if (remaining == 0) {
    // End of data - provide proper return value location
    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "end-of-stream", &ret);

    if (ret != GST_FLOW_OK) {
      g_printerr("End-of-stream failed: %d\n", ret);
    }

    current_pos = NULL; // Reset for next playback
    return;
  }

  guint chunk_size = MIN(size, remaining);
  GstBuffer *buffer = gst_buffer_new_allocate(NULL, chunk_size, NULL);
  GstMapInfo map;

  if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
    memcpy(map.data, current_pos, chunk_size);
    gst_buffer_unmap(buffer, &map);

    current_pos += chunk_size;
    remaining -= chunk_size;

    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    if (ret != GST_FLOW_OK) {
      g_printerr("Failed to push buffer: %d\n", ret);
      // Reset position on error
      current_pos = NULL;
      remaining = 0;
    }
  }

  gst_buffer_unref(buffer);
}

// Video bus callback to handle messages
// Video bus callback to handle messages
static gboolean video_bus_callback(GstBus *bus, GstMessage *msg, gpointer user_data) {
  MediaNote *media_note = (MediaNote*)user_data;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    // End of stream - normal termination
    g_print("Video playback finished\n");
    break;

  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);

    // Check if it's a window closure error (common with autovideosink)
    if (g_strrstr(err->message, "Output window was closed") ||
        g_strrstr(err->message, "window close")) {
    } else {
      g_printerr("Video error: %s\n", err->message);
      if (debug_info) {
        g_printerr("Debug info: %s\n", debug_info);
      }
    }

    g_error_free(err);
    g_free(debug_info);
    break;
  }

  case GST_MESSAGE_WARNING: {
    GError *err;
    gchar *debug_info;
    gst_message_parse_warning(msg, &err, &debug_info);

    // Suppress common warnings about window closure
    if (g_strrstr(err->message, "Output window was closed") ||
        g_strrstr(err->message, "window close")) {
      // Just ignore these warnings
    } else {
      g_printerr("Video warning: %s\n", err->message);
    }

    g_error_free(err);
    g_free(debug_info);
    break;
  }

  default:
    break;
  }

  // Always clean up pipeline for EOS or error
  if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS || GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
    // Stop and free pipeline
    if (media_note->video_pipeline) {
      gst_element_set_state(media_note->video_pipeline, GST_STATE_NULL);
      gst_object_unref(media_note->video_pipeline);
      media_note->video_pipeline = NULL;
    }

    // Reset playback flag
    media_note->video_playing = FALSE;

    // Remove video widget from overlay
    if (media_note->video_widget) {
      gtk_widget_unparent(media_note->video_widget);
      media_note->video_widget = NULL;
    }

    // Redraw the canvas
    if (media_note->base.canvas_data && media_note->base.canvas_data->drawing_area) {
      gtk_widget_queue_draw(GTK_WIDGET(media_note->base.canvas_data->drawing_area));
    }
  }

  return TRUE; // keep watching bus
}

MediaNote* media_note_create(ElementPosition position,
                             ElementColor bg_color,
                             ElementSize size,
                             ElementMedia media,
                             ElementText text,
                             CanvasData *data) {
  MediaNote *media_note = g_new0(MediaNote, 1);
  media_note->base.type = ELEMENT_MEDIA_FILE;
  media_note->base.vtable = &media_note_vtable;
  media_note->base.x = position.x;
  media_note->base.y = position.y;
  media_note->base.z = position.z;

  media_note->base.bg_r = bg_color.r;
  media_note->base.bg_g = bg_color.g;
  media_note->base.bg_b = bg_color.b;
  media_note->base.bg_a = bg_color.a;
  media_note->duration = media.duration;

  media_note->base.width = size.width;
  media_note->base.height = size.height;
  media_note->base.canvas_data = data;
  media_note->text = g_strdup(text.text ? text.text : "");
  media_note->text_view = NULL;
  media_note->editing = FALSE;
  media_note->media_type = media.type;
  media_note->video_playing = FALSE;
  media_note->video_pipeline = NULL;
  media_note->video_widget = NULL;

  media_note->text_r = text.text_color.r;
  media_note->text_g = text.text_color.g;
  media_note->text_b = text.text_color.b;
  media_note->text_a = text.text_color.a;
  media_note->font_description = g_strdup(text.font_description);


  media_note->reset_video_data = FALSE;

  // Always try to create pixbuf from image_data (this is the thumbnail)
  if (media.image_data && media.image_size > 0) {
    GInputStream *stream = g_memory_input_stream_new_from_data(media.image_data, media.image_size, NULL);
    media_note->pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    g_object_unref(stream);
  } else {
    // Fallback: create a placeholder
    media_note->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 100, 100);
    gdk_pixbuf_fill(media_note->pixbuf, 0x303030FF);
  }

  // Store video data for playback
  if (media.type == MEDIA_TYPE_VIDEO && media.video_data && media.video_size > 0) {
    media_note->video_data = g_malloc(media.video_size);
    memcpy(media_note->video_data, media.video_data, media.video_size);
    media_note->video_size = media.video_size;
    media_note->reset_video_data = TRUE;
  }

  return media_note;
}

// Add this helper function
static gboolean return_focus_to_main(GtkWindow *main_window) {
  gtk_window_present(main_window);
  return G_SOURCE_REMOVE; // Remove the timeout after executing
}


void media_note_toggle_video_playback(Element *element) {
  MediaNote *media_note = (MediaNote*)element;

  if (media_note->media_type != MEDIA_TYPE_VIDEO) {
    return;
  }

  if (!gst_initialized) {
    GError *error = NULL;
    if (!gst_init_check(NULL, NULL, &error)) {
      g_printerr("Failed to initialize GStreamer: %s\n", error->message);
      g_error_free(error);
      return;
    }
    gst_initialized = TRUE;
  }

  ModelElement *model_element = model_get_by_visual(media_note->base.canvas_data->model, element);
  if (!model_element || !model_element->video) {
    return;
  }

  if (!model_element->video->is_loaded) {
    if (!model_load_video_data(media_note->base.canvas_data->model, model_element->video)) {
      g_printerr("Failed to load video data\n");
      return;
    }

    media_note->video_data = g_malloc(model_element->video->video_size);
    memcpy(media_note->video_data, model_element->video->video_data, model_element->video->video_size);
    media_note->video_size = model_element->video->video_size;
  }

  // CREATE PIPELINE ON FIRST PLAY
  if (!media_note->video_pipeline) {
    GError *error = NULL;
    media_note->video_pipeline = gst_parse_launch(
                                                  "appsrc name=source is-live=true format=time ! "
                                                  "queue ! "
                                                  "qtdemux name=demux "
                                                  "demux.video_0 ! queue ! decodebin ! videoconvert ! autovideosink name=sink "
                                                  "demux.audio_0 ! queue ! decodebin ! audioconvert ! autoaudiosink",
                                                  &error
                                                  );

    if (error) {
      g_printerr("Failed to create video pipeline: %s\n", error->message);
      g_error_free(error);
      return;
    }

    // Configure appsrc
    GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->video_pipeline), "source");
    if (appsrc) {
      GstCaps *caps = gst_caps_new_simple("video/quicktime",
                                          "variant", G_TYPE_STRING, "iso",
                                          NULL);
      g_object_set(appsrc,
                   "caps", caps,
                   "block", TRUE,
                   "stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
                   "format", GST_FORMAT_TIME,
                   "do-timestamp", TRUE,
                   NULL);
      gst_caps_unref(caps);

      g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_callback), media_note);
      gst_object_unref(appsrc);
    }

    // Set up bus callback
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(media_note->video_pipeline));
    gst_bus_add_watch(bus, video_bus_callback, media_note);
    gst_object_unref(bus);

    // Create video widget
    media_note->video_widget = gtk_drawing_area_new();
    gtk_widget_set_size_request(media_note->video_widget,
                                media_note->base.width,
                                media_note->base.height);

    // Make widget ignore input events so clicks go through
    gtk_widget_set_sensitive(media_note->video_widget, FALSE);
    gtk_widget_set_can_focus(media_note->video_widget, FALSE);
    gtk_widget_set_focusable(media_note->video_widget, FALSE);

    // Add to overlay
    gtk_overlay_add_overlay(GTK_OVERLAY(media_note->base.canvas_data->overlay),
                            media_note->video_widget);

    // Position video widget
    int screen_x, screen_y;
    canvas_canvas_to_screen(media_note->base.canvas_data,
                            media_note->base.x,
                            media_note->base.y,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->video_widget, screen_x);
    gtk_widget_set_margin_top(media_note->video_widget, screen_y);
  }

  if (media_note->video_playing) {
    // Pause playback
    gst_element_set_state(media_note->video_pipeline, GST_STATE_PAUSED);
    media_note->video_playing = FALSE;

    if (media_note->video_widget) {
      gtk_widget_hide(media_note->video_widget);
    }
  } else {
    // If pipeline was in NULL state (after EOS), we need to reset it
    GstState state;
    gst_element_get_state(media_note->video_pipeline, &state, NULL, 0);

    if (state == GST_STATE_NULL) {
      // Reset the pipeline to READY state first
      gst_element_set_state(media_note->video_pipeline, GST_STATE_READY);

      // Reset appsrc to prepare for new data
      GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->video_pipeline), "source");
      if (appsrc) {
        // Reset appsrc properties
        g_object_set(appsrc, "block", TRUE, NULL);
        gst_object_unref(appsrc);
      }
    }

    // Start playback from beginning
    media_note->reset_video_data = TRUE;
    gst_element_set_state(media_note->video_pipeline, GST_STATE_PLAYING);
    media_note->video_playing = TRUE;

    if (media_note->video_widget) {
      gtk_widget_show(media_note->video_widget);

      // Return focus to main window after showing video
      GtkWindow *main_window = GTK_WINDOW(
                                          gtk_widget_get_ancestor(media_note->base.canvas_data->drawing_area, GTK_TYPE_WINDOW)
                                          );
      if (main_window) {
        g_timeout_add(100, (GSourceFunc)return_focus_to_main, main_window);
      }
    }
  }

  // Redraw main canvas
  gtk_widget_queue_draw(GTK_WIDGET(media_note->base.canvas_data->drawing_area));
}

gboolean media_note_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  MediaNote *media_note = (MediaNote*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) {
      // Enter finishes editing
      GtkTextView *text_view = GTK_TEXT_VIEW(media_note->text_view);
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_insert(buffer, &iter, "\n", 1);

      return TRUE; // Handled - prevent default behavior
    } else {
      // Ctrl+Enter inserts a newline
      media_note_finish_editing((Element*)media_note);
      return TRUE;
    }
  }
  return FALSE;
}

void media_note_finish_editing(Element *element) {
  MediaNote *media_note = (MediaNote*)element;
  if (!media_note->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(media_note->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  char* old_text = g_strdup(media_note->text);
  g_free(media_note->text);
  media_note->text = new_text;

  Model* model = media_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  undo_manager_push_text_action(media_note->base.canvas_data->undo_manager, model_element, old_text, new_text);
  model_update_text(model, model_element, new_text);

  media_note->editing = FALSE;
  gtk_widget_hide(media_note->text_view);

  // Queue redraw using the stored canvas data
  if (media_note->base.canvas_data && media_note->base.canvas_data->drawing_area) {
    canvas_sync_with_model(media_note->base.canvas_data);
    gtk_widget_queue_draw(media_note->base.canvas_data->drawing_area);
    gtk_widget_grab_focus(media_note->base.canvas_data->drawing_area);
  }
}

void media_note_start_editing(Element *element, GtkWidget *overlay) {
  MediaNote *media_note = (MediaNote*)element;

  // Don't allow editing while video is playing
  if (media_note->media_type == MEDIA_TYPE_VIDEO && media_note->video_playing) {
    return;
  }

  media_note->editing = TRUE;

  if (!media_note->text_view) {
    media_note->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(media_note->text_view), GTK_WRAP_WORD);

    // Make text view smaller for the bottom-right corner
    gtk_widget_set_size_request(media_note->text_view,
                                element->width / 3,
                                element->height / 6);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), media_note->text_view);
    gtk_widget_set_halign(media_note->text_view, GTK_ALIGN_START);
    gtk_widget_set_valign(media_note->text_view, GTK_ALIGN_START);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(media_note_on_textview_key_press), media_note);
    gtk_widget_add_controller(media_note->text_view, key_controller);
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(media_note->text_view));
  gtk_text_buffer_set_text(buffer, media_note->text, -1);

  // Position the text view in the bottom right corner of the actual image
  if (media_note->pixbuf) {
    int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
    int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

    double scale_x = element->width / (double)pixbuf_width;
    double scale_y = element->height / (double)pixbuf_height;
    double scale = MIN(scale_x, scale_y);

    int draw_width = (int) pixbuf_width * scale;
    int draw_height = (int) pixbuf_height * scale;
    int draw_x = element->x + (element->width - draw_width) / 2;
    int draw_y = element->y + (element->height - draw_height) / 2;

    int text_view_width, text_view_height;
    gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data,
                            draw_x + draw_width - text_view_width - 10,
                            draw_y + draw_height - text_view_height - 10,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->text_view, screen_x);
    gtk_widget_set_margin_top(media_note->text_view, screen_y);
  } else {
    // Fallback to element bounds if no pixbuf
    int text_view_width, text_view_height;
    gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data,
                            element->x + element->width - text_view_width - 10,
                            element->y + element->height - text_view_height - 10,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->text_view, screen_x);
    gtk_widget_set_margin_top(media_note->text_view, screen_y);
  }

  gtk_widget_show(media_note->text_view);
  gtk_widget_grab_focus(media_note->text_view);
}

void media_note_update_position(Element *element, int x, int y, int z) {
  MediaNote *media_note = (MediaNote*)element;
  element->x = x;
  element->y = y;
  element->z = z;

  // Update text view position if editing
  if (media_note->text_view && media_note->editing) {
    if (media_note->pixbuf) {
      int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
      int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

      double scale_x = element->width / (double)pixbuf_width;
      double scale_y = element->height / (double)pixbuf_height;
      double scale = MIN(scale_x, scale_y);

      int draw_width = pixbuf_width * scale;
      int draw_height = pixbuf_height * scale;
      int draw_x = element->x + (element->width - draw_width) / 2;
      int draw_y = element->y + (element->height - draw_height) / 2;

      int text_view_width, text_view_height;
      gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

      int screen_draw_x, screen_draw_y;
      canvas_canvas_to_screen(element->canvas_data, draw_x, draw_y, &screen_draw_x, &screen_draw_y);

      gtk_widget_set_margin_start(media_note->text_view,
                                  screen_draw_x + draw_width - text_view_width - 10);
      gtk_widget_set_margin_top(media_note->text_view,
                                screen_draw_y + draw_height - text_view_height - 10);
    } else {
      // Fallback to element bounds if no pixbuf
      int text_view_width, text_view_height;
      gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

      int screen_x, screen_y;
      canvas_canvas_to_screen(element->canvas_data,
                              element->x + element->width - text_view_width - 10,
                              element->y + element->height - text_view_height - 10,
                              &screen_x, &screen_y);

      gtk_widget_set_margin_start(media_note->text_view, screen_x);
      gtk_widget_set_margin_top(media_note->text_view, screen_y);
    }
  }

  // Update video widget position if it exists
  if (media_note->video_widget) {
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, x, y, &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->video_widget, screen_x);
    gtk_widget_set_margin_top(media_note->video_widget, screen_y);
  }

  // Update model
  Model* model = media_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_position(model, model_element, x, y, z);
}

void media_note_update_size(Element *element, int width, int height) {
  MediaNote *media_note = (MediaNote*)element;
  element->width = width;
  element->height = height;

  if (media_note->text_view) {
    // Resize text view proportionally
    gtk_widget_set_size_request(media_note->text_view, width / 3, height / 6);

    // Reposition text view if editing
    if (media_note->editing) {
      if (media_note->pixbuf) {
        int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
        int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

        double scale_x = element->width / (double)pixbuf_width;
        double scale_y = element->height / (double)pixbuf_height;
        double scale = MIN(scale_x, scale_y);

        int draw_width = pixbuf_width * scale;
        int draw_height = pixbuf_height * scale;
        int draw_x = element->x + (element->width - draw_width) / 2;
        int draw_y = element->y + (element->height - draw_height) / 2;

        int text_view_width, text_view_height;
        gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

        gtk_widget_set_margin_start(media_note->text_view, draw_x + draw_width - text_view_width - 10);
        gtk_widget_set_margin_top(media_note->text_view, draw_y + draw_height - text_view_height - 10);
      } else {
        // Fallback to element bounds if no pixbuf
        int text_view_width, text_view_height;
        gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

        gtk_widget_set_margin_start(media_note->text_view, element->x + element->width - text_view_width - 10);
        gtk_widget_set_margin_top(media_note->text_view, element->y + element->height - text_view_height - 10);
      }
    }
  }

  // Update video widget size if it exists
  if (media_note->video_widget) {
    gtk_widget_set_size_request(media_note->video_widget, width, height);
  }

  // Update model
  Model* model = media_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_size(model, model_element, width, height);
}

void media_note_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  MediaNote *media_note = (MediaNote*)element;

  // Always draw the element, even when video is playing
  if (!media_note->pixbuf) return;

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw the image scaled to fit the entire element
  int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

  // Calculate scaling to fit the entire element
  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  // Save the current state
  cairo_save(cr);

  // Create a rectangle for the image and clip to it
  cairo_rectangle(cr, draw_x, draw_y, draw_width, draw_height);
  cairo_clip(cr);

  // Translate to the draw position
  cairo_translate(cr, draw_x, draw_y);

  // Scale the image
  cairo_scale(cr, scale, scale);

  // Draw the pixbuf at the origin (since we already translated)
  gdk_cairo_set_source_pixbuf(cr, media_note->pixbuf, 0, 0);

  // If video is playing, draw with some transparency
  if (media_note->media_type == MEDIA_TYPE_VIDEO && media_note->video_playing) {
    cairo_paint_with_alpha(cr, 0.3); // Semi-transparent when video is playing
  } else {
    cairo_paint(cr); // Opaque when not playing
  }

  // Restore the state (back to original coordinate system)
  cairo_restore(cr);

  // Draw play icon for video elements
  if (media_note->media_type == MEDIA_TYPE_VIDEO) {
    cairo_save(cr);

    if (media_note->video_playing) {
      // Draw pause icon when video is playing
      cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
      cairo_arc(cr, element->x + element->width/2, element->y + element->height/2,
                MIN(element->width, element->height)/4, 0, 2 * G_PI);
      cairo_fill(cr);

      cairo_set_source_rgb(cr, 1, 1, 1);
      // Draw two vertical bars for pause icon
      cairo_rectangle(cr, element->x + element->width/2 - 12, element->y + element->height/2 - 15, 6, 30);
      cairo_rectangle(cr, element->x + element->width/2 + 6, element->y + element->height/2 - 15, 6, 30);
      cairo_fill(cr);
    } else {
      // Draw play icon when video is not playing
      cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
      cairo_arc(cr, element->x + element->width/2, element->y + element->height/2,
                MIN(element->width, element->height)/4, 0, 2 * G_PI);
      cairo_fill(cr);

      cairo_set_source_rgb(cr, 1, 1, 1);
      cairo_move_to(cr, element->x + element->width/2 - 10, element->y + element->height/2 - 15);
      cairo_line_to(cr, element->x + element->width/2 - 10, element->y + element->height/2 + 15);
      cairo_line_to(cr, element->x + element->width/2 + 15, element->y + element->height/2);
      cairo_close_path(cr);
      cairo_fill(cr);
    }
    cairo_restore(cr);
  }

  // Draw text or duration (if not editing and not video playing)
  if (!media_note->editing &&
      !(media_note->media_type == MEDIA_TYPE_VIDEO && media_note->video_playing)) {
    cairo_save(cr);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string(media_note->font_description);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    char display_text[64] = {0};

    if (media_note->media_type == MEDIA_TYPE_VIDEO && media_note->duration > 0) {
      // Show duration for videos
      gint seconds = media_note->duration;
      gint minutes = seconds / 60;
      seconds = seconds % 60;
      snprintf(display_text, sizeof(display_text), "%02d:%02d", minutes, seconds);
    } else if (media_note->text && media_note->text[0] != '\0') {
      // Show text for images or videos without duration
      snprintf(display_text, sizeof(display_text), "%s", media_note->text);
    }

    // Only draw if we have something to show
    if (display_text[0] != '\0') {
      pango_layout_set_text(layout, display_text, -1);
      pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);

      int text_width, text_height;
      pango_layout_get_pixel_size(layout, &text_width, &text_height);

      int text_x = draw_x + draw_width - text_width - 8;
      int text_y = draw_y + draw_height - text_height - 8;

      // Draw background rectangle for better text visibility
      cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
      cairo_rectangle(cr, text_x - 4, text_y - 2, text_width + 8, text_height + 4);
      cairo_fill(cr);

      // Draw white text
      cairo_set_source_rgba(cr, media_note->text_r, media_note->text_g, media_note->text_b, media_note->text_a);
      cairo_move_to(cr, text_x, text_y);
      pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
    cairo_restore(cr);
  }

  // Restore cairo state before drawing selection UI
  cairo_restore(cr);

  // Draw resize handles and connection points (only when selected)
  if (is_selected) {
    // Draw resize handles at the corners of the actual image (without rotation)
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.8);
    cairo_set_line_width(cr, 2.0);

    int handles[4][2] = {
      {draw_x, draw_y},
      {draw_x + draw_width, draw_y},
      {draw_x + draw_width, draw_y + draw_height},
      {draw_x, draw_y + draw_height}
    };

    for (int i = 0; i < 4; i++) {
      cairo_rectangle(cr, handles[i][0] - 4, handles[i][1] - 4, 8, 8);
      cairo_fill(cr);
    }

    // Draw connection points on the actual image borders (with rotation)
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      media_note_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
      cairo_fill(cr);
    }

    // Draw rotation handle (without rotation)
    element_draw_rotation_handle(element, cr);
  }
}

void media_note_get_connection_point(Element *element, int point, int *cx, int *cy) {
  MediaNote *media_note = (MediaNote*)element;
  int unrotated_x, unrotated_y;

  if (!media_note->pixbuf) {
    // Fallback to element bounds if no pixbuf
    switch(point) {
    case 0: unrotated_x = element->x + element->width/2; unrotated_y = element->y; break;
    case 1: unrotated_x = element->x + element->width; unrotated_y = element->y + element->height/2; break;
    case 2: unrotated_x = element->x + element->width/2; unrotated_y = element->y + element->height; break;
    case 3: unrotated_x = element->x; unrotated_y = element->y + element->height/2; break;
    }
  } else {
    // Calculate actual image position and size
    int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
    int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

    double scale_x = element->width / (double)pixbuf_width;
    double scale_y = element->height / (double)pixbuf_height;
    double scale = MIN(scale_x, scale_y);

    int draw_width = pixbuf_width * scale;
    int draw_height = pixbuf_height * scale;
    int draw_x = element->x + (element->width - draw_width) / 2;
    int draw_y = element->y + (element->height - draw_height) / 2;

    // Return connection points on the actual image borders
    switch(point) {
    case 0: unrotated_x = draw_x + draw_width/2; unrotated_y = draw_y; break;                    // top center
    case 1: unrotated_x = draw_x + draw_width; unrotated_y = draw_y + draw_height/2; break;      // right center
    case 2: unrotated_x = draw_x + draw_width/2; unrotated_y = draw_y + draw_height; break;      // bottom center
    case 3: unrotated_x = draw_x; unrotated_y = draw_y + draw_height/2; break;                   // left center
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

int media_note_pick_resize_handle(Element *element, int x, int y) {
  MediaNote *media_note = (MediaNote*)element;

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

  if (!media_note->pixbuf) {
    // Fallback to element bounds if no pixbuf
    int size = 8;
    int handles[4][2] = {
      {element->x, element->y},
      {element->x + element->width, element->y},
      {element->x + element->width, element->y + element->height},
      {element->x, element->y + element->height}
    };

    for (int i = 0; i < 4; i++) {
      if (abs(rotated_cx - handles[i][0]) <= size && abs(rotated_cy - handles[i][1]) <= size) {
        return i;
      }
    }
    return -1;
  }

  // Calculate actual image position and size
  int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
  int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

  double scale_x = element->width / (double)pixbuf_width;
  double scale_y = element->height / (double)pixbuf_height;
  double scale = MIN(scale_x, scale_y);

  int draw_width = pixbuf_width * scale;
  int draw_height = pixbuf_height * scale;
  int draw_x = element->x + (element->width - draw_width) / 2;
  int draw_y = element->y + (element->height - draw_height) / 2;

  int size = 8;
  int handles[4][2] = {
    {draw_x, draw_y},
    {draw_x + draw_width, draw_y},
    {draw_x + draw_width, draw_y + draw_height},
    {draw_x, draw_y + draw_height}
  };

  for (int i = 0; i < 4; i++) {
    if (abs(rotated_cx - handles[i][0]) <= size && abs(rotated_cy - handles[i][1]) <= size) {
      return i;
    }
  }
  return -1;
}

int media_note_pick_connection_point(Element *element, int x, int y) {
  for (int i = 0; i < 4; i++) {
    int px, py;
    media_note_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    if (dx * dx + dy * dy < 100) return i;
  }
  return -1;
}

void media_note_free(Element *element) {
  MediaNote *media_note = (MediaNote*)element;
  if (media_note->pixbuf) g_object_unref(media_note->pixbuf);
  if (media_note->text) g_free(media_note->text);
  if (media_note->font_description) g_free(media_note->font_description);

  // Free video data
  if (media_note->video_data) {
    g_free(media_note->video_data);
    media_note->video_data = NULL;
    media_note->video_size = 0;
  }

  if (media_note->text_view && GTK_IS_WIDGET(media_note->text_view) && gtk_widget_get_parent(media_note->text_view)) {
    gtk_widget_unparent(media_note->text_view);
  }

  if (media_note->video_pipeline) {
    gst_element_set_state(media_note->video_pipeline, GST_STATE_NULL);
    gst_object_unref(media_note->video_pipeline);
    media_note->video_pipeline = NULL;
  }

  if (media_note->video_widget && GTK_IS_WIDGET(media_note->video_widget)) {
    gtk_widget_unparent(media_note->video_widget);
  }

  media_note->video_playing = FALSE;

  g_free(media_note);
}