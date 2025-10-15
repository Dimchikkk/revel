#include "media_note.h"
#include "../canvas/canvas.h"
#include "../canvas/canvas_core.h"
#include "element.h"
#include "connection.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>
#include <math.h>
#include <glib.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "../undo_manager.h"

gboolean gst_initialized = FALSE;

static gboolean media_note_playback_state_matches_element(gpointer key,
                                                          gpointer value,
                                                          gpointer user_data) {
  (void)key;
  AudioPlaybackState *state = (AudioPlaybackState*)value;
  Element *element = (Element*)user_data;
  return state && state->element == element;
}

static void media_note_store_playback_state(MediaNote *media_note, gboolean playing) {
  if (!media_note) {
    return;
  }

  CanvasData *canvas_data = media_note->base.canvas_data;
  if (!canvas_data || !canvas_data->audio_playback_states) {
    return;
  }

  Element *element = (Element*)media_note;
  ModelElement *model_element = element->model_element;
  if (!model_element && canvas_data->model) {
    model_element = model_get_by_visual(canvas_data->model, element);
  }

  if (playing) {
    if (!model_element || !model_element->uuid) {
      return;
    }
    AudioPlaybackState *state = g_new0(AudioPlaybackState, 1);
    state->element = element;
    state->playing = TRUE;
    g_hash_table_replace(canvas_data->audio_playback_states,
                         g_strdup(model_element->uuid),
                          state);
  } else {
    gboolean removed = FALSE;
    if (model_element && model_element->uuid) {
      removed = g_hash_table_remove(canvas_data->audio_playback_states,
                                    model_element->uuid);
    }

    if (!removed) {
      g_hash_table_foreach_remove(canvas_data->audio_playback_states,
                                  media_note_playback_state_matches_element,
                                  element);
    }
  }
}

void media_note_get_visible_bounds(MediaNote *media_note,
                                   int *out_x, int *out_y,
                                   int *out_width, int *out_height) {
  Element *element = (Element*)media_note;

  if (media_note->media_type == MEDIA_TYPE_AUDIO && !media_note->has_thumbnail) {
    *out_x = element->x;
    *out_y = element->y;
    *out_width = element->width;
    *out_height = element->height;
    return;
  }

  if (media_note->pixbuf) {
    int pixbuf_width = gdk_pixbuf_get_width(media_note->pixbuf);
    int pixbuf_height = gdk_pixbuf_get_height(media_note->pixbuf);

    double scale_x = element->width / (double)pixbuf_width;
    double scale_y = element->height / (double)pixbuf_height;
    double scale = MIN(scale_x, scale_y);

    *out_width = pixbuf_width * scale;
    *out_height = pixbuf_height * scale;
    *out_x = element->x + (element->width - *out_width) / 2;
    *out_y = element->y + (element->height - *out_height) / 2;
  } else {
    // Fallback to element bounds if no pixbuf
    *out_x = element->x;
    *out_y = element->y;
    *out_width = element->width;
    *out_height = element->height;
  }
}

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

  // Initialize on first call or if reset flag is set
  if (media_note->current_pos == NULL || media_note->reset_media_data) {
    media_note->current_pos = media_note->media_data;
    media_note->remaining = media_note->media_size;
    media_note->reset_media_data = FALSE; // Reset the flag
  }

  if (media_note->remaining == 0) {
    // End of data - provide proper return value location
    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "end-of-stream", &ret);

    if (ret != GST_FLOW_OK) {
      g_printerr("End-of-stream failed: %d\n", ret);
    }

    media_note->current_pos = NULL; // Reset for next playback
    return;
  }

  // Use a larger chunk size for better typefinding, especially for audio
  // Typefind needs at least 4KB, but we'll use 64KB for better performance
  guint chunk_size = MIN(MAX(size, 65536), media_note->remaining);
  GstBuffer *buffer = gst_buffer_new_allocate(NULL, chunk_size, NULL);
  GstMapInfo map;

  if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
    memcpy(map.data, media_note->current_pos, chunk_size);
    gst_buffer_unmap(buffer, &map);

    media_note->current_pos += chunk_size;
    media_note->remaining -= chunk_size;

    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);

    if (ret != GST_FLOW_OK) {
      g_printerr("Failed to push buffer: %d at position %ld, stopping data feed\n",
                 ret, (long)(media_note->media_size - media_note->remaining));
      // Reset position on error
      media_note->current_pos = NULL;
      media_note->remaining = 0;
    }
  }

  gst_buffer_unref(buffer);
}

