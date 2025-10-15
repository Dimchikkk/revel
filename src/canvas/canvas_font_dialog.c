#include "canvas_font_dialog.h"
#include "canvas/canvas.h"
#include "canvas/canvas_core.h"
#include "elements/element.h"
#include "model.h"
#include "elements/note.h"
#include "elements/paper_note.h"
#include "elements/media_note.h"
#include "elements/space.h"
#include "elements/shape.h"
#include "elements/inline_text.h"
#include <pango/pango.h>
#include <pango/pangocairo.h>

typedef struct {
  Element *element;
  char *element_uuid;
  GtkWidget *dialog;
  GtkWidget *font_combo;
  GtkWidget *size_spin;
  GtkWidget *bold_check;
  GtkWidget *italic_check;
  GtkWidget *strikethrough_check;
  GtkWidget *color_button;
  GtkWidget *alignment_combo;
  char *original_font_desc;
  double original_r, original_g, original_b, original_a;
  char *original_alignment;
} FontDialogData;

char* get_font_family_from_desc(const char *font_desc) {
  PangoFontDescription *desc = pango_font_description_from_string(font_desc);
  const char *family = pango_font_description_get_family(desc);
  char *result = family ? g_strdup(family) : g_strdup("Ubuntu Mono");
  pango_font_description_free(desc);
  return result;
}

int get_font_size_from_desc(const char *font_desc) {
  PangoFontDescription *desc = pango_font_description_from_string(font_desc);
  int size = pango_font_description_get_size(desc) / PANGO_SCALE;
  pango_font_description_free(desc);
  return size > 0 ? size : 12;
}

gboolean is_font_bold(const char *font_desc) {
  PangoFontDescription *desc = pango_font_description_from_string(font_desc);
  PangoWeight weight = pango_font_description_get_weight(desc);
  pango_font_description_free(desc);
  return weight >= PANGO_WEIGHT_BOLD;
}

gboolean is_font_italic(const char *font_desc) {
  PangoFontDescription *desc = pango_font_description_from_string(font_desc);
  PangoStyle style = pango_font_description_get_style(desc);
  pango_font_description_free(desc);
  return style == PANGO_STYLE_ITALIC;
}

