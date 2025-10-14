#ifndef CANVAS_EXECUTE_H
#define CANVAS_EXECUTE_H

#include <gtk/gtk.h>
#include "element.h"

typedef struct _CanvasData CanvasData;

typedef struct {
    gchar *from_id;
    gchar *to_id;
    int connection_type;  // CONNECTION_TYPE_PARALLEL or CONNECTION_TYPE_STRAIGHT
    int arrowhead_type;   // ARROWHEAD_NONE, ARROWHEAD_SINGLE, or ARROWHEAD_DOUBLE
    gboolean has_color;   // TRUE when DSL provided an explicit color
    double r, g, b, a;    // Arrow color (valid when has_color is TRUE)
} ConnectionInfo;

void canvas_show_script_dialog(GtkButton *button, gpointer user_data);
void canvas_execute_script(CanvasData *data, const gchar *script);
void canvas_execute_script_file(CanvasData *data, const gchar *script, const gchar *filename);
void canvas_execute_script_internal(CanvasData *data, const gchar *script, const gchar *filename, gboolean skip_type_check);
gchar *canvas_generate_dsl_from_model(CanvasData *data);
void canvas_presentation_next_slide(CanvasData *data);
void canvas_presentation_prev_slide(CanvasData *data);
gboolean canvas_is_presentation_mode(CanvasData *data);
gboolean dsl_handle_element_click(CanvasData *data, Element *element);

#endif
