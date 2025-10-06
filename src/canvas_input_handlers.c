#include "canvas_input.h"
#include "canvas_core.h"
#include "canvas_actions.h"
#include "canvas_search.h"
#include "canvas_spaces.h"
#include "canvas_space_select.h"
#include "element.h"
#include "model.h"
#include "paper_note.h"
#include "note.h"
#include "media_note.h"
#include "inline_text.h"
#include "connection.h"
#include "space.h"
#include "undo_manager.h"
#include "dsl_executor.h"
#include "freehand_drawing.h"
#include "shape.h"
#include "shape_dialog.h"
#include "font_dialog.h"
#include "clone_dialog.h"
#include "ui_event_bus.h"

#include <graphene.h>
#include <math.h>

extern void canvas_show_shortcuts_dialog(CanvasData *data);

static void on_clone_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    model_save_elements(data->model);
    ModelElement *original = g_hash_table_lookup(data->model->elements, element_uuid);
    if (original) {
      clone_dialog_open(data, original);
    }
  }
}

static gboolean destroy_popover_callback(gpointer user_data) {
  GtkWidget *popover = user_data;
  gtk_widget_unparent(popover);
  return G_SOURCE_REMOVE;
}

static void on_popover_closed(GtkPopover *popover, gpointer user_data) {
  g_idle_add(destroy_popover_callback, popover);
}

static void on_description_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    ModelElement *model_element = g_object_get_data(G_OBJECT(dialog), "model_element");
    GtkTextBuffer *buffer = g_object_get_data(G_OBJECT(dialog), "text_buffer");
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");

    if (model_element && buffer && data) {
      GtkTextIter start, end;
      gtk_text_buffer_get_bounds(buffer, &start, &end);
      gchar *new_description = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

      g_free(model_element->description);
      model_element->description = g_strdup(new_description);

      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }

      g_free(new_description);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_description_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      GtkWidget *dialog = gtk_dialog_new_with_buttons("Element Description",
                                                      GTK_WINDOW(gtk_widget_get_root(data->drawing_area)),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "Cancel", GTK_RESPONSE_CANCEL,
                                                      "Save", GTK_RESPONSE_OK,
                                                      NULL);

      GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
      gtk_widget_set_size_request(dialog, 400, 300);

      gchar *created_text;
      if (model_element->created_at) {
        created_text = g_strdup_printf("Created: %s", model_element->created_at);
      } else if (model_element->state == MODEL_STATE_NEW) {
        created_text = g_strdup("Created: Just now (not saved yet)");
      } else {
        created_text = g_strdup("Created: Unknown");
      }
      GtkWidget *created_label = gtk_label_new(created_text);
      gtk_label_set_xalign(GTK_LABEL(created_label), 0.0);
      gtk_box_append(GTK_BOX(content_area), created_label);
      g_free(created_text);

      GtkWidget *scrolled = gtk_scrolled_window_new();
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand(scrolled, TRUE);

      GtkWidget *text_view = gtk_text_view_new();
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
      if (model_element->description) {
        gtk_text_buffer_set_text(buffer, model_element->description, -1);
      }

      gtk_box_append(GTK_BOX(content_area), scrolled);

      g_object_set_data(G_OBJECT(dialog), "model_element", model_element);
      g_object_set_data(G_OBJECT(dialog), "text_buffer", buffer);
      g_object_set_data(G_OBJECT(dialog), "canvas_data", data);

      g_signal_connect(dialog, "response", G_CALLBACK(on_description_dialog_response), NULL);

      gtk_widget_show(dialog);
    }
  }
}

static void on_lock_unlock_element_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      gboolean new_locked_state = !model_element->locked;
      model_update_locked(data->model, model_element, new_locked_state);
      gtk_widget_queue_draw(data->drawing_area);
      canvas_show_notification(data, new_locked_state ? "Element locked" : "Element unlocked");
    }
  }
}

static void on_delete_element_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      if (model_element->type->type == ELEMENT_SPACE) {
        if (model_element->state != MODEL_STATE_NEW) {
          canvas_show_notification(data, "Cannot delete a saved space from canvas view");
          return;
        }
      }

      undo_manager_push_delete_action(data->undo_manager, model_element);

      model_delete_element(data->model, model_element);
      canvas_sync_with_model(data);
      gtk_widget_queue_draw(data->drawing_area);
    }
  }
}

static void on_color_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");
    const gchar *element_uuid = g_object_get_data(G_OBJECT(dialog), "element_uuid");

    GtkColorChooser *chooser = GTK_COLOR_CHOOSER(dialog);
    GdkRGBA color;
    gtk_color_chooser_get_rgba(chooser, &color);

    if (data && data->model && element_uuid) {
      ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
      if (model_element && model_element->bg_color) {
        double old_r = model_element->bg_color->r;
        double old_g = model_element->bg_color->g;
        double old_b = model_element->bg_color->b;
        double old_a = model_element->bg_color->a;

        undo_manager_push_color_action(data->undo_manager, model_element,
                                       old_r, old_g, old_b, old_a,
                                       color.red, color.green, color.blue, color.alpha);

        model_update_color(data->model, model_element, color.red, color.green, color.blue, color.alpha);
        canvas_sync_with_model(data);
        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_change_color_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element) {
      GtkWidget *dialog = gtk_color_chooser_dialog_new("Choose Element Color",
                                                       GTK_WINDOW(gtk_widget_get_root(data->drawing_area)));

      gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dialog), TRUE);

      if (model_element->bg_color) {
        GdkRGBA initial_color = {
          .red = model_element->bg_color->r,
          .green = model_element->bg_color->g,
          .blue = model_element->bg_color->b,
          .alpha = model_element->bg_color->a
        };
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &initial_color);
      }

      g_object_set_data(G_OBJECT(dialog), "canvas_data", data);
      g_object_set_data_full(G_OBJECT(dialog), "element_uuid", g_strdup(element_uuid), g_free);

      g_signal_connect(dialog, "response", G_CALLBACK(on_color_dialog_response), NULL);
      gtk_window_present(GTK_WINDOW(dialog));
    }
  }
}

static void on_change_space_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_show_space_select_dialog(data, element_uuid);
  }
}

static void on_change_text_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (!model_element || !model_element->visual_element) {
      return;
    }

    Element *element = model_element->visual_element;

    if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE ||
        element->type == ELEMENT_INLINE_TEXT || element->type == ELEMENT_SPACE ||
        element->type == ELEMENT_MEDIA_FILE || element->type == ELEMENT_SHAPE) {
      element_start_editing(element, data->overlay);
    }
  }
}

static void on_hide_children_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_hide_children(data, element_uuid);
    gtk_widget_queue_draw(data->drawing_area);
  }
}

static void on_show_children_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    canvas_show_children(data, element_uuid);
    gtk_widget_queue_draw(data->drawing_area);
  }
}

typedef struct {
  CanvasData *canvas_data;
  ModelElement *model_element;
  GtkWidget *stroke_combo;
  GtkWidget *fill_combo;
} ShapeStyleDialogData;

