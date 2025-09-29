#ifndef FONT_DIALOG_H
#define FONT_DIALOG_H

#include <gtk/gtk.h>
#include "element.h"

void font_dialog_open(CanvasData *canvas_data, Element *element);

PangoFontDescription* parse_font_description(const char *font_desc);
char* get_font_family_from_desc(const char *font_desc);
int get_font_size_from_desc(const char *font_desc);
gboolean is_font_bold(const char *font_desc);
gboolean is_font_italic(const char *font_desc);
char* create_font_description_string(const char *family, int size,
                                   gboolean bold, gboolean italic);

#endif