char* create_font_description_string(const char *family, int size,
                                     gboolean bold, gboolean italic, gboolean strikethrough) {
  PangoFontDescription *desc = pango_font_description_new();

  pango_font_description_set_family(desc, family);
  pango_font_description_set_size(desc, size * PANGO_SCALE);
  pango_font_description_set_weight(desc, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
  pango_font_description_set_style(desc, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
  // Note: strikethrough parameter is ignored here as it's not part of PangoFontDescription
  // It must be stored separately in ModelText and applied as a Pango attribute during rendering

  char *result = pango_font_description_to_string(desc);
  pango_font_description_free(desc);

  return result;
}

static void font_dialog_data_free(FontDialogData *data) {
  if (data->original_font_desc) {
    g_free(data->original_font_desc);
  }
  if (data->element_uuid) {
    g_free(data->element_uuid);
  }
  if (data->original_alignment) {
    g_free(data->original_alignment);
  }
  g_free(data);
}

static void update_font_and_color(char **font_description,
                                  char *new_font_desc,
                                  double *r, double *g, double *b, double *a,
                                  const GdkRGBA *color) {
  g_free(*font_description);
  *font_description = new_font_desc;
  *r = color->red;
  *g = color->green;
  *b = color->blue;
  *a = color->alpha;
}

static void update_visual_element(FontDialogData *data) {
  gchar *font_family = gtk_combo_box_text_get_active_text(
                                                          GTK_COMBO_BOX_TEXT(data->font_combo));

  gdouble font_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->size_spin));

  gboolean bold = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->bold_check));
  gboolean italic = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->italic_check));
  gboolean strikethrough = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->strikethrough_check));

  char *new_font_desc = create_font_description_string(font_family, font_size, bold, italic, strikethrough);

  GdkRGBA new_color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(data->color_button), &new_color);

  // Get selected alignment (if alignment combo exists)
  const char *new_alignment = NULL;
  if (data->alignment_combo) {
    guint alignment_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(data->alignment_combo));
    const char *alignment_values[] = {"top-left", "top-center", "top-right", "center", "bottom-left", "bottom-right"};
    new_alignment = alignment_values[alignment_index];
  }

  // Update visual element immediately
  switch (data->element->type) {
  case ELEMENT_NOTE: {
    Note* el = (Note*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    if (new_alignment) {
      g_free(el->alignment);
      el->alignment = g_strdup(new_alignment);
    }
    break;
  }
  case ELEMENT_PAPER_NOTE: {
    PaperNote* el = (PaperNote*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    if (new_alignment) {
      g_free(el->alignment);
      el->alignment = g_strdup(new_alignment);
    }
    break;
  }
  case ELEMENT_SPACE: {
    SpaceElement* el = (SpaceElement*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    if (new_alignment) {
      g_free(el->alignment);
      el->alignment = g_strdup(new_alignment);
    }
    break;
  }
  case ELEMENT_MEDIA_FILE: {
    MediaNote* el = (MediaNote*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    if (new_alignment) {
      g_free(el->alignment);
      el->alignment = g_strdup(new_alignment);
    }
    break;
  }
  case ELEMENT_SHAPE: {
    Shape* el = (Shape*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    if (new_alignment) {
      g_free(el->alignment);
      el->alignment = g_strdup(new_alignment);
    }
    break;
  }
  case ELEMENT_INLINE_TEXT: {
    InlineText* el = (InlineText*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    el->strikethrough = strikethrough;
    break;
  }
  case ELEMENT_CONNECTION:
  case ELEMENT_FREEHAND_DRAWING:
    g_free(new_font_desc);
    break;
  }

  g_free(font_family);
  gtk_widget_queue_draw(data->element->canvas_data->drawing_area);
}

static void apply_font_changes(FontDialogData *data) {
  // Update model with final changes
  gchar *font_family = gtk_combo_box_text_get_active_text(
                                                          GTK_COMBO_BOX_TEXT(data->font_combo));

  gdouble font_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->size_spin));

  gboolean bold = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->bold_check));
  gboolean italic = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->italic_check));
  gboolean strikethrough = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->strikethrough_check));

  char *new_font_desc = create_font_description_string(font_family, font_size, bold, italic, strikethrough);

  GdkRGBA new_color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(data->color_button), &new_color);

  Model* model = data->element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, data->element);

  // Update model (persistent storage)
  model_update_text_color(model, model_element, new_color.red, new_color.green, new_color.blue, new_color.alpha);
  model_update_font(model, model_element, new_font_desc);
  model_update_strikethrough(model, model_element, strikethrough);

  // Get selected alignment (if alignment combo exists)
  if (data->alignment_combo) {
    guint alignment_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(data->alignment_combo));
    const char *alignment_values[] = {"top-left", "top-center", "top-right", "center", "bottom-left", "bottom-right"};
    const char *new_alignment = alignment_values[alignment_index];
    model_update_text_alignment(model, model_element, new_alignment);
  }

  canvas_sync_with_model(data->element->canvas_data);
  gtk_widget_queue_draw(data->element->canvas_data->drawing_area);

  g_free(font_family);
  g_free(new_font_desc);
}

