#ifndef CANVAS_EXECUTE_H
#define CANVAS_EXECUTE_H

#include <gtk/gtk.h>

typedef struct _CanvasData CanvasData;

typedef struct {
    gchar *from_id;
    gchar *to_id;
} ConnectionInfo;

void canvas_show_script_dialog(GtkButton *button, gpointer user_data);

#endif
