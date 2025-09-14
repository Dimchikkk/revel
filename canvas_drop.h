#ifndef CANVAS_VIDEO_DROP_H
#define CANVAS_VIDEO_DROP_H

#include "canvas_core.h"

void canvas_setup_drop_target(CanvasData *data);
gboolean canvas_on_drop(GtkDropTarget *target, const GValue *value,
                       double x, double y, gpointer user_data);

#endif