static void revert_visual_changes(FontDialogData *data) {
  // Revert visual element to original state
  GdkRGBA original_color = {
    .red = data->original_r,
    .green = data->original_g,
    .blue = data->original_b,
    .alpha = data->original_a,
  };

  switch (data->element->type) {
  case ELEMENT_NOTE: {
    Note* el = (Note*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    g_free(el->alignment);
    el->alignment = g_strdup(data->original_alignment);
    break;
  }
  case ELEMENT_PAPER_NOTE: {
    PaperNote* el = (PaperNote*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    g_free(el->alignment);
    el->alignment = g_strdup(data->original_alignment);
    break;
  }
  case ELEMENT_SPACE: {
    SpaceElement* el = (SpaceElement*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    g_free(el->alignment);
    el->alignment = g_strdup(data->original_alignment);
    break;
  }
  case ELEMENT_MEDIA_FILE: {
    MediaNote* el = (MediaNote*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    g_free(el->alignment);
    el->alignment = g_strdup(data->original_alignment);
    break;
  }
  case ELEMENT_SHAPE: {
    Shape* el = (Shape*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    g_free(el->alignment);
    el->alignment = g_strdup(data->original_alignment);
    break;
  }
  case ELEMENT_INLINE_TEXT: {
    InlineText* el = (InlineText*)data->element;
    update_font_and_color(&el->font_description, g_strdup(data->original_font_desc),
                         &el->text_r, &el->text_g, &el->text_b, &el->text_a, &original_color);
    break;
  }
  case ELEMENT_CONNECTION:
  case ELEMENT_FREEHAND_DRAWING:
    break;
  }

  gtk_widget_queue_draw(data->element->canvas_data->drawing_area);
}

static void on_font_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  FontDialogData *data = (FontDialogData *)user_data;

  if (response_id == GTK_RESPONSE_OK) {
    apply_font_changes(data); // Save changes to model
  } else {
    revert_visual_changes(data); // Revert visual to original state
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
  font_dialog_data_free(data);
}

static void copy_original_font_and_color(FontDialogData *data,
                                         char *font_description,
                                         double text_r,
                                         double text_g,
                                         double text_b,
                                         double text_a)
{
  data->original_font_desc = g_strdup(font_description);
  data->original_r = text_r;
  data->original_g = text_g;
  data->original_b = text_b;
  data->original_a = text_a;
}

static void on_font_selection_changed(GtkWidget *widget, FontDialogData *data) {
  update_visual_element(data); // Update visual element immediately on change
}

static void on_alignment_changed(GtkDropDown *dropdown, GParamSpec *pspec, FontDialogData *data) {
  update_visual_element(data); // Update visual element immediately on alignment change
}

void font_dialog_open(CanvasData *canvas_data, Element *element) {
  FontDialogData *data = g_new0(FontDialogData, 1);
  data->element = element;

  // Save original values
  switch (data->element->type) {
  case ELEMENT_NOTE: {
    Note* el = (Note*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_PAPER_NOTE: {
    PaperNote* el = (PaperNote*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_SPACE: {
    SpaceElement* el = (SpaceElement*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_MEDIA_FILE: {
    MediaNote* el = (MediaNote*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_SHAPE: {
    Shape* el = (Shape*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_INLINE_TEXT: {
    InlineText* el = (InlineText*)data->element;
    copy_original_font_and_color(data, el->font_description, el->text_r, el->text_g, el->text_b, el->text_a);
    break;
  }
  case ELEMENT_CONNECTION:
  case ELEMENT_FREEHAND_DRAWING:
    break;
  }

  GtkRoot *root = gtk_widget_get_root(element->canvas_data->drawing_area);
  GtkWindow *window = GTK_WINDOW(root);
  data->dialog = gtk_dialog_new_with_buttons(
    "Change Text Properties",
    window,
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    "_Cancel", GTK_RESPONSE_CANCEL,
    "_Apply", GTK_RESPONSE_OK,
    NULL
  );

  gtk_window_set_default_size(GTK_WINDOW(data->dialog), 450, 300);
  gtk_window_set_resizable(GTK_WINDOW(data->dialog), TRUE);

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(data->dialog));
  gtk_widget_set_margin_top(content_area, 18);
  gtk_widget_set_margin_bottom(content_area, 18);
  gtk_widget_set_margin_start(content_area, 18);
  gtk_widget_set_margin_end(content_area, 18);

  // Create main container with better spacing
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
  gtk_box_append(GTK_BOX(content_area), main_box);

  // Get current font properties
  char *current_family = get_font_family_from_desc(data->original_font_desc);
  int current_size = get_font_size_from_desc(data->original_font_desc);
  gboolean current_bold = is_font_bold(data->original_font_desc);
  gboolean current_italic = is_font_italic(data->original_font_desc);

  // Get strikethrough from ModelElement (not from font description)
  Model* model = element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  gboolean current_strikethrough = (model_element && model_element->text) ? model_element->text->strikethrough : FALSE;

  // Font Selection Frame
  GtkWidget *font_frame = gtk_frame_new("Font");
  gtk_widget_set_margin_bottom(font_frame, 10);
  GtkWidget *font_grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(font_grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(font_grid), 12);
  gtk_frame_set_child(GTK_FRAME(font_frame), font_grid);
  gtk_widget_set_margin_top(font_grid, 12);
  gtk_widget_set_margin_bottom(font_grid, 12);
  gtk_widget_set_margin_start(font_grid, 12);
  gtk_widget_set_margin_end(font_grid, 12);

  // Font family selection
  GtkWidget *font_label = gtk_label_new("Family:");
  gtk_widget_set_halign(font_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(font_label, FALSE);
  data->font_combo = gtk_combo_box_text_new();
  gtk_widget_set_hexpand(data->font_combo, TRUE);

  // Populate font list
  PangoFontMap *font_map = pango_cairo_font_map_get_default();
  PangoFontFamily **families;
  int n_families;
  pango_font_map_list_families(font_map, &families, &n_families);

  // Resolve which font Pango actually uses from the description
  PangoFontDescription *resolved_desc = pango_font_description_from_string(data->original_font_desc);
  PangoFont *resolved_font = pango_font_map_load_font(font_map, NULL, resolved_desc);
  PangoFontDescription *actual_desc = pango_font_describe(resolved_font);
  const char *resolved_family = pango_font_description_get_family(actual_desc);

  int current_index = 0;
  for (int i = 0; i < n_families; i++) {
    const char *family_name = pango_font_family_get_name(families[i]);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->font_combo), family_name);

    if (g_strcmp0(family_name, resolved_family) == 0) {
      current_index = i;
    }
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(data->font_combo), current_index);

  pango_font_description_free(actual_desc);
  g_object_unref(resolved_font);
  pango_font_description_free(resolved_desc);
  g_free(families);
  g_free(current_family);

  // Font size
  GtkWidget *size_label = gtk_label_new("Size:");
  gtk_widget_set_halign(size_label, GTK_ALIGN_START);
  data->size_spin = gtk_spin_button_new_with_range(6, 144, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->size_spin), current_size);

  // Font style checkboxes in a horizontal box
  GtkWidget *style_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
  data->bold_check = gtk_check_button_new_with_label("Bold");
  data->italic_check = gtk_check_button_new_with_label("Italic");
  data->strikethrough_check = gtk_check_button_new_with_label("Strikethrough");
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data->bold_check), current_bold);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data->italic_check), current_italic);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data->strikethrough_check), current_strikethrough);

  gtk_box_append(GTK_BOX(style_box), data->bold_check);
  gtk_box_append(GTK_BOX(style_box), data->italic_check);
  gtk_box_append(GTK_BOX(style_box), data->strikethrough_check);

  // Layout font grid
  gtk_grid_attach(GTK_GRID(font_grid), font_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(font_grid), data->font_combo, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(font_grid), size_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(font_grid), data->size_spin, 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(font_grid), gtk_label_new("Style:"), 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(font_grid), style_box, 1, 2, 1, 1);

  // Color Selection Frame
  GtkWidget *color_frame = gtk_frame_new("Color");
  GtkWidget *color_grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(color_grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(color_grid), 12);
  gtk_frame_set_child(GTK_FRAME(color_frame), color_grid);
  gtk_widget_set_margin_top(color_grid, 12);
  gtk_widget_set_margin_bottom(color_grid, 12);
  gtk_widget_set_margin_start(color_grid, 12);
  gtk_widget_set_margin_end(color_grid, 12);

  GtkWidget *color_label = gtk_label_new("Text Color:");
  gtk_widget_set_halign(color_label, GTK_ALIGN_START);
  data->color_button = gtk_color_button_new();

  GdkRGBA current_color = {
    .red = data->original_r,
    .green = data->original_g,
    .blue = data->original_b,
    .alpha = data->original_a,
  };
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->color_button), &current_color);

  gtk_grid_attach(GTK_GRID(color_grid), color_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(color_grid), data->color_button, 1, 0, 1, 1);

  // Alignment Selection Frame (not shown for inline text)
  GtkWidget *alignment_frame = NULL;
  if (element->type != ELEMENT_INLINE_TEXT) {
    alignment_frame = gtk_frame_new("Alignment");
    GtkWidget *alignment_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(alignment_grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(alignment_grid), 12);
    gtk_frame_set_child(GTK_FRAME(alignment_frame), alignment_grid);
    gtk_widget_set_margin_top(alignment_grid, 12);
    gtk_widget_set_margin_bottom(alignment_grid, 12);
    gtk_widget_set_margin_start(alignment_grid, 12);
    gtk_widget_set_margin_end(alignment_grid, 12);

    GtkWidget *alignment_label = gtk_label_new("Text Alignment:");
    gtk_widget_set_halign(alignment_label, GTK_ALIGN_START);
    data->alignment_combo = gtk_drop_down_new_from_strings((const char *[]){
      "Top-Left", "Top-Center", "Top-Right", "Center", "Bottom-Left", "Bottom-Right", NULL
    });

  // Get current alignment from element
  const char *current_alignment = NULL;
  if (element->type == ELEMENT_NOTE) {
    Note *note = (Note*)element;
    current_alignment = note->alignment;
  } else if (element->type == ELEMENT_PAPER_NOTE) {
    PaperNote *note = (PaperNote*)element;
    current_alignment = note->alignment;
  } else if (element->type == ELEMENT_SPACE) {
    SpaceElement *space = (SpaceElement*)element;
    current_alignment = space->alignment;
  } else if (element->type == ELEMENT_MEDIA_FILE) {
    MediaNote *media = (MediaNote*)element;
    current_alignment = media->alignment;
  } else if (element->type == ELEMENT_SHAPE) {
    Shape *shape = (Shape*)element;
    current_alignment = shape->alignment;
  }

    data->original_alignment = current_alignment ? g_strdup(current_alignment) : g_strdup("center");

    // Set current selection
    int selected_index = 3; // Default to "Center"
    if (current_alignment) {
      if (g_strcmp0(current_alignment, "top-left") == 0) selected_index = 0;
      else if (g_strcmp0(current_alignment, "top-center") == 0) selected_index = 1;
      else if (g_strcmp0(current_alignment, "top-right") == 0) selected_index = 2;
      else if (g_strcmp0(current_alignment, "center") == 0) selected_index = 3;
      else if (g_strcmp0(current_alignment, "bottom-left") == 0) selected_index = 4;
      else if (g_strcmp0(current_alignment, "bottom-right") == 0) selected_index = 5;
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(data->alignment_combo), selected_index);

    gtk_grid_attach(GTK_GRID(alignment_grid), alignment_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(alignment_grid), data->alignment_combo, 1, 0, 1, 1);
  }

  gtk_box_append(GTK_BOX(main_box), font_frame);
  gtk_box_append(GTK_BOX(main_box), color_frame);
  if (alignment_frame) {
    gtk_box_append(GTK_BOX(main_box), alignment_frame);
  }

  g_signal_connect_data(data->font_combo, "changed",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  g_signal_connect_data(data->size_spin, "value-changed",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  g_signal_connect_data(data->bold_check, "toggled",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  g_signal_connect_data(data->italic_check, "toggled",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  g_signal_connect_data(data->strikethrough_check, "toggled",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  g_signal_connect_data(data->color_button, "color-set",
                       G_CALLBACK(on_font_selection_changed), data, NULL, 0);
  if (data->alignment_combo) {
    g_signal_connect_data(data->alignment_combo, "notify::selected",
                         G_CALLBACK(on_alignment_changed), data, NULL, 0);
  }

  g_signal_connect(data->dialog, "response", G_CALLBACK(on_font_dialog_response), data);

  gtk_widget_show(data->dialog);
}
