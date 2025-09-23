#include "font_dialog.h"
#include "canvas.h"
#include "model.h"
#include "note.h"
#include "paper_note.h"
#include "media_note.h"
#include "space.h"
#include <pango/pango.h>
#include <pango/pangocairo.h>

// Font dialog data structure
typedef struct {
  Element *element;
  char *element_uuid;
  GtkWidget *dialog;
  GtkWidget *font_combo;
  GtkWidget *size_spin;
  GtkWidget *bold_check;
  GtkWidget *italic_check;
  GtkWidget *color_button;
  char *original_font_desc;
  double original_r, original_g, original_b, original_a;
} FontDialogData;

char* get_font_family_from_desc(const char *font_desc) {
  PangoFontDescription *desc = pango_font_description_from_string(font_desc);
  const char *family = pango_font_description_get_family(desc);
  char *result = family ? g_strdup(family) : g_strdup("Sans");
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
                                     gboolean bold, gboolean italic) {
  PangoFontDescription *desc = pango_font_description_new();

  pango_font_description_set_family(desc, family);
  pango_font_description_set_size(desc, size * PANGO_SCALE);
  pango_font_description_set_weight(desc, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
  pango_font_description_set_style(desc, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

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

static void apply_font_changes(FontDialogData *data) {
  gchar *font_family = gtk_combo_box_text_get_active_text(
                                                          GTK_COMBO_BOX_TEXT(data->font_combo));

  gdouble font_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->size_spin));

  gboolean bold = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->bold_check));
  gboolean italic = gtk_check_button_get_active(GTK_CHECK_BUTTON(data->italic_check));

  char *new_font_desc = create_font_description_string(font_family, font_size, bold, italic);

  GdkRGBA new_color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(data->color_button), &new_color);
  Model* model = data->element->canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, data->element);
  // Update model
  model_update_text_color(model, model_element, new_color.red, new_color.green, new_color.blue, new_color.alpha);
  model_update_font(model, model_element, new_font_desc);

  // Update visual
  switch (data->element->type) {
  case ELEMENT_NOTE: {
    Note* el = (Note*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    break;
  }
  case ELEMENT_PAPER_NOTE: {
    PaperNote* el = (PaperNote*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    break;
  }
  case ELEMENT_SPACE: {
    SpaceElement* el = (SpaceElement*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
    break;
  }
  case ELEMENT_MEDIA_FILE: {
    MediaNote* el = (MediaNote*)data->element;
    update_font_and_color(&el->font_description, new_font_desc, &el->text_r, &el->text_g, &el->text_b, &el->text_a, &new_color);
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

static void on_font_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  FontDialogData *data = (FontDialogData *)user_data;

  if (response_id == GTK_RESPONSE_OK) {
    apply_font_changes(data);
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
  case ELEMENT_CONNECTION:
  case ELEMENT_FREEHAND_DRAWING:
    break;
  }

  GtkRoot *root = gtk_widget_get_root(element->canvas_data->drawing_area);
  GtkWindow *window = GTK_WINDOW(root);
  data->dialog = gtk_dialog_new_with_buttons(
                                             "Change Text Properties",
                                             window,
                                             GTK_DIALOG_MODAL,
                                             "_Cancel", GTK_RESPONSE_CANCEL,
                                             "_Apply", GTK_RESPONSE_OK,
                                             NULL
                                             );

  gtk_window_set_default_size(GTK_WINDOW(data->dialog), 400, 300);

  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(data->dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
  gtk_widget_set_margin_top(grid, 12);
  gtk_widget_set_margin_bottom(grid, 12);
  gtk_widget_set_margin_start(grid, 12);
  gtk_widget_set_margin_end(grid, 12);

  GtkWidget *font_label = gtk_label_new("Font Family:");
  gtk_widget_set_halign(font_label, GTK_ALIGN_START);
  data->font_combo = gtk_combo_box_text_new();

  // Get current font properties
  char *current_family = get_font_family_from_desc(data->original_font_desc);
  int current_size = get_font_size_from_desc(data->original_font_desc);
  gboolean current_bold = is_font_bold(data->original_font_desc);
  gboolean current_italic = is_font_italic(data->original_font_desc);

  // Get ALL available fonts using Pango
  PangoFontMap *font_map = pango_cairo_font_map_get_default();
  PangoFontFamily **families;
  int n_families;
  pango_font_map_list_families(font_map, &families, &n_families);

  int current_index = 0;
  for (int i = 0; i < n_families; i++) {
    const char *family_name = pango_font_family_get_name(families[i]);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(data->font_combo), family_name);

    if (g_strcmp0(family_name, current_family) == 0) {
      current_index = i;
    }
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(data->font_combo), current_index);
  g_free(families);
  g_free(current_family);

  GtkWidget *size_label = gtk_label_new("Font Size:");
  gtk_widget_set_halign(size_label, GTK_ALIGN_START);
  data->size_spin = gtk_spin_button_new_with_range(6, 72, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->size_spin), current_size);

  data->bold_check = gtk_check_button_new_with_label("Bold");
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data->bold_check), current_bold);

  data->italic_check = gtk_check_button_new_with_label("Italic");
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data->italic_check), current_italic);

  GtkWidget *color_label = gtk_label_new("Text Color:");
  gtk_widget_set_halign(color_label, GTK_ALIGN_START);
  data->color_button = gtk_color_button_new();

  GdkRGBA current_color = {
    data->original_r,
    data->original_g,
    data->original_b,
    data->original_a,
  };
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(data->color_button), &current_color);

  gtk_grid_attach(GTK_GRID(grid), font_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), data->font_combo, 1, 0, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), size_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), data->size_spin, 1, 1, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), data->bold_check, 0, 2, 2, 1);
  gtk_grid_attach(GTK_GRID(grid), data->italic_check, 0, 3, 2, 1);

  gtk_grid_attach(GTK_GRID(grid), color_label, 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), data->color_button, 1, 4, 1, 1);

  gtk_box_append(GTK_BOX(content_area), grid);

  g_signal_connect(data->dialog, "response", G_CALLBACK(on_font_dialog_response), data);

  gtk_widget_show(data->dialog);
}
