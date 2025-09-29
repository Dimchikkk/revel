#include "canvas_drop.h"
#include "canvas_core.h"
#include "model.h"
#include "element.h"
#include "media_note.h"
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/app.h>
#include <glib.h>
#include <string.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MAX_VIDEO_SIZE_MB 30

// Structure to hold thumbnail generation data
typedef struct {
  GstSample *sample;
  GMutex mutex;
  GCond cond;
  gboolean finished;
} ThumbnailData;

static unsigned char* read_file_data(const gchar *file_path, int *file_size) {
  GError *error = NULL;
  gchar *contents = NULL;
  gsize length = 0;

  if (g_file_get_contents(file_path, &contents, &length, &error)) {
    *file_size = (int)length;
    return (unsigned char*)contents;
  } else {
    g_printerr("Failed to read file: %s\n", error->message);
    g_error_free(error);
    return NULL;
  }
}

// Callback to capture thumbnail from appsink
static GstFlowReturn on_new_sample(GstElement *sink, ThumbnailData *thumb_data) {
  GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

  if (sample) {
    g_mutex_lock(&thumb_data->mutex);
    if (thumb_data->sample) {
      gst_sample_unref(thumb_data->sample);
    }
    thumb_data->sample = gst_sample_ref(sample);
    thumb_data->finished = TRUE;
    g_cond_signal(&thumb_data->cond);
    g_mutex_unlock(&thumb_data->mutex);

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

// Generate thumbnail from video file in memory
GstSample* generate_video_thumbnail(const gchar *file_path) {
  // Ensure GStreamer is initialized
  static gboolean gst_initialized = FALSE;
  if (!gst_initialized) {
    gst_init(NULL, NULL);
    gst_initialized = TRUE;
  }

  GError *error = NULL;
  ThumbnailData thumb_data = {0};
  g_mutex_init(&thumb_data.mutex);
  g_cond_init(&thumb_data.cond);

  // Create pipeline: filesrc -> decodebin -> videoconvert -> videoscale -> appsink
  gchar *pipeline_str = g_strdup_printf(
                                        "filesrc location=\"%s\" ! "
                                        "decodebin ! "
                                        "videoconvert ! "
                                        "videoscale ! "
                                        "video/x-raw,width=320,height=240,format=RGB ! "
                                        "appsink name=thumb_sink emit-signals=true sync=false max-buffers=1 drop=true",
                                        file_path
                                        );

  GstElement *pipeline = gst_parse_launch(pipeline_str, &error);
  g_free(pipeline_str);

  if (error) {
    g_printerr("Failed to create thumbnail pipeline: %s\n", error->message);
    g_error_free(error);
    g_mutex_clear(&thumb_data.mutex);
    g_cond_clear(&thumb_data.cond);
    return NULL;
  }

  if (!pipeline) {
    g_printerr("Failed to create GStreamer pipeline\n");
    g_mutex_clear(&thumb_data.mutex);
    g_cond_clear(&thumb_data.cond);
    return NULL;
  }

  // Get appsink and configure it
  GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "thumb_sink");
  if (!sink) {
    g_printerr("Failed to get appsink element\n");
    gst_object_unref(pipeline);
    g_mutex_clear(&thumb_data.mutex);
    g_cond_clear(&thumb_data.cond);
    return NULL;
  }

  g_object_set(sink, "emit-signals", TRUE, NULL);
  g_object_set(sink, "sync", FALSE, NULL);
  g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), &thumb_data);

  // Start pipeline
  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Failed to set pipeline to playing state\n");
    gst_object_unref(sink);
    gst_object_unref(pipeline);
    g_mutex_clear(&thumb_data.mutex);
    g_cond_clear(&thumb_data.cond);
    return NULL;
  }

  // Wait for thumbnail to be captured (max 5 seconds)
  g_mutex_lock(&thumb_data.mutex);
  if (!thumb_data.finished) {
    g_cond_wait_until(&thumb_data.cond, &thumb_data.mutex,
                      g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND);
  }
  g_mutex_unlock(&thumb_data.mutex);

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(sink);
  gst_object_unref(pipeline);

  GstSample *result = thumb_data.sample;
  g_mutex_clear(&thumb_data.mutex);
  g_cond_clear(&thumb_data.cond);

  return result;
}