static void on_shape_style_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  ShapeStyleDialogData *style_data = (ShapeStyleDialogData*)user_data;
  if (!style_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
    return;
  }

  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = style_data->canvas_data;
    ModelElement *model_element = style_data->model_element;
    if (data && model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)model_element->visual_element;

      int stroke_index = gtk_combo_box_get_active(GTK_COMBO_BOX(style_data->stroke_combo));
      StrokeStyle new_stroke_style = STROKE_STYLE_SOLID;
      switch (stroke_index) {
        case 1:
          new_stroke_style = STROKE_STYLE_DASHED;
          break;
        case 2:
          new_stroke_style = STROKE_STYLE_DOTTED;
          break;
        default:
          new_stroke_style = STROKE_STYLE_SOLID;
          break;
      }

      int fill_index = gtk_combo_box_get_active(GTK_COMBO_BOX(style_data->fill_combo));
      gboolean new_filled = FALSE;
      FillStyle new_fill_style = FILL_STYLE_SOLID;
      switch (fill_index) {
        case 0:
          new_filled = FALSE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
        case 1:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
        case 2:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_HACHURE;
          break;
        case 3:
          new_filled = TRUE;
          new_fill_style = FILL_STYLE_CROSS_HATCH;
          break;
        default:
          new_filled = FALSE;
          new_fill_style = FILL_STYLE_SOLID;
          break;
      }

      shape->stroke_style = new_stroke_style;
      shape->fill_style = new_fill_style;
      shape->filled = new_filled;

      model_element->stroke_style = new_stroke_style;
      model_element->fill_style = new_fill_style;
      model_element->filled = new_filled;
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }

      gtk_widget_queue_draw(data->drawing_area);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
  g_free(user_data);
}

static void on_change_shape_style_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)model_element->visual_element;

      GtkWidget *dialog = gtk_dialog_new_with_buttons("Shape Style",
                                                      GTK_WINDOW(gtk_widget_get_root(data->drawing_area)),
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "Cancel", GTK_RESPONSE_CANCEL,
                                                      "Apply", GTK_RESPONSE_OK,
                                                      NULL);

      GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
      gtk_widget_set_margin_top(content_area, 12);
      gtk_widget_set_margin_bottom(content_area, 12);
      gtk_widget_set_margin_start(content_area, 12);
      gtk_widget_set_margin_end(content_area, 12);

      GtkWidget *stroke_label = gtk_label_new("Stroke Style");
      gtk_widget_set_halign(stroke_label, GTK_ALIGN_START);
      gtk_box_append(GTK_BOX(content_area), stroke_label);

      GtkWidget *stroke_combo = gtk_combo_box_text_new();
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(stroke_combo), "Solid");
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(stroke_combo), "Dashed");
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(stroke_combo), "Dotted");
      gtk_combo_box_set_active(GTK_COMBO_BOX(stroke_combo), shape->stroke_style);
      gtk_box_append(GTK_BOX(content_area), stroke_combo);

      GtkWidget *fill_label = gtk_label_new("Fill Style");
      gtk_widget_set_halign(fill_label, GTK_ALIGN_START);
      gtk_box_append(GTK_BOX(content_area), fill_label);

      GtkWidget *fill_combo = gtk_combo_box_text_new();
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fill_combo), "None");
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fill_combo), "Solid");
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fill_combo), "Hatch");
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fill_combo), "Cross Hatch");
      gtk_combo_box_set_active(GTK_COMBO_BOX(fill_combo), shape->filled ? shape->fill_style + 1 : 0);
      gtk_box_append(GTK_BOX(content_area), fill_combo);

      ShapeStyleDialogData *style_data = g_new0(ShapeStyleDialogData, 1);
      style_data->canvas_data = data;
      style_data->model_element = model_element;
      style_data->stroke_combo = stroke_combo;
      style_data->fill_combo = fill_combo;

      g_signal_connect(dialog, "response", G_CALLBACK(on_shape_style_dialog_response), style_data);
      gtk_widget_show(dialog);
    }
  }
}

static void on_stroke_color_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  if (response_id == GTK_RESPONSE_OK) {
    CanvasData *data = g_object_get_data(G_OBJECT(dialog), "canvas_data");
    const gchar *element_uuid = g_object_get_data(G_OBJECT(dialog), "element_uuid");
    if (data && data->model && element_uuid) {
      ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
      if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
        Shape *shape = (Shape*)model_element->visual_element;
        GtkColorChooser *chooser = GTK_COLOR_CHOOSER(dialog);
        GdkRGBA color;
        gtk_color_chooser_get_rgba(chooser, &color);

        shape->stroke_r = color.red;
        shape->stroke_g = color.green;
        shape->stroke_b = color.blue;
        shape->stroke_a = color.alpha;

        if (model_element->stroke_color) {
          g_free(model_element->stroke_color);
        }
        model_element->stroke_color = g_strdup_printf("#%02X%02X%02X%02X",
          (int)CLAMP(color.red * 255.0, 0, 255),
          (int)CLAMP(color.green * 255.0, 0, 255),
          (int)CLAMP(color.blue * 255.0, 0, 255),
          (int)CLAMP(color.alpha * 255.0, 0, 255));

        if (model_element->state != MODEL_STATE_NEW) {
          model_element->state = MODEL_STATE_UPDATED;
        }

        gtk_widget_queue_draw(data->drawing_area);
      }
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_change_shape_stroke_color_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)model_element->visual_element;

      GtkWidget *dialog = gtk_color_chooser_dialog_new("Choose Stroke Color",
                                                       GTK_WINDOW(gtk_widget_get_root(data->drawing_area)));
      gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(dialog), TRUE);

      GdkRGBA initial_color = {
        .red = shape->stroke_r,
        .green = shape->stroke_g,
        .blue = shape->stroke_b,
        .alpha = shape->stroke_a
      };
      gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &initial_color);

      g_object_set_data(G_OBJECT(dialog), "canvas_data", data);
      g_object_set_data_full(G_OBJECT(dialog), "element_uuid", g_strdup(element_uuid), g_free);

      g_signal_connect(dialog, "response", G_CALLBACK(on_stroke_color_dialog_response), NULL);
      gtk_widget_show(dialog);
    }
  }
}

