#ifndef CANVAS_SPACES_H
#define CANVAS_SPACES_H

#include "canvas.h"

void switch_to_space(CanvasData *data, Space *space);
void go_back_to_parent_space(CanvasData *data);
void space_creation_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);

#endif