// Video bus callback to handle messages
static gboolean media_bus_callback(GstBus *bus, GstMessage *msg, gpointer user_data) {
  MediaNote *media_note = (MediaNote*)user_data;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    // End of stream - normal termination
    if (media_note->media_pipeline) {
      gst_element_set_state(media_note->media_pipeline, GST_STATE_PAUSED);
      media_note->media_playing = FALSE;
      media_note_store_playback_state(media_note, FALSE);
    }

    // For audio, try to play next connected audio
    if (media_note->media_type == MEDIA_TYPE_AUDIO && media_note->base.canvas_data) {
      // Get the model element for this audio
      ModelElement *current_model = model_get_by_visual(media_note->base.canvas_data->model,
                                                        (Element*)media_note);

      if (current_model) {
        // Find outgoing connection from this audio element
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, media_note->base.canvas_data->model->elements);

        while (g_hash_table_iter_next(&iter, &key, &value)) {
          ModelElement *elem = (ModelElement*)value;

          // Check if this is a connection from the current audio element
          if (elem->type && elem->type->type == ELEMENT_CONNECTION &&
              elem->from_element_uuid &&
              g_strcmp0(elem->from_element_uuid, current_model->uuid) == 0) {

            // Check arrowhead type - only follow if arrow points forward
            // ARROWHEAD_NONE (0) = no arrowhead, skip
            // ARROWHEAD_SINGLE (1) = arrow points to target, follow
            // ARROWHEAD_DOUBLE (2) = arrows on both ends, follow
            if (elem->arrowhead_type == ARROWHEAD_NONE) {
              continue; // Skip connections with no arrowhead
            }

            // Found a connection from current audio, get the target element
            if (elem->to_element_uuid) {
              ModelElement *next_elem = g_hash_table_lookup(
                media_note->base.canvas_data->model->elements,
                elem->to_element_uuid
              );

              // Check if target is also an audio element
              if (next_elem && next_elem->visual_element &&
                  next_elem->type && next_elem->type->type == ELEMENT_MEDIA_FILE &&
                  next_elem->audio) {

                // Start playing the next audio
                MediaNote *next_audio = (MediaNote*)next_elem->visual_element;
                if (next_audio->media_type == MEDIA_TYPE_AUDIO) {
                  media_note_toggle_audio_playback((Element*)next_audio);
                  break; // Only play the first valid connected audio (based on iteration order)
                }
              }
            }
          }
        }
      }
    }
    break;

  case GST_MESSAGE_ERROR: {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);

    // Check if it's a window closure error (common with autovideosink)
    if (g_strrstr(err->message, "Output window was closed") ||
        g_strrstr(err->message, "window close")) {
    } else {
      g_printerr("Media error: %s\n", err->message);
      if (debug_info) {
        g_printerr("Debug info: %s\n", debug_info);
      }
    }

    media_note->media_playing = FALSE;
    media_note_store_playback_state(media_note, FALSE);

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
      g_printerr("Media warning: %s\n", err->message);
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
    if (media_note->media_pipeline) {
      gst_element_set_state(media_note->media_pipeline, GST_STATE_NULL);
      gst_object_unref(media_note->media_pipeline);
      media_note->media_pipeline = NULL;
    }

    // Reset playback flag
    media_note->media_playing = FALSE;

    // Remove media widget from overlay
    if (media_note->media_widget) {
      gtk_widget_unparent(media_note->media_widget);
      media_note->media_widget = NULL;
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
  media_note->media_playing = FALSE;
  media_note->media_pipeline = NULL;
  media_note->media_widget = NULL;

  media_note->text_r = text.text_color.r;
  media_note->text_g = text.text_color.g;
  media_note->text_b = text.text_color.b;
  media_note->text_a = text.text_color.a;
  media_note->font_description = g_strdup(text.font_description);
  media_note->strikethrough = text.strikethrough;
  media_note->alignment = g_strdup(text.alignment ? text.alignment : "bottom-right");

  media_note->reset_media_data = FALSE;
  media_note->has_thumbnail = FALSE;

  // Always try to create pixbuf from image_data (this is the thumbnail)
  if (media.image_data && media.image_size > 0) {
    GInputStream *stream = g_memory_input_stream_new_from_data(media.image_data, media.image_size, NULL);
    media_note->pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    g_object_unref(stream);
    media_note->has_thumbnail = TRUE;
  } else if (media.type != MEDIA_TYPE_AUDIO) {
    // Fallback: create a placeholder for non-audio media
    media_note->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 100, 100);
    gdk_pixbuf_fill(media_note->pixbuf, 0x303030FF);
  }

  // Store media data for playback
  if ((media.type == MEDIA_TYPE_VIDEO || media.type == MEDIA_TYPE_AUDIO) && media.video_data && media.video_size > 0) {
    media_note->media_data = g_malloc(media.video_size);
    memcpy(media_note->media_data, media.video_data, media.video_size);
    media_note->media_size = media.video_size;
    media_note->reset_media_data = TRUE;
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

    media_note->media_data = g_malloc(model_element->video->video_size);
    memcpy(media_note->media_data, model_element->video->video_data, model_element->video->video_size);
    media_note->media_size = model_element->video->video_size;
  }

  // CREATE PIPELINE ON FIRST PLAY
  if (!media_note->media_pipeline) {
    GError *error = NULL;
    media_note->media_pipeline = gst_parse_launch(
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
    GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->media_pipeline), "source");
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
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(media_note->media_pipeline));
    media_note->bus_watch_id = gst_bus_add_watch(bus, media_bus_callback, media_note);
    gst_object_unref(bus);

    // Create video widget
    media_note->media_widget = gtk_drawing_area_new();
    gtk_widget_set_size_request(media_note->media_widget,
                                media_note->base.width,
                                media_note->base.height);

    // Make widget ignore input events so clicks go through
    gtk_widget_set_sensitive(media_note->media_widget, FALSE);
    gtk_widget_set_can_focus(media_note->media_widget, FALSE);
    gtk_widget_set_focusable(media_note->media_widget, FALSE);

    // Add to overlay
    gtk_overlay_add_overlay(GTK_OVERLAY(media_note->base.canvas_data->overlay),
                            media_note->media_widget);

    // Position video widget
    int screen_x, screen_y;
    canvas_canvas_to_screen(media_note->base.canvas_data,
                            media_note->base.x,
                            media_note->base.y,
                            &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->media_widget, screen_x);
    gtk_widget_set_margin_top(media_note->media_widget, screen_y);
  }

  if (media_note->media_playing) {
    // Pause playback
    gst_element_set_state(media_note->media_pipeline, GST_STATE_PAUSED);
    media_note->media_playing = FALSE;

    if (media_note->media_widget) {
      gtk_widget_hide(media_note->media_widget);
    }
  } else {
    // If pipeline was in NULL state (after EOS), we need to reset it
    GstState state;
    gst_element_get_state(media_note->media_pipeline, &state, NULL, 0);

    if (state == GST_STATE_NULL) {
      // Reset the pipeline to READY state first
      gst_element_set_state(media_note->media_pipeline, GST_STATE_READY);

      // Reset appsrc to prepare for new data
      GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->media_pipeline), "source");
      if (appsrc) {
        // Reset appsrc properties
        g_object_set(appsrc, "block", TRUE, NULL);
        gst_object_unref(appsrc);
      }
    }

    // Start playback from beginning
    media_note->reset_media_data = TRUE;
    gst_element_set_state(media_note->media_pipeline, GST_STATE_PLAYING);
    media_note->media_playing = TRUE;

    if (media_note->media_widget) {
      gtk_widget_show(media_note->media_widget);

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

void media_note_toggle_audio_playback(Element *element) {
  MediaNote *media_note = (MediaNote*)element;

  if (media_note->media_type != MEDIA_TYPE_AUDIO) {
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
  if (!model_element || !model_element->audio) {
    return;
  }

  // Load audio data if not loaded
  if (!model_element->audio->is_loaded) {
    if (!model_load_audio_data(media_note->base.canvas_data->model, model_element->audio)) {
      g_printerr("Failed to load audio data\n");
      return;
    }

    media_note->media_data = g_malloc(model_element->audio->audio_size);
    memcpy(media_note->media_data, model_element->audio->audio_data, model_element->audio->audio_size);
    media_note->media_size = model_element->audio->audio_size;
  }

  // CREATE PIPELINE ON FIRST PLAY
  if (!media_note->media_pipeline) {
    GError *error = NULL;
    media_note->media_pipeline = gst_parse_launch(
                                                  "appsrc name=source is-live=true format=time ! "
                                                  "queue ! "
                                                  "decodebin ! audioconvert ! autoaudiosink",
                                                  &error
                                                  );

    if (error) {
      g_printerr("Failed to create audio pipeline: %s\n", error->message);
      g_error_free(error);
      return;
    }

    // Configure appsrc
    GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->media_pipeline), "source");
    if (appsrc) {
      GstCaps *caps = gst_caps_new_simple("audio/mpeg",
                                          "mpegversion", G_TYPE_INT, 1,
                                          "layer", G_TYPE_INT, 3,
                                          NULL);
      // Set appsrc properties - limit buffering so we don't push all data at once
      g_object_set(appsrc,
                   "caps", caps,
                   "stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
                   "format", GST_FORMAT_TIME,
                   "is-live", FALSE,
                   "max-bytes", (guint64)(1 * 1024 * 1024), // Buffer max 1MB - controls backpressure
                   NULL);
      gst_caps_unref(caps);

      g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_callback), media_note);
      gst_object_unref(appsrc);
    }

    // Set up bus callback
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(media_note->media_pipeline));
    media_note->bus_watch_id = gst_bus_add_watch(bus, media_bus_callback, media_note);
    gst_object_unref(bus);
  }

  if (media_note->media_playing) {
    // Pause playback
    gst_element_set_state(media_note->media_pipeline, GST_STATE_PAUSED);
    media_note->media_playing = FALSE;
    media_note_store_playback_state(media_note, FALSE);
  } else {
    // If pipeline was in NULL state (after EOS), we need to reset it
    GstState state;
    gst_element_get_state(media_note->media_pipeline, &state, NULL, 0);

    if (state == GST_STATE_NULL) {
      // Reset the pipeline to READY state first
      gst_element_set_state(media_note->media_pipeline, GST_STATE_READY);

      // Reset appsrc to prepare for new data
      GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->media_pipeline), "source");
      if (appsrc) {
        g_object_set(appsrc, "block", TRUE, NULL);
        gst_object_unref(appsrc);
      }
    }

    // Start playback from beginning
    media_note->reset_media_data = TRUE;

    GstStateChangeReturn ret = gst_element_set_state(media_note->media_pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
      g_printerr("Failed to start audio playback\n");
      media_note_store_playback_state(media_note, FALSE);
      return;
    }

    media_note->media_playing = TRUE;
    media_note_store_playback_state(media_note, TRUE);
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

  // Don't allow editing while media is playing
  if (media_note->media_type == MEDIA_TYPE_VIDEO && media_note->media_playing) {
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

  int draw_x, draw_y, draw_width, draw_height;
  media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

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
    int draw_x, draw_y, draw_width, draw_height;
    media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

    int text_view_width, text_view_height;
    gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

    int screen_draw_x, screen_draw_y;
    canvas_canvas_to_screen(element->canvas_data, draw_x, draw_y, &screen_draw_x, &screen_draw_y);

    gtk_widget_set_margin_start(media_note->text_view,
                                screen_draw_x + draw_width - text_view_width - 10);
    gtk_widget_set_margin_top(media_note->text_view,
                              screen_draw_y + draw_height - text_view_height - 10);
  }

  // Update media widget position if it exists
  if (media_note->media_widget) {
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, x, y, &screen_x, &screen_y);

    gtk_widget_set_margin_start(media_note->media_widget, screen_x);
    gtk_widget_set_margin_top(media_note->media_widget, screen_y);
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
      int draw_x, draw_y, draw_width, draw_height;
      media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

      int text_view_width, text_view_height;
      gtk_widget_get_size_request(media_note->text_view, &text_view_width, &text_view_height);

      gtk_widget_set_margin_start(media_note->text_view, draw_x + draw_width - text_view_width - 10);
      gtk_widget_set_margin_top(media_note->text_view, draw_y + draw_height - text_view_height - 10);
    }
  }

  // Update media widget size if it exists
  if (media_note->media_widget) {
    gtk_widget_set_size_request(media_note->media_widget, width, height);
  }

  // Update model
  Model* model = media_note->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  model_update_size(model, model_element, width, height);
}