static void on_change_arrow_type_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_CONNECTION) {
      Connection *conn = (Connection*)model_element->visual_element;
      conn->connection_type = (conn->connection_type + 1) % 2;
      model_element->connection_type = conn->connection_type;
      gtk_widget_queue_draw(data->drawing_area);

      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

static void on_change_arrowhead_type_action(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
  CanvasData *data = g_object_get_data(G_OBJECT(action), "canvas_data");
  const gchar *element_uuid = g_object_get_data(G_OBJECT(action), "element_uuid");

  if (data && data->model && element_uuid) {
    ModelElement *model_element = g_hash_table_lookup(data->model->elements, element_uuid);
    if (model_element && model_element->visual_element && model_element->type->type == ELEMENT_CONNECTION) {
      Connection *conn = (Connection*)model_element->visual_element;
      conn->arrowhead_type = (conn->arrowhead_type + 1) % 3;

      model_element->arrowhead_type = conn->arrowhead_type;

      gtk_widget_queue_draw(data->drawing_area);

      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

static void canvas_update_connections_for_selection(CanvasData *data) {
  if (!data || !data->model || !data->selected_elements) {
    return;
  }

  GHashTable *elements = data->model->elements;
  if (!elements) return;

  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, elements);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ModelElement *model_element = (ModelElement*)value;
    if (!model_element || model_element->state == MODEL_STATE_DELETED) {
      continue;
    }

    if (!model_element->type || model_element->type->type != ELEMENT_CONNECTION) {
      continue;
    }

    Connection *connection = (Connection*)model_element->visual_element;
    if (!connection || !connection->from || !connection->to) {
      continue;
    }

    gboolean affects_from = g_list_find(data->selected_elements, connection->from) != NULL;
    gboolean affects_to = g_list_find(data->selected_elements, connection->to) != NULL;

    if (!affects_from && !affects_to) {
      continue;
    }

    int new_from_point = connection->from_point;
    int new_to_point = connection->to_point;

    ConnectionRect from_rect = {
      .x = connection->from->x,
      .y = connection->from->y,
      .width = connection->from->width,
      .height = connection->from->height,
    };
    ConnectionRect to_rect = {
      .x = connection->to->x,
      .y = connection->to->y,
      .width = connection->to->width,
      .height = connection->to->height,
    };

    connection_determine_optimal_points(from_rect, to_rect, &new_from_point, &new_to_point);

    if (new_from_point != connection->from_point || new_to_point != connection->to_point) {
      connection->from_point = new_from_point;
      connection->to_point = new_to_point;

      model_element->from_point = new_from_point;
      model_element->to_point = new_to_point;
      if (model_element->state != MODEL_STATE_NEW) {
        model_element->state = MODEL_STATE_UPDATED;
      }
    }
  }
}

static void canvas_process_left_click(CanvasData *data, int n_press, double x, double y) {
  int cx, cy;
  canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

  if (data->shape_mode) {
    if (!data->current_shape) {
      ElementPosition position = { cx, cy, data->next_z_index++ };
      ElementSize size = { 0, 0 };
      ElementText text = {
        .text = "",
        .text_color = { .r = 1.0, .g = 1.0, .b = 1.0, .a = 1.0 },
        .font_description = "Ubuntu Mono 12"
      };
      ElementColor stroke_color = data->drawing_color;
      if (stroke_color.a <= 0.0) {
        stroke_color.a = 1.0;
      }
      ElementColor bg_color = stroke_color;
      ElementShape shape_config = {
        .shape_type = data->selected_shape_type,
        .stroke_width = data->drawing_stroke_width,
        .filled = data->shape_filled,
        .stroke_style = data->shape_stroke_style,
        .fill_style = data->shape_fill_style,
        .stroke_color = stroke_color
      };
      data->current_shape = shape_create(position, size, bg_color,
                                         data->drawing_stroke_width,
                                         data->selected_shape_type,
                                         data->shape_filled, text, shape_config,
                                         NULL,
                                         data);
      data->shape_start_x = cx;
      data->shape_start_y = cy;
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode) {
    if (!data->current_drawing) {
      ElementPosition position = { cx, cy, data->next_z_index++ };
      gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;
      data->current_drawing = freehand_drawing_create(position, data->drawing_color,
                                                      data->drawing_stroke_width, data);
      freehand_drawing_add_point(data->current_drawing, cx, cy);

      if (is_straight_line) {
        freehand_drawing_add_point(data->current_drawing, cx, cy);
      }
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->selected_elements) {
      for (GList *l = data->selected_elements; l != NULL; l = l->next) {
          Element *selected_element = (Element *)l->data;

          ModelElement *model_element_check = model_get_by_visual(data->model, selected_element);
          if (model_element_check && model_element_check->locked) {
              continue;
          }

          if (element_pick_rotation_handle(selected_element, cx, cy)) {
              if (!(data->modifier_state & GDK_SHIFT_MASK)) {
              } else {
                  if (!canvas_is_element_selected(data, selected_element)) {
                      data->selected_elements = g_list_append(data->selected_elements, selected_element);
                  }
              }

              element_bring_to_front(selected_element, &data->next_z_index);
              selected_element->rotating = TRUE;

              ModelElement *model_element = model_get_by_visual(data->model, selected_element);
              if (model_element) {
                  selected_element->orig_rotation = model_element->rotation_degrees;
              } else {
                  selected_element->orig_rotation = selected_element->rotation_degrees;
              }
              return;
          }
      }
  }

  Element *element = canvas_pick_element(data, cx, cy);

  if (element && element->type == ELEMENT_MEDIA_FILE &&
      (n_press == 2 || (n_press == 1 && (data->modifier_state & GDK_CONTROL_MASK)))) {
    MediaNote *media_note = (MediaNote*)element;
    if (media_note->media_type == MEDIA_TYPE_VIDEO) {
      media_note_toggle_video_playback(element);
      return;
    }
  }

  if (!element && !(data->modifier_state & GDK_SHIFT_MASK)) {
    canvas_clear_selection(data);
  }

  if (element && element->type == ELEMENT_SPACE &&
      (n_press == 2 || (n_press == 1 && (data->modifier_state & GDK_CONTROL_MASK)))) {
    model_save_elements(data->model);
    ModelElement *model_element = model_get_by_visual(data->model, element);
    switch_to_space(data, model_element->target_space_uuid);
    return;
  }

  if (!data->drag_start_positions) {
    data->drag_start_positions = g_hash_table_new(g_direct_hash, g_direct_equal);
  } else {
    g_hash_table_remove_all(data->drag_start_positions);
  }

  for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
    Element *selected_element = (Element*)sel->data;
    ModelElement *model_element = model_get_by_visual(data->model, selected_element);

    if (model_element && model_element->position) {
      PositionData *pos_data = g_new0(PositionData, 1);
      pos_data->element = model_element;
      pos_data->x = model_element->position->x;
      pos_data->y = model_element->position->y;

      g_hash_table_insert(data->drag_start_positions, model_element, pos_data);
    }
  }

  if (element) {
    int rh = element_pick_resize_handle(element, cx, cy);
    if (rh >= 0) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        canvas_clear_selection(data);
      }
      if (!canvas_is_element_selected(data, element)) {
        data->selected_elements = g_list_append(data->selected_elements, element);

        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element && model_element->position) {
          PositionData *pos_data = g_new0(PositionData, 1);
          pos_data->element = model_element;
          pos_data->x = model_element->position->x;
          pos_data->y = model_element->position->y;

          g_hash_table_insert(data->drag_start_positions, model_element, pos_data);
        }
      }
      element->dragging = TRUE;
      element->drag_offset_x = cx - element->x;
      element->drag_offset_y = cy - element->y;

      element->resizing = TRUE;
      element->resize_edge = rh;
      element->resize_start_x = cx;
      element->resize_start_y = cy;
      element->orig_x = element->x;
      element->orig_y = element->y;

      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && model_element->size) {
        element->orig_width = model_element->size->width;
        element->orig_height = model_element->size->height;
      } else {
        element->orig_width = element->width;
        element->orig_height = element->height;
      }

      return;
    }

    int cp = element_pick_connection_point(element, cx, cy);
    if (cp >= 0) {
      if (element->type == ELEMENT_SHAPE && canvas_is_element_selected(data, element)) {
        Shape *shape = (Shape*)element;
        if (shape->shape_type == SHAPE_BEZIER && shape->has_bezier_points) {
          shape->dragging_control_point = TRUE;
          shape->dragging_control_point_index = cp;
          return;
        }
      }

      if (!data->connection_start) {
        data->connection_start = element;
        data->connection_start_point = cp;
      } else {
        if (element != data->connection_start) {
          ModelElement *from_model = model_get_by_visual(data->model, data->connection_start);
          ModelElement *to_model = model_get_by_visual(data->model, element);

          if (from_model && to_model) {
            ElementPosition position = {
              .x = 0,
              .y = 0,
              .z = MAX(from_model->position->z, to_model->position->z),
            };
            ElementColor bg_color = {
              .r = 1.0,
              .g = 1.0,
              .b = 1.0,
              .a = 1.0,
            };
            ElementColor text_color = {
              .r = 0.0,
              .g = 0.0,
              .b = 0.0,
              .a = 0.0,
            };
            ElementSize size = {
              .width = 1,
              .height = 1,
            };
            ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
            ElementConnection connection = {
              .from_element = data->connection_start,
              .to_element = element,
              .from_element_uuid = from_model->uuid,
              .to_element_uuid = to_model->uuid,
              .from_point = data->connection_start_point,
              .to_point = cp,
              .connection_type = CONNECTION_TYPE_PARALLEL,
              .arrowhead_type = ARROWHEAD_SINGLE,
            };
            ElementDrawing drawing = { .drawing_points = NULL, .stroke_width = 0 };
            ElementText text = {
              .text = NULL,
              .text_color = text_color,
              .font_description = NULL,
            };
            ElementConfig config = {
              .type = ELEMENT_CONNECTION,
              .bg_color = bg_color,
              .position = position,
              .size = size,
              .media = media,
              .drawing = drawing,
              .connection = connection,
              .text = text,
            };

            ModelElement *model_conn = model_create_element(data->model, config);
            model_conn->visual_element = create_visual_element(model_conn, data);
            undo_manager_push_create_action(data->undo_manager, model_conn);
          }
        }
        data->connection_start = NULL;
        data->connection_start_point = -1;
      }
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    element_bring_to_front(element, &data->next_z_index);

    if (n_press == 2 || (n_press == 1 && (data->modifier_state & GDK_CONTROL_MASK))) {
      element_start_editing(element, data->overlay);
      gtk_widget_queue_draw(data->drawing_area);
      return;
    }

    if (!((element->type == ELEMENT_PAPER_NOTE && ((PaperNote*)element)->editing) ||
          (element->type == ELEMENT_MEDIA_FILE && ((MediaNote*)element)->editing) ||
          (element->type == ELEMENT_NOTE && ((Note*)element)->editing) ||
          (element->type == ELEMENT_SHAPE && ((Shape*)element)->editing) ||
          (element->type == ELEMENT_INLINE_TEXT && ((InlineText*)element)->editing))) {
      if (!(data->modifier_state & GDK_SHIFT_MASK)) {
        canvas_clear_selection(data);
      }
      if (!canvas_is_element_selected(data, element)) {
        data->selected_elements = g_list_append(data->selected_elements, element);

        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element && model_element->position) {
          PositionData *pos_data = g_new0(PositionData, 1);
          pos_data->element = model_element;
          pos_data->x = model_element->position->x;
          pos_data->y = model_element->position->y;

          g_hash_table_insert(data->drag_start_positions, model_element, pos_data);
        }
      }
      element->dragging = TRUE;
      element->drag_offset_x = cx - element->x;
      element->drag_offset_y = cy - element->y;
    }
  } else {
    data->connection_start = NULL;
    data->connection_start_point = -1;

    if (!(data->modifier_state & GDK_SHIFT_MASK)) {
      canvas_clear_selection(data);
    }

    data->selecting = TRUE;
    data->start_x = (int)x;
    data->start_y = (int)y;
    data->current_x = (int)x;
    data->current_y = (int)y;
  }

  gtk_widget_queue_draw(data->drawing_area);
}