// Convert GstSample to GdkPixbuf (in-memory)
GdkPixbuf* sample_to_pixbuf(GstSample *sample) {
  if (!sample) return NULL;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstCaps *caps = gst_sample_get_caps(sample);

  if (!buffer || !caps) return NULL;

  // Get video frame information
  GstVideoInfo info;
  if (!gst_video_info_from_caps(&info, caps)) {
    g_printerr("Failed to get video info from caps\n");
    return NULL;
  }

  // Map the buffer for reading
  GstMapInfo map;
  if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    g_printerr("Failed to map buffer\n");
    return NULL;
  }

  // Create pixbuf from the video frame data
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
                                               map.data,
                                               GDK_COLORSPACE_RGB,
                                               FALSE, // has_alpha
                                               8, // bits_per_sample
                                               info.width,
                                               info.height,
                                               info.stride[0], // rowstride
                                               NULL, // destroy_fn
                                               NULL // destroy_fn_data
                                               );

  // Keep a reference to the buffer data (we'll unmap when pixbuf is destroyed)
  gst_buffer_unmap(buffer, &map);
  return pixbuf;
}

// Function to extract filename from path
static gchar* get_filename_from_path(const gchar *file_path) {
  const gchar *filename = strrchr(file_path, '/');
  if (filename) {
    return g_strdup(filename + 1); // Skip the '/'
  }
  return g_strdup(file_path); // Return original if no path separator found
}

// Function to create media note from pixbuf at specified coordinates
static void create_media_note_from_pixbuf(CanvasData *data, GdkPixbuf *pixbuf,
                                          int canvas_x, int canvas_y, const gchar *filename,
                                          const gchar *file_path, gint64 duration_seconds,
                                          const gchar *original_extension) {
  if (!pixbuf) return;

  GError *error = NULL;
  gsize buffer_size;
  gchar *buffer = NULL;
  gchar *format = "png"; // Default format

  // Determine the format based on original file extension
  if (original_extension) {
    if (g_ascii_strcasecmp(original_extension, ".jpg") == 0 ||
        g_ascii_strcasecmp(original_extension, ".jpeg") == 0) {
      format = "jpeg";
    } else if (g_ascii_strcasecmp(original_extension, ".png") == 0) {
      format = "png";
    }
    // Add more formats as needed
  }

  // Convert pixbuf to the original format in memory
  if (gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size, format, &error, NULL)) {
    ElementPosition position = {
      .x = canvas_x,
      .y = canvas_y,
      .z = data->next_z_index++,
    };
    ElementColor bg_color = {
      .r = 1.0,
      .g = 1.0,
      .b = 1.0,
      .a = 1.0,
    };
    ElementColor text_color = {
      .r = 1.0,
      .g = 1.0,
      .b = 1.0,
      .a = 1.0,
    };
    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(data->drawing_area));
    ElementSize size = {
      .width = gdk_pixbuf_get_width(pixbuf) / scale,
      .height = gdk_pixbuf_get_height(pixbuf) / scale,
    };

    // Read video data into memory
    int video_size = 0;
    unsigned char *video_data = read_file_data(file_path, &video_size);

    if (!video_data) {
      g_free(buffer);
      return;
    }

    // Create ElementMedia structure
    ElementMedia media = {
      .type = MEDIA_TYPE_VIDEO,
      .image_data = (unsigned char*)buffer,  // Thumbnail data
      .image_size = buffer_size,             // Thumbnail size
      .video_data = video_data,              // Video data
      .video_size = video_size,              // Video size
      .duration = (int)duration_seconds      // Duration in seconds
    };
    ElementConnection connection = {
      .from_element_uuid = NULL,
      .to_element_uuid = NULL,
      .from_point = -1,
      .to_point = -1,
    };
    ElementDrawing drawing = {
      .drawing_points = NULL,
      .stroke_width = 0,
    };
    ElementText text = {
      .text = g_strdup(filename),
      .text_color = text_color,
      .font_description = g_strdup("Ubuntu Mono 12"),
    };
    ElementConfig config = {
      .type = ELEMENT_MEDIA_FILE,
      .bg_color = bg_color,
      .position = position,
      .size = size,
      .media = media,
      .drawing = drawing,
      .connection = connection,
      .text = text,
    };

    ModelElement *model_element = model_create_element(data->model, config);

    if (model_element) {
      model_element->visual_element = create_visual_element(model_element, data);

      if (model_element->visual_element) {
        gtk_widget_queue_draw(data->drawing_area);
      }
    }

    g_free(buffer);
    // Note: video_data is now owned by the model_element, don't free it here
  } else {
    g_printerr("Failed to save pixbuf to buffer: %s\n", error ? error->message : "Unknown error");
    if (error) g_error_free(error);
  }
}