void media_note_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  MediaNote *media_note = (MediaNote*)element;

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  int draw_x, draw_y, draw_width, draw_height;
  media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

  gboolean custom_audio_card = FALSE;

  if (media_note->media_type == MEDIA_TYPE_AUDIO && !media_note->has_thumbnail) {
    custom_audio_card = TRUE;

    double x = element->x;
    double y = element->y;
    double w = element->width;
    double h = element->height;
    double corner = MIN(h / 2.0, 18.0);

    cairo_save(cr);
    cairo_new_path(cr);
    cairo_arc(cr, x + w - corner, y + corner, corner, -G_PI_2, 0);
    cairo_arc(cr, x + w - corner, y + h - corner, corner, 0, G_PI_2);
    cairo_arc(cr, x + corner, y + h - corner, corner, G_PI_2, G_PI);
    cairo_arc(cr, x + corner, y + corner, corner, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr,
                          element->bg_r,
                          element->bg_g,
                          element->bg_b,
                          element->bg_a);
    cairo_fill(cr);
    cairo_restore(cr);

    // Draw a subtle accent panel on the left
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w * 0.3, h);
    cairo_set_source_rgba(cr,
                          CLAMP(element->bg_r * 0.75 + 0.1, 0.0, 1.0),
                          CLAMP(element->bg_g * 0.75 + 0.1, 0.0, 1.0),
                          CLAMP(element->bg_b * 0.75 + 0.1, 0.0, 1.0),
                          MIN(1.0, element->bg_a + 0.2));
    cairo_fill(cr);
    cairo_restore(cr);

    // Audio glyph (circle with lines)
    cairo_save(cr);
    double center_x = x + (w * 0.18);
    double center_y = y + h / 2.0;
    double radius = MIN(h * 0.28, w * 0.18);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_arc(cr, center_x, center_y, radius, 0, 2 * G_PI);
    cairo_fill(cr);

    cairo_set_line_width(cr, MAX(2.0, h * 0.04));
    cairo_set_source_rgba(cr, element->bg_r * 0.4, element->bg_g * 0.4, element->bg_b * 0.4, 0.9);
    cairo_move_to(cr, center_x + radius * 0.4, center_y - radius * 0.6);
    cairo_line_to(cr, center_x + radius * 0.4, center_y + radius * 0.6);
    cairo_stroke(cr);

    cairo_set_line_width(cr, MAX(1.5, h * 0.025));
    for (int wave = 0; wave < 2; wave++) {
      double offset = (wave + 1) * radius * 0.5;
      cairo_arc(cr, center_x, center_y, offset, -G_PI_4, G_PI_4);
      cairo_stroke(cr);
      cairo_arc(cr, center_x, center_y, offset, G_PI - G_PI_4, G_PI + G_PI_4);
      cairo_stroke(cr);
    }
    cairo_restore(cr);
  }

  if (!custom_audio_card && media_note->pixbuf) {
    // Draw the image scaled to fit the entire element
    cairo_save(cr);
    cairo_rectangle(cr, draw_x, draw_y, draw_width, draw_height);
    cairo_clip(cr);
    cairo_translate(cr, draw_x, draw_y);
    double scale_x = (double)draw_width / gdk_pixbuf_get_width(media_note->pixbuf);
    double scale_y = (double)draw_height / gdk_pixbuf_get_height(media_note->pixbuf);
    cairo_scale(cr, scale_x, scale_y);
    gdk_cairo_set_source_pixbuf(cr, media_note->pixbuf, 0, 0);

    if (media_note->media_type == MEDIA_TYPE_VIDEO && media_note->media_playing) {
      cairo_paint_with_alpha(cr, 0.3); // Semi-transparent when media is playing
    } else {
      cairo_paint(cr); // Opaque when not playing
    }
    cairo_restore(cr);
  } else if (!custom_audio_card) {
    // Fallback to element bounds if no pixbuf
    // Draw a simple rectangle to represent the element
    cairo_set_source_rgba(cr, element->bg_r, element->bg_g, element->bg_b, element->bg_a);
    cairo_rectangle(cr, draw_x, draw_y, draw_width, draw_height);
    cairo_fill(cr);
  }

  // Draw play icon for video and audio elements
  if (media_note->media_type == MEDIA_TYPE_VIDEO || media_note->media_type == MEDIA_TYPE_AUDIO) {
    cairo_save(cr);

    if (media_note->media_playing) {
      // Draw pause icon when media is playing
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
      // Draw play icon when media is not playing
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

  // Draw text or duration (if not editing and not media playing)
  if (!media_note->editing &&
      !(media_note->media_type == MEDIA_TYPE_VIDEO && media_note->media_playing)) {
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
      pango_layout_set_alignment(layout, element_get_pango_alignment(media_note->alignment));

      // Apply strikethrough if enabled
      if (media_note->strikethrough) {
        PangoAttrList *attrs = pango_attr_list_new();
        PangoAttribute *strike_attr = pango_attr_strikethrough_new(TRUE);
        pango_attr_list_insert(attrs, strike_attr);
        pango_layout_set_attributes(layout, attrs);
        pango_attr_list_unref(attrs);
      }

      int text_width, text_height;
      pango_layout_get_pixel_size(layout, &text_width, &text_height);

      // Calculate horizontal position based on alignment
      int text_x;
      PangoAlignment pango_align = element_get_pango_alignment(media_note->alignment);
      if (pango_align == PANGO_ALIGN_LEFT) {
        text_x = draw_x + 8;
      } else if (pango_align == PANGO_ALIGN_RIGHT) {
        text_x = draw_x + draw_width - text_width - 8;
      } else {
        text_x = draw_x + (draw_width - text_width) / 2;
      }

      // Calculate vertical position based on alignment
      int text_y;
      VerticalAlign valign = element_get_vertical_alignment(media_note->alignment);
      if (valign == VALIGN_TOP) {
        text_y = draw_y + 8;
      } else if (valign == VALIGN_BOTTOM) {
        text_y = draw_y + draw_height - text_height - 8;
      } else {
        text_y = draw_y + (draw_height - text_height) / 2;
      }

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

  // Draw resize handles and connection points (only when selected) - BEFORE restoring rotation
  if (is_selected) {
    // Draw resize handles at the corners of the actual image (with rotation)
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
      // Get unrotated connection point on image
      int conn_draw_x, conn_draw_y, conn_draw_width, conn_draw_height;
      media_note_get_visible_bounds(media_note, &conn_draw_x, &conn_draw_y, &conn_draw_width, &conn_draw_height);

      switch(i) {
      case 0: cx = conn_draw_x + conn_draw_width/2; cy = conn_draw_y; break;
      case 1: cx = conn_draw_x + conn_draw_width; cy = conn_draw_y + conn_draw_height/2; break;
      case 2: cx = conn_draw_x + conn_draw_width/2; cy = conn_draw_y + conn_draw_height; break;
      case 3: cx = conn_draw_x; cy = conn_draw_y + conn_draw_height/2; break;
      }

      cairo_arc(cr, cx, cy, 5, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.3, 0.3, 0.8, 0.3);
      cairo_fill(cr);
    }
  }

  // Restore cairo state after drawing selection UI with rotation
  cairo_restore(cr);

  // Draw rotation handle without rotation
  if (is_selected) {
    element_draw_rotation_handle(element, cr);
  }
}

void media_note_get_connection_point(Element *element, int point, int *cx, int *cy) {
  MediaNote *media_note = (MediaNote*)element;
  int draw_x, draw_y, draw_width, draw_height;
  media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

  int unrotated_x = 0, unrotated_y = 0;

  // Calculate unrotated connection point based on the visual bounds
  switch(point) {
  case 0: // Top-center
    unrotated_x = draw_x + draw_width / 2;
    unrotated_y = draw_y;
    break;
  case 1: // Right-center
    unrotated_x = draw_x + draw_width;
    unrotated_y = draw_y + draw_height / 2;
    break;
  case 2: // Bottom-center
    unrotated_x = draw_x + draw_width / 2;
    unrotated_y = draw_y + draw_height;
    break;
  case 3: // Left-center
    unrotated_x = draw_x;
    unrotated_y = draw_y + draw_height / 2;
    break;
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

  int draw_x, draw_y, draw_width, draw_height;
  media_note_get_visible_bounds(media_note, &draw_x, &draw_y, &draw_width, &draw_height);

  int size = 8;
  int unrotated_handles[4][2] = {
    {draw_x, draw_y},
    {draw_x + draw_width, draw_y},
    {draw_x + draw_width, draw_y + draw_height},
    {draw_x, draw_y + draw_height}
  };

  // For small elements (< 50px), only show bottom-right handle (index 2)
  gboolean is_small = (element->width < 50 || element->height < 50);

  // Rotate handles around element center if rotated
  double center_x = element->x + element->width / 2.0;
  double center_y = element->y + element->height / 2.0;

  for (int i = 0; i < 4; i++) {
    if (is_small && i != 2) continue; // Skip all but bottom-right for small elements

    int handle_x = unrotated_handles[i][0];
    int handle_y = unrotated_handles[i][1];

    if (element->rotation_degrees != 0.0) {
      double dx = handle_x - center_x;
      double dy = handle_y - center_y;
      double angle_rad = element->rotation_degrees * M_PI / 180.0;
      handle_x = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
      handle_y = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
    }

    if (abs(x - handle_x) <= size && abs(y - handle_y) <= size) {
      return i;
    }
  }
  return -1;
}

int media_note_pick_connection_point(Element *element, int x, int y) {
  // Hide connection points for small elements (< 100px on either dimension)
  if (element->width < 100 || element->height < 100) {
    return -1;
  }

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

  if (media_note->media_type == MEDIA_TYPE_AUDIO) {
    media_note_store_playback_state(media_note, FALSE);
  }

  if (media_note->media_pipeline) {
    GstElement *appsrc = gst_bin_get_by_name(GST_BIN(media_note->media_pipeline), "source");
    if (appsrc) {
      g_signal_handlers_disconnect_by_func(appsrc, G_CALLBACK(need_data_callback), media_note);
      gst_object_unref(appsrc);
    }

    gst_element_set_state(media_note->media_pipeline, GST_STATE_NULL);
    gst_element_get_state(media_note->media_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    if (media_note->bus_watch_id > 0) {
      g_source_remove(media_note->bus_watch_id);
      media_note->bus_watch_id = 0;
    }

    gst_object_unref(media_note->media_pipeline);
    media_note->media_pipeline = NULL;
  }

  if (media_note->media_widget && GTK_IS_WIDGET(media_note->media_widget)) {
    gtk_widget_unparent(media_note->media_widget);
  }
  media_note->media_widget = NULL;

  if (media_note->pixbuf) g_object_unref(media_note->pixbuf);
  if (media_note->text) g_free(media_note->text);
  if (media_note->font_description) g_free(media_note->font_description);
  if (media_note->alignment) g_free(media_note->alignment);

  // Free media data
  if (media_note->media_data) {
    g_free(media_note->media_data);
    media_note->media_data = NULL;
    media_note->media_size = 0;
  }

  if (media_note->text_view && GTK_IS_WIDGET(media_note->text_view) && gtk_widget_get_parent(media_note->text_view)) {
    gtk_widget_unparent(media_note->text_view);
  }

  media_note->media_playing = FALSE;

  g_free(media_note);
}