static void canvas_process_motion(CanvasData *data, double x, double y) {
  data->last_mouse_x = x;
  data->last_mouse_y = y;

  int cx, cy;
  canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

  if (data->shape_mode) {
    canvas_set_cursor(data, data->draw_cursor);
  } else if (data->drawing_mode) {
    if (data->modifier_state & GDK_SHIFT_MASK) {
      canvas_set_cursor(data, data->line_cursor);
    } else {
      canvas_set_cursor(data, data->draw_cursor);
    }
  } else {
    canvas_update_cursor(data, (int)x, (int)y);
  }

  if (data->shape_mode && data->current_shape) {
    int x1 = data->shape_start_x;
    int y1 = data->shape_start_y;

    data->current_shape->base.x = MIN(x1, cx);
    data->current_shape->base.y = MIN(y1, cy);
    data->current_shape->base.width = MAX(ABS(cx - x1), 10);
    data->current_shape->base.height = MAX(ABS(cy - y1), 10);

    Shape *shape = data->current_shape;
    if ((shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW)) {
      double width = MAX(data->current_shape->base.width, 1);
      double height = MAX(data->current_shape->base.height, 1);

      double start_x = (double)data->shape_start_x;
      double start_y = (double)data->shape_start_y;
      double end_x = (double)cx;
      double end_y = (double)cy;

      double base_x = data->current_shape->base.x;
      double base_y = data->current_shape->base.y;

      shape->line_start_u = CLAMP((start_x - base_x) / width, 0.0, 1.0);
      shape->line_start_v = CLAMP((start_y - base_y) / height, 0.0, 1.0);
      shape->line_end_u = CLAMP((end_x - base_x) / width, 0.0, 1.0);
      shape->line_end_v = CLAMP((end_y - base_y) / height, 0.0, 1.0);
      shape->has_line_points = TRUE;
    } else if (shape->shape_type == SHAPE_BEZIER) {
      shape->has_bezier_points = TRUE;
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && data->current_drawing) {
    gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;

    if (is_straight_line) {
      if (data->current_drawing->points->len >= 2) {
        DrawingPoint *points = (DrawingPoint*)data->current_drawing->points->data;

        float rel_x = (float)(cx - data->current_drawing->base.x);
        float rel_y = (float)(cy - data->current_drawing->base.y);

        points[1].x = rel_x;
        points[1].y = rel_y;

        float min_x = MIN(points[0].x, rel_x);
        float min_y = MIN(points[0].y, rel_y);
        float max_x = MAX(points[0].x, rel_x);
        float max_y = MAX(points[0].y, rel_y);

        float padding = data->current_drawing->stroke_width / 2.0f;
        data->current_drawing->base.width = (int)(max_x - min_x + padding * 2);
        data->current_drawing->base.height = (int)(max_y - min_y + padding * 2);

        if (min_x < 0) {
          data->current_drawing->base.x += (int)min_x;
          for (guint i = 0; i < data->current_drawing->points->len; i++) {
            points[i].x -= min_x;
          }
          data->current_drawing->base.width += (int)(-min_x);
        }

        if (min_y < 0) {
          data->current_drawing->base.y += (int)min_y;
          for (guint i = 0; i < data->current_drawing->points->len; i++) {
            points[i].y -= min_y;
          }
          data->current_drawing->base.height += (int)(-min_y);
        }
      }
    } else {
      freehand_drawing_add_point(data->current_drawing, cx, cy);
    }

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && !data->current_drawing) {
    gtk_widget_queue_draw(data->drawing_area);
  }

  if (data->panning) {
    int dx = (int)x - data->pan_start_x;
    int dy = (int)y - data->pan_start_y;

    data->offset_x += dx;
    data->offset_y += dy;

    data->pan_start_x = (int)x;
    data->pan_start_y = (int)y;

    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->selected_elements) {
    for (GList *l = data->selected_elements; l != NULL; l = l->next) {
      Element *element = (Element*)l->data;

      if (element->rotating) {
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;

        double angle = atan2(cx - center_x, -(cy - center_y)) * 180.0 / M_PI;
        while (angle < 0) angle += 360.0;
        while (angle >= 360.0) angle -= 360.0;

        element->rotation_degrees = angle;

        gtk_widget_queue_draw(data->drawing_area);
        continue;
      }

      if (element->resizing) {
        int dx = cx - element->resize_start_x;
        int dy = cy - element->resize_start_y;

        double angle_rad = -element->rotation_degrees * M_PI / 180.0;
        double cos_a = cos(angle_rad);
        double sin_a = sin(angle_rad);

        double rotated_dx = dx * cos_a - dy * sin_a;
        double rotated_dy = dx * sin_a + dy * cos_a;

        int new_x = element->orig_x;
        int new_y = element->orig_y;
        int new_width = element->orig_width;
        int new_height = element->orig_height;

        switch (element->resize_edge) {
        case 0:
          new_width -= rotated_dx;
          new_height -= rotated_dy;
          new_x += rotated_dx * cos(-angle_rad) - rotated_dy * sin(-angle_rad);
          new_y += rotated_dx * sin(-angle_rad) + rotated_dy * cos(-angle_rad);
          break;
        case 1:
          new_width += rotated_dx;
          new_height -= rotated_dy;
          new_y += rotated_dx * sin(-angle_rad);
          break;
        case 2:
          new_width += rotated_dx;
          new_height += rotated_dy;
          break;
        case 3:
          new_width -= rotated_dx;
          new_height += rotated_dy;
          new_x += rotated_dx * cos(-angle_rad);
          break;
        }

        if (new_width < 50) new_width = 50;
        if (new_height < 30) new_height = 30;

        element->x = new_x;
        element->y = new_y;
        element->width = new_width;
        element->height = new_height;

        gtk_widget_queue_draw(data->drawing_area);
        return;
      }

      if (element->dragging) {
        int dx = cx - element->drag_offset_x - element->x;
        int dy = cy - element->drag_offset_y - element->y;

        for (GList *sel = data->selected_elements; sel != NULL; sel = sel->next) {
          Element *selected_element = (Element*)sel->data;
          selected_element->x += dx;
          selected_element->y += dy;
        }

        canvas_update_connections_for_selection(data);
        gtk_widget_queue_draw(data->drawing_area);
        return;
      }
    }
  }

  if (data->selecting) {
    data->current_x = (int)x;
    data->current_y = (int)y;
    gtk_widget_queue_draw(data->drawing_area);
  }
}

static void canvas_process_left_release(CanvasData *data, int n_press, double x, double y) {
  if (data->shape_mode && data->current_shape) {
    ElementConfig config = {0};
    Element* element = (Element*) data->current_shape;
    config.type = element->type;
    config.position.x = element->x;
    config.position.y = element->y;
    config.position.z = element->z;
    config.size.width = element->width;
    config.size.height = element->height;
    config.bg_color.r = element->bg_r;
    config.bg_color.g = element->bg_g;
    config.bg_color.b = element->bg_b;
    config.bg_color.a = element->bg_a;
    Shape *shape = (Shape*)element;
    config.text.text = shape->text;
    config.text.text_color.r = shape->text_r;
    config.text.text_color.g = shape->text_g;
    config.text.text_color.b = shape->text_b;
    config.text.text_color.a = shape->text_a;
    config.text.font_description = shape->font_description;
    config.shape.shape_type = shape->shape_type;
    config.shape.stroke_width = shape->stroke_width;
    config.shape.filled = shape->filled;
    config.shape.stroke_style = shape->stroke_style;
    config.shape.fill_style = shape->fill_style;
    config.shape.stroke_color.r = shape->stroke_r;
    config.shape.stroke_color.g = shape->stroke_g;
    config.shape.stroke_color.b = shape->stroke_b;
    config.shape.stroke_color.a = shape->stroke_a;
    ElementDrawing drawing = { .drawing_points = NULL, .stroke_width = shape->stroke_width };

    GArray *line_points = NULL;
    if ((shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW) && shape->has_line_points) {
      line_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 2);

      DrawingPoint start_point;
      graphene_point_init(&start_point, (float)shape->line_start_u, (float)shape->line_start_v);
      g_array_append_val(line_points, start_point);

      DrawingPoint end_point;
      graphene_point_init(&end_point, (float)shape->line_end_u, (float)shape->line_end_v);
      g_array_append_val(line_points, end_point);

      drawing.drawing_points = line_points;
    } else if (shape->shape_type == SHAPE_BEZIER && shape->has_bezier_points) {
      line_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 4);

      DrawingPoint p0;
      graphene_point_init(&p0, (float)shape->bezier_p0_u, (float)shape->bezier_p0_v);
      g_array_append_val(line_points, p0);

      DrawingPoint p1;
      graphene_point_init(&p1, (float)shape->bezier_p1_u, (float)shape->bezier_p1_v);
      g_array_append_val(line_points, p1);

      DrawingPoint p2;
      graphene_point_init(&p2, (float)shape->bezier_p2_u, (float)shape->bezier_p2_v);
      g_array_append_val(line_points, p2);

      DrawingPoint p3;
      graphene_point_init(&p3, (float)shape->bezier_p3_u, (float)shape->bezier_p3_v);
      g_array_append_val(line_points, p3);

      drawing.drawing_points = line_points;
    }

    config.drawing = drawing;

    ModelElement *model_element = model_create_element(data->model, config);

    if (model_element) {
      model_element->visual_element = create_visual_element(model_element, data);
      undo_manager_push_create_action(data->undo_manager, model_element);
    }

    if (line_points) {
      g_array_free(line_points, TRUE);
    }

    shape_free((Element*)data->current_shape);
    data->current_shape = NULL;
    data->shape_mode = FALSE;
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->drawing_mode && !data->shape_mode && data->current_drawing) {
    int cx, cy;
    canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);

    gboolean is_straight_line = (data->modifier_state & GDK_SHIFT_MASK) != 0;

    if (is_straight_line) {
      if (data->current_drawing->points->len >= 2) {
        DrawingPoint *points = (DrawingPoint*)data->current_drawing->points->data;

        float rel_x = (float)(cx - data->current_drawing->base.x);
        float rel_y = (float)(cy - data->current_drawing->base.y);

        points[1].x = rel_x;
        points[1].y = rel_y;
      }
    } else {
      freehand_drawing_add_point(data->current_drawing, cx, cy);
    }
    ElementPosition position = {
      .x = data->current_drawing->base.x,
      .y = data->current_drawing->base.y,
      .z = data->next_z_index++,
    };
    ElementColor bg_color = {
      .r = data->current_drawing->base.bg_r,
      .g = data->current_drawing->base.bg_g,
      .b = data->current_drawing->base.bg_b,
      .a = data->current_drawing->base.bg_a,
    };
    ElementSize size = {
      .width = data->current_drawing->base.width,
      .height = data->current_drawing->base.height
    };
    ElementColor text_color = { .r = 0, .g = 0, .b = 0, .a = 0 };
    ElementMedia media = { .type = MEDIA_TYPE_NONE, .image_data = NULL, .image_size = 0, .video_data = NULL, .video_size = 0, .duration = 0 };
    ElementConnection connection = { .from_element = NULL };
    ElementText text = {
      .text = NULL,
      .text_color = text_color,
      .font_description = NULL,
    };
    ElementConfig config = {
      .type = ELEMENT_FREEHAND_DRAWING,
      .bg_color = bg_color,
      .position = position,
      .size = size,
      .media = media,
      .drawing.drawing_points = data->current_drawing->points,
      .drawing.stroke_width = data->current_drawing->stroke_width,
      .connection = connection,
      .text = text,
    };

    ModelElement *model_element = model_create_element(data->model, config);

    if (!model_element) {
      g_printerr("Failed to create drawing element\n");
      return;
    }
    model_element->visual_element = create_visual_element(model_element, data);
    undo_manager_push_create_action(data->undo_manager, model_element);
    data->current_drawing = NULL;
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if (data->selecting) {
    data->selecting = FALSE;

    int start_cx, start_cy, current_cx, current_cy;
    canvas_screen_to_canvas(data, data->start_x, data->start_y, &start_cx, &start_cy);
    canvas_screen_to_canvas(data, data->current_x, data->current_y, &current_cx, &current_cy);

    int sel_x = MIN(start_cx, current_cx);
    int sel_y = MIN(start_cy, current_cy);
    int sel_width = ABS(current_cx - start_cx);
    int sel_height = ABS(current_cy - start_cy);

    GList *visual_elements = canvas_get_visual_elements(data);

    for (GList *iter = visual_elements; iter != NULL; iter = iter->next) {
      Element *element = (Element*)iter->data;

      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && model_element->locked) {
        continue;
      }

      if (element->x + element->width >= sel_x &&
          element->x <= sel_x + sel_width &&
          element->y + element->height >= sel_y &&
          element->y <= sel_y + sel_height) {
        if (!canvas_is_element_selected(data, element)) {
          data->selected_elements = g_list_append(data->selected_elements, element);
        }
      }
    }
    g_list_free(visual_elements);
  }

  gboolean was_moved = FALSE;
  if (data->drag_start_positions && g_hash_table_size(data->drag_start_positions) > 0) {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, data->drag_start_positions);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
      ModelElement *model_element = (ModelElement*)key;
      PositionData *start_pos = (PositionData*)value;

      Element *visual_element = model_element->visual_element;

      if (visual_element && model_element->position &&
          (visual_element->x != start_pos->x ||
           visual_element->y != start_pos->y)) {

        was_moved = TRUE;

        model_update_position(data->model, model_element,
                              visual_element->x, visual_element->y,
                              model_element->position->z);

        undo_manager_push_move_action(data->undo_manager, model_element,
                                      start_pos->x, start_pos->y,
                                      visual_element->x, visual_element->y);
      }

      g_free(start_pos);
    }

    g_hash_table_remove_all(data->drag_start_positions);
  }

  gboolean was_resized = FALSE;
  gboolean was_rotated = FALSE;
  GList *visual_elements = canvas_get_visual_elements(data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    if (element->resizing) {
      was_resized = TRUE;

      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && model_element->size) {
        undo_manager_push_resize_action(data->undo_manager, model_element,
                                        element->orig_width, element->orig_height,
                                        element->width, element->height);

        model_update_size(data->model, model_element, element->width, element->height);
      }
    }
    if (element->rotating) {
      was_rotated = TRUE;

      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element) {
        undo_manager_push_rotate_action(data->undo_manager, model_element,
                                        element->orig_rotation,
                                        element->rotation_degrees);

        model_update_rotation(data->model, model_element, element->rotation_degrees);
      }
    }
    if (element->type == ELEMENT_SHAPE) {
      Shape *shape = (Shape*)element;
      if (shape->dragging_control_point && shape->shape_type == SHAPE_BEZIER) {
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element && shape->has_bezier_points) {
          GArray *bezier_points = g_array_sized_new(FALSE, FALSE, sizeof(DrawingPoint), 4);

          DrawingPoint p0; graphene_point_init(&p0, (float)shape->bezier_p0_u, (float)shape->bezier_p0_v); g_array_append_val(bezier_points, p0);
          DrawingPoint p1; graphene_point_init(&p1, (float)shape->bezier_p1_u, (float)shape->bezier_p1_v); g_array_append_val(bezier_points, p1);
          DrawingPoint p2; graphene_point_init(&p2, (float)shape->bezier_p2_u, (float)shape->bezier_p2_v); g_array_append_val(bezier_points, p2);
          DrawingPoint p3; graphene_point_init(&p3, (float)shape->bezier_p3_u, (float)shape->bezier_p3_v); g_array_append_val(bezier_points, p3);

          if (model_element->drawing_points) {
            g_array_free(model_element->drawing_points, TRUE);
          }
          model_element->drawing_points = bezier_points;
        }

        shape->dragging_control_point = FALSE;
        shape->dragging_control_point_index = -1;
      }
    }

    element->dragging = FALSE;
    element->resizing = FALSE;
    element->rotating = FALSE;
  }

  g_list_free(visual_elements);

  if (was_moved || was_resized || was_rotated) {
    canvas_sync_with_model(data);
  }

  gtk_widget_queue_draw(data->drawing_area);
}