// Function to get MP4 duration in seconds
gint64 get_mp4_duration(const gchar *file_path) {
  GError *error = NULL;
  GstElement *pipeline;
  GstStateChangeReturn ret;
  gint64 duration_nanoseconds = -1;

  // Initialize GStreamer if not already done
  static gboolean gst_initialized = FALSE;
  if (!gst_initialized) {
    gst_init(NULL, NULL);
    gst_initialized = TRUE;
  }

  // Create a simple pipeline to discover duration
  gchar *pipeline_str = g_strdup_printf("filesrc location=\"%s\" ! qtdemux ! fakesink", file_path);
  pipeline = gst_parse_launch(pipeline_str, &error);
  g_free(pipeline_str);

  if (error) {
    g_printerr("Failed to create pipeline: %s\n", error->message);
    g_error_free(error);
    return -1;
  }

  if (!pipeline) {
    g_printerr("Failed to create GStreamer pipeline\n");
    return -1;
  }

  // Set pipeline to PAUSED state to discover duration
  ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Failed to set pipeline to paused state\n");
    gst_object_unref(pipeline);
    return -1;
  }

  // Wait for state change to complete
  ret = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Failed to get pipeline state\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return -1;
  }

  // Query the duration
  if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration_nanoseconds)) {
    gint64 duration_seconds = duration_nanoseconds / GST_SECOND;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return duration_seconds;
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return -1;
}

// Function to get file size in bytes
static gint64 get_file_size(const gchar *file_path) {
  GFile *file = g_file_new_for_path(file_path);
  GFileInfo *info = g_file_query_info(file,
                                      G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL,
                                      NULL);
  gint64 size = -1;

  if (info) {
    size = g_file_info_get_size(info);
    g_object_unref(info);
  }

  g_object_unref(file);
  return size;
}