static void canvas_process_right_release(CanvasData *data, int n_press, double x, double y) {
  (void)n_press;
  (void)x;
  (void)y;

  if (data->panning) {
    data->panning = FALSE;
    canvas_set_cursor(data, data->default_cursor);
  }
}

static void canvas_process_leave(CanvasData *data) {
  canvas_set_cursor(data, data->default_cursor);
}

static void canvas_process_right_click(CanvasData *data, int n_press, double x, double y) {
  if (n_press != 1) {
    return;
  }

  int cx, cy;
  canvas_screen_to_canvas(data, (int)x, (int)y, &cx, &cy);
  Element *element = canvas_pick_element_including_locked(data, cx, cy);

  if (!element) {
    data->panning = TRUE;
    data->pan_start_x = (int)x;
    data->pan_start_y = (int)y;
    canvas_set_cursor(data, data->move_cursor);
    return;
  }

  ModelElement *model_element = model_get_by_visual(data->model, element);

  if (!model_element) {
    return;
  }

  GSimpleActionGroup *action_group = g_simple_action_group_new();

  GSimpleAction *delete_action = g_simple_action_new("delete", NULL);
  g_object_set_data(G_OBJECT(delete_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(delete_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(delete_action, "activate", G_CALLBACK(on_delete_element_action), NULL);

  GSimpleAction *description_action = g_simple_action_new("description", NULL);
  g_object_set_data(G_OBJECT(description_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(description_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(description_action, "activate", G_CALLBACK(on_description_action), NULL);

  GSimpleAction *lock_unlock_action = g_simple_action_new("lock-unlock", NULL);
  g_object_set_data(G_OBJECT(lock_unlock_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(lock_unlock_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(lock_unlock_action, "activate", G_CALLBACK(on_lock_unlock_element_action), NULL);

  GSimpleAction *change_color_action = g_simple_action_new("change-color", NULL);
  g_object_set_data(G_OBJECT(change_color_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(change_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(change_color_action, "activate", G_CALLBACK(on_change_color_action), NULL);

  GSimpleAction *clone_action = g_simple_action_new("clone", NULL);
  g_object_set_data(G_OBJECT(clone_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(clone_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(clone_action, "activate", G_CALLBACK(on_clone_action), NULL);

  GSimpleAction *change_space_action = g_simple_action_new("change-space", NULL);
  g_object_set_data(G_OBJECT(change_space_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(change_space_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(change_space_action, "activate", G_CALLBACK(on_change_space_action), NULL);

  GSimpleAction *change_text_action = g_simple_action_new("change-text", NULL);
  g_object_set_data(G_OBJECT(change_text_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(change_text_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(change_text_action, "activate", G_CALLBACK(on_change_text_action), NULL);

  GSimpleAction *hide_children_action = g_simple_action_new("hide-children", NULL);
  g_object_set_data(G_OBJECT(hide_children_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(hide_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(hide_children_action, "activate", G_CALLBACK(on_hide_children_action), NULL);

  GSimpleAction *show_children_action = g_simple_action_new("show-children", NULL);
  g_object_set_data(G_OBJECT(show_children_action), "canvas_data", data);
  g_object_set_data_full(G_OBJECT(show_children_action), "element_uuid", g_strdup(model_element->uuid), g_free);
  g_signal_connect(show_children_action, "activate", G_CALLBACK(on_show_children_action), NULL);

  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(delete_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(description_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(lock_unlock_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(clone_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_color_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_space_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_text_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(hide_children_action));
  g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(show_children_action));

  if (element->type == ELEMENT_SHAPE) {
    GSimpleAction *change_shape_style_action = g_simple_action_new("change-shape-style", NULL);
    g_object_set_data(G_OBJECT(change_shape_style_action), "canvas_data", data);
    g_object_set_data_full(G_OBJECT(change_shape_style_action), "element_uuid", g_strdup(model_element->uuid), g_free);
    g_signal_connect(change_shape_style_action, "activate", G_CALLBACK(on_change_shape_style_action), NULL);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_shape_style_action));

    GSimpleAction *change_shape_stroke_color_action = g_simple_action_new("change-shape-stroke-color", NULL);
    g_object_set_data(G_OBJECT(change_shape_stroke_color_action), "canvas_data", data);
    g_object_set_data_full(G_OBJECT(change_shape_stroke_color_action), "element_uuid", g_strdup(model_element->uuid), g_free);
    g_signal_connect(change_shape_stroke_color_action, "activate", G_CALLBACK(on_change_shape_stroke_color_action), NULL);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_shape_stroke_color_action));
  }

  if (element->type == ELEMENT_CONNECTION) {
    GSimpleAction *change_arrow_type_action = g_simple_action_new("change-arrow-type", NULL);
    g_object_set_data(G_OBJECT(change_arrow_type_action), "canvas_data", data);
    g_object_set_data_full(G_OBJECT(change_arrow_type_action), "element_uuid", g_strdup(model_element->uuid), g_free);
    g_signal_connect(change_arrow_type_action, "activate", G_CALLBACK(on_change_arrow_type_action), NULL);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_arrow_type_action));

    GSimpleAction *change_arrowhead_type_action = g_simple_action_new("change-arrowhead-type", NULL);
    g_object_set_data(G_OBJECT(change_arrowhead_type_action), "canvas_data", data);
    g_object_set_data_full(G_OBJECT(change_arrowhead_type_action), "element_uuid", g_strdup(model_element->uuid), g_free);
    g_signal_connect(change_arrowhead_type_action, "activate", G_CALLBACK(on_change_arrowhead_type_action), NULL);
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(change_arrowhead_type_action));
  }

  GMenu *menu_model = g_menu_new();
  GMenu *modify_section = g_menu_new();
  GMenu *structure_section = g_menu_new();
  GMenu *clone_section = g_menu_new();
  GMenu *info_section = g_menu_new();
  GMenu *danger_section = g_menu_new();

  g_menu_append(modify_section, "Change Space", "menu.change-space");

  if (element->type == ELEMENT_NOTE || element->type == ELEMENT_PAPER_NOTE  ||
      element->type == ELEMENT_SPACE || element->type == ELEMENT_MEDIA_FILE ||
      element->type == ELEMENT_SHAPE || element->type == ELEMENT_INLINE_TEXT) {
    g_menu_append(modify_section, "Change Text", "menu.change-text");
  }

  gboolean show_bg_color = TRUE;
  if (element->type == ELEMENT_SHAPE) {
    Shape *shape = (Shape *)element;
    if (!shape->filled ||
        shape->shape_type == SHAPE_LINE ||
        shape->shape_type == SHAPE_ARROW ||
        shape->shape_type == SHAPE_BEZIER) {
      show_bg_color = FALSE;
    }
  }
  if (show_bg_color) {
    g_menu_append(modify_section, "Change Color", "menu.change-color");
  }

  if (element->type == ELEMENT_SHAPE) {
    g_menu_append(modify_section, "Change Shape Style", "menu.change-shape-style");
    g_menu_append(modify_section, "Change Stroke Color", "menu.change-shape-stroke-color");
  }

  g_menu_append(structure_section, "Hide Children", "menu.hide-children");
  g_menu_append(structure_section, "Show Children", "menu.show-children");

  g_menu_append(clone_section, "Clone", "menu.clone");

  g_menu_append(info_section, "Edit Description", "menu.description");

  g_menu_append(danger_section, "Delete", "menu.delete");

  g_menu_append_section(menu_model, NULL, G_MENU_MODEL(modify_section));
  g_menu_append_section(menu_model, NULL, G_MENU_MODEL(structure_section));
  g_menu_append_section(menu_model, NULL, G_MENU_MODEL(clone_section));
  g_menu_append_section(menu_model, NULL, G_MENU_MODEL(info_section));
  g_menu_append_section(menu_model, NULL, G_MENU_MODEL(danger_section));

  GtkWidget *menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu_model));
  g_signal_connect(menu, "closed", G_CALLBACK(on_popover_closed), NULL);
  gtk_widget_insert_action_group(menu, "menu", G_ACTION_GROUP(action_group));
  gtk_popover_set_has_arrow(GTK_POPOVER(menu), FALSE);
  gtk_widget_set_parent(menu, data->overlay);
  gtk_popover_set_pointing_to(GTK_POPOVER(menu), &(GdkRectangle){ (int)x, (int)y, 1, 1 });
  gtk_widget_show(menu);

  g_object_unref(menu_model);
  g_object_unref(action_group);
}

static void canvas_process_key_press(CanvasData *data, guint keyval,
                                     guint keycode, GdkModifierType state) {
  (void)keycode;

  GList *visual_elements = canvas_get_visual_elements(data);
  for (GList *l = visual_elements; l != NULL; l = l->next) {
    Element *element = (Element*)l->data;
    if (element->type == ELEMENT_INLINE_TEXT && ((InlineText*)element)->editing) {
      g_list_free(visual_elements);
      return;
    }
  }
  g_list_free(visual_elements);

  if (!data->selected_elements &&
      !(state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) &&
      keyval >= 0x20 && keyval <= 0x7E) {
    canvas_on_add_text(NULL, data);
    return;
  }

  if (keyval == GDK_KEY_F1 && (state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) == 0) {
    canvas_show_shortcuts_dialog(data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_c) {
    if (data->selected_elements) {
      if (data->copied_elements) {
        g_list_free(data->copied_elements);
        data->copied_elements = NULL;
      }
      for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element) {
          data->copied_elements = g_list_append(data->copied_elements, model_element);
        }
      }
      int count = g_list_length(data->copied_elements);
      char message[64];
      snprintf(message, sizeof(message), "%d element%s copied", count, count == 1 ? "" : "s");
      canvas_show_notification(data, message);
    }
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_v) {
    canvas_on_paste(data->drawing_area, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_s) {
    canvas_show_search_dialog(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_N) {
    canvas_on_add_note(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_n) {
    canvas_on_add_text(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_e) {
    canvas_show_script_dialog(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_d) {
    canvas_toggle_drawing_mode(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_l) {
    canvas_show_shape_selection_dialog(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_P) {
    canvas_on_add_paper_note(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_S) {
    canvas_on_add_space(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_t) {
    extern void toggle_toolbar_visibility(CanvasData *data);
    toggle_toolbar_visibility(data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_T) {
    extern void toggle_toolbar_auto_hide(CanvasData *data);
    toggle_toolbar_auto_hide(data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_j) {
    if (data->tree_toggle_button) {
      gboolean is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->tree_toggle_button));
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->tree_toggle_button), !is_active);
    }
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_z) {
    on_undo_clicked(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_y) {
    on_redo_clicked(NULL, data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_a || keyval == GDK_KEY_A)) {
    canvas_clear_selection(data);
    GList *elements = canvas_get_visual_elements(data);
    for (GList *l = elements; l != NULL; l = l->next) {
      Element *element = (Element*)l->data;
      ModelElement *model_element = model_get_by_visual(data->model, element);
      if (model_element && !canvas_is_element_hidden(data, model_element->uuid)) {
        data->selected_elements = g_list_append(data->selected_elements, element);
      }
    }
    g_list_free(elements);
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_plus || keyval == GDK_KEY_equal)) {
    if (data->selected_elements) {
      for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        if (element->type == ELEMENT_INLINE_TEXT) {
          InlineText *text = (InlineText*)element;
          PangoFontDescription *font_desc = pango_font_description_from_string(text->font_description);
          int current_size = pango_font_description_get_size(font_desc) / PANGO_SCALE;
          int new_size = MIN(current_size + 2, 72);
          pango_font_description_set_size(font_desc, new_size * PANGO_SCALE);
          char *new_font_desc = pango_font_description_to_string(font_desc);
          g_free(text->font_description);
          text->font_description = g_strdup(new_font_desc);
          g_free(new_font_desc);
          pango_font_description_free(font_desc);

          inline_text_update_layout(text);

          ModelElement *model_element = model_get_by_visual(data->model, element);
          if (model_element) {
            model_update_font(data->model, model_element, text->font_description);
            model_update_size(data->model, model_element, text->base.width, text->base.height);
          }
        }
      }
      gtk_widget_queue_draw(data->drawing_area);
    }
    return;
  }

  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_minus || keyval == GDK_KEY_underscore)) {
    if (data->selected_elements) {
      for (GList *l = data->selected_elements; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        if (element->type == ELEMENT_INLINE_TEXT) {
          InlineText *text = (InlineText*)element;
          PangoFontDescription *font_desc = pango_font_description_from_string(text->font_description);
          int current_size = pango_font_description_get_size(font_desc) / PANGO_SCALE;
          int new_size = MAX(current_size - 2, 6);
          pango_font_description_set_size(font_desc, new_size * PANGO_SCALE);
          char *new_font_desc = pango_font_description_to_string(font_desc);
          g_free(text->font_description);
          text->font_description = g_strdup(new_font_desc);
          g_free(new_font_desc);
          pango_font_description_free(font_desc);

          inline_text_update_layout(text);

          ModelElement *model_element = model_get_by_visual(data->model, element);
          if (model_element) {
            model_update_font(data->model, model_element, text->font_description);
            model_update_size(data->model, model_element, text->base.width, text->base.height);
          }
        }
      }
      gtk_widget_queue_draw(data->drawing_area);
    }
    return;
  }

  if (keyval == GDK_KEY_Delete) {
    if (data->selected_elements) {
      GList *elements_to_delete = g_list_copy(data->selected_elements);
      for (GList *l = elements_to_delete; l != NULL; l = l->next) {
        Element *element = (Element*)l->data;
        ModelElement *model_element = model_get_by_visual(data->model, element);
        if (model_element) {
          if (model_element->type->type == ELEMENT_SPACE) {
            if (model_element->state != MODEL_STATE_NEW) {
              int element_count = model_get_amount_of_elements(data->model, model_element->target_space_uuid);
              if (element_count > 0) {
                char message[128];
                snprintf(message, sizeof(message),
                         "Cannot delete space with %d element%s",
                         element_count, element_count == 1 ? "" : "s");
                canvas_show_notification(data, message);
                continue;
              }
            }
          }

          undo_manager_push_delete_action(data->undo_manager, model_element);
          model_delete_element(data->model, model_element);
        }
      }
      g_list_free(elements_to_delete);
      canvas_sync_with_model(data);
      canvas_clear_selection(data);
      gtk_widget_queue_draw(data->drawing_area);
    }
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_Right) {
    extern void canvas_presentation_next_slide(CanvasData *data);
    canvas_presentation_next_slide(data);
    return;
  }

  if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_Left) {
    extern void canvas_presentation_prev_slide(CanvasData *data);
    canvas_presentation_prev_slide(data);
    return;
  }

  if (keyval == GDK_KEY_BackSpace) {
    go_back_to_parent_space(data);
    return;
  }
}

static gboolean canvas_process_scroll(CanvasData *data, double dx, double dy) {
  (void)dx;

  const double zoom_speed = 0.1;
  double zoom_factor = 1.0;

  if (dy > 0) {
    zoom_factor = 1.0 - zoom_speed;
  } else if (dy < 0) {
    zoom_factor = 1.0 + zoom_speed;
  }

  double new_zoom = data->zoom_scale * zoom_factor;
  if (new_zoom < 0.1) new_zoom = 0.1;
  if (new_zoom > 10.0) new_zoom = 10.0;

  if (new_zoom != data->zoom_scale) {
    double cursor_x = data->last_mouse_x;
    double cursor_y = data->last_mouse_y;

    int canvas_point_x, canvas_point_y;
    canvas_screen_to_canvas(data, (int)cursor_x, (int)cursor_y, &canvas_point_x, &canvas_point_y);

    data->zoom_scale = new_zoom;

    data->offset_x = (cursor_x / new_zoom) - canvas_point_x;
    data->offset_y = (cursor_y / new_zoom) - canvas_point_y;

    canvas_update_zoom_entry(data);
    gtk_widget_queue_draw(data->drawing_area);
  }

  return TRUE;
}

static gboolean canvas_handle_left_press(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  data->modifier_state = event->data.pointer.modifiers;
  canvas_process_left_click(data,
                            event->data.pointer.n_press,
                            event->data.pointer.x,
                            event->data.pointer.y);
  return TRUE;
}

static gboolean canvas_handle_left_release(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  data->modifier_state = event->data.pointer.modifiers;
  canvas_process_left_release(data,
                              event->data.pointer.n_press,
                              event->data.pointer.x,
                              event->data.pointer.y);
  return TRUE;
}

static gboolean canvas_handle_right_press(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  data->modifier_state = event->data.pointer.modifiers;
  canvas_process_right_click(data,
                             event->data.pointer.n_press,
                             event->data.pointer.x,
                             event->data.pointer.y);
  return TRUE;
}

static gboolean canvas_handle_right_release(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  data->modifier_state = event->data.pointer.modifiers;
  canvas_process_right_release(data,
                               event->data.pointer.n_press,
                               event->data.pointer.x,
                               event->data.pointer.y);
  return TRUE;
}

static gboolean canvas_handle_motion(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  data->modifier_state = event->data.pointer.modifiers;
  canvas_process_motion(data, event->data.pointer.x, event->data.pointer.y);
  return TRUE;
}

static gboolean canvas_handle_leave(const UIEvent *event, gpointer user_data) {
  (void)event;
  CanvasData *data = (CanvasData*)user_data;
  if (!data) {
    return FALSE;
  }

  canvas_process_leave(data);
  return TRUE;
}

static gboolean canvas_handle_key_press(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  canvas_process_key_press(data,
                           event->data.key.keyval,
                           event->data.key.keycode,
                           event->data.key.modifiers);
  return TRUE;
}

static gboolean canvas_handle_scroll(const UIEvent *event, gpointer user_data) {
  CanvasData *data = (CanvasData*)user_data;
  if (!data || !event) {
    return FALSE;
  }

  return canvas_process_scroll(data,
                               event->data.scroll.dx,
                               event->data.scroll.dy);
}

void canvas_input_register_event_handlers(CanvasData *data) {
  if (!data) {
    return;
  }

  ui_event_bus_init();
  canvas_input_unregister_event_handlers(data);

  data->ui_event_subscriptions[UI_EVENT_POINTER_PRIMARY_PRESS] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_PRIMARY_PRESS, canvas_handle_left_press, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_POINTER_PRIMARY_RELEASE] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_PRIMARY_RELEASE, canvas_handle_left_release, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_POINTER_SECONDARY_PRESS] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_SECONDARY_PRESS, canvas_handle_right_press, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_POINTER_SECONDARY_RELEASE] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_SECONDARY_RELEASE, canvas_handle_right_release, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_POINTER_MOTION] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_MOTION, canvas_handle_motion, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_POINTER_LEAVE] =
    ui_event_bus_subscribe(UI_EVENT_POINTER_LEAVE, canvas_handle_leave, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_SCROLL] =
    ui_event_bus_subscribe(UI_EVENT_SCROLL, canvas_handle_scroll, data, NULL);
  data->ui_event_subscriptions[UI_EVENT_KEY_PRESS] =
    ui_event_bus_subscribe(UI_EVENT_KEY_PRESS, canvas_handle_key_press, data, NULL);
}

void canvas_input_unregister_event_handlers(CanvasData *data) {
  if (!data) {
    return;
  }

  for (int i = 0; i < UI_EVENT_TYPE_COUNT; i++) {
    guint subscription_id = data->ui_event_subscriptions[i];
    if (subscription_id != 0) {
      ui_event_bus_unsubscribe(subscription_id);
      data->ui_event_subscriptions[i] = 0;
    }
  }
}