gboolean canvas_on_drop(GtkDropTarget *target, const GValue *value,
                        double x, double y, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;

  if (G_VALUE_HOLDS(value, G_TYPE_FILE)) {
    GFile *file = g_value_get_object(value);
    gchar *file_path = g_file_get_path(file);

    if (file_path) {
      // Check file extension
      const gchar *extension = strrchr(file_path, '.');
      if (extension) {
        gboolean is_video = (g_ascii_strcasecmp(extension, ".mp4") == 0);
        gboolean is_image = (g_ascii_strcasecmp(extension, ".png") == 0 ||
                            g_ascii_strcasecmp(extension, ".jpg") == 0 ||
                            g_ascii_strcasecmp(extension, ".jpeg") == 0);

        if (is_video) {
          // Check video duration and file size
          gint64 duration_seconds = get_mp4_duration(file_path);
          gint64 file_size_bytes = get_file_size(file_path);
          gint64 file_size_mb = file_size_bytes / (1024 * 1024); // Convert to MB

          // Check both conditions
          gboolean size_ok = (file_size_bytes > 0 && file_size_mb <= MAX_VIDEO_SIZE_MB);

          if (size_ok) {
            // Convert screen coordinates to canvas coordinates
            int canvas_x, canvas_y;
            canvas_screen_to_canvas(data, (int)x, (int)y, &canvas_x, &canvas_y);

            // Generate thumbnail in memory
            GstSample *thumbnail_sample = generate_video_thumbnail(file_path);
            if (thumbnail_sample) {
              // Convert to GdkPixbuf
              GdkPixbuf *thumbnail = sample_to_pixbuf(thumbnail_sample);

              if (thumbnail) {
                // Extract just the filename
                gchar *filename = get_filename_from_path(file_path);

                // Create media note with the thumbnail at drop location
                create_media_note_from_pixbuf(data, thumbnail, canvas_x, canvas_y, filename, file_path, duration_seconds, extension);

                g_free(filename);
                g_object_unref(thumbnail);
              }
              gst_sample_unref(thumbnail_sample);
            }
          } else {
            // Show error message for invalid videos
            GtkWidget *dialog;
            gchar *message = NULL;

            if (!size_ok) {
              message = g_strdup_printf(
                                        "Video too large: %ld MB\nMaximum allowed: " STR(MAX_VIDEO_SIZE_MB) " MB",
                                        file_size_mb
                                        );
            }

            if (message) {
              dialog = gtk_message_dialog_new(
                                              GTK_WINDOW(gtk_widget_get_ancestor(data->drawing_area, GTK_TYPE_WINDOW)),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_OK,
                                              "%s",
                                              message
                                              );
              gtk_window_present(GTK_WINDOW(dialog));
              g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
              g_free(message);
            }
          }
        } else if (is_image) {
          // Handle image files (PNG, JPEG)
          GError *error = NULL;
          GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(file_path, &error);

          if (pixbuf) {
            // Convert screen coordinates to canvas coordinates
            int canvas_x, canvas_y;
            canvas_screen_to_canvas(data, (int)x, (int)y, &canvas_x, &canvas_y);

            // Extract just the filename
            gchar *filename = get_filename_from_path(file_path);

            // Read the original image file data instead of converting from pixbuf
            int image_size = 0;
            unsigned char *image_data = read_file_data(file_path, &image_size);

            if (image_data) {
              ElementPosition position = {
                .x = canvas_x,
                .y = canvas_y,
                .z = data->next_z_index++,
              };
              ElementColor bg_color = {
                .r = 1.0,
                .g = 1.0,
                .b = 1.0,
                .a = 1.0,
              };
              ElementColor text_color = {
                .r = 1.0,
                .g = 1.0,
                .b = 1.0,
                .a = 1.0,
              };
              int scale = gtk_widget_get_scale_factor(GTK_WIDGET(data->drawing_area));
              ElementSize size = {
                .width = gdk_pixbuf_get_width(pixbuf) / scale,
                .height = gdk_pixbuf_get_height(pixbuf) / scale,
              };

              // Create ElementMedia structure for image using original file data
              ElementMedia media = {
                .type = MEDIA_TYPE_IMAGE,
                .image_data = image_data,  // Use original file data
                .image_size = image_size,  // Original file size
                .video_data = NULL,
                .video_size = 0,
                .duration = 0
              };

              ElementConnection connection = {
                .from_element_uuid = NULL,
                .to_element_uuid = NULL,
                .from_point = -1,
                .to_point = -1,
              };
              ElementDrawing drawing = {
                .drawing_points = NULL,
                .stroke_width = 0,
              };
              ElementText text = {
                .text = filename,
                .text_color = text_color,
                .font_description = g_strdup("Ubuntu Mono 12"),
              };
              ElementConfig config = {
                .type = ELEMENT_MEDIA_FILE,
                .bg_color = bg_color,
                .position = position,
                .size = size,
                .media = media,
                .drawing = drawing,
                .connection = connection,
                .text = text,
              };

              // Create image note element
              ModelElement *model_element = model_create_element(data->model, config);

              if (model_element) {
                model_element->visual_element = create_visual_element(model_element, data);
                if (model_element->visual_element) {
                  gtk_widget_queue_draw(data->drawing_area);
                }
              }

              // Note: image_data is now owned by the model_element, don't free it here
            } else {
              g_printerr("Failed to read original image file: %s\n", file_path);
            }

            g_free(filename);
            g_object_unref(pixbuf);
          } else {
            g_printerr("Failed to load image: %s\n", error ? error->message : "Unknown error");
            if (error) g_error_free(error);
          }
        }
      }
      g_free(file_path);
    }
    return TRUE;
  }
  return FALSE;
}

void canvas_setup_drop_target(CanvasData *data) {
  GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);

  GType file_types[] = { G_TYPE_FILE };
  gtk_drop_target_set_gtypes(drop_target, file_types, G_N_ELEMENTS(file_types));

  g_signal_connect(drop_target, "drop", G_CALLBACK(canvas_on_drop), data);

  gtk_widget_add_controller(data->drawing_area, GTK_EVENT_CONTROLLER(drop_target));
}
