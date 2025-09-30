#ifndef CLONE_DIALOG_H
#define CLONE_DIALOG_H

#include <gtk/gtk.h>
#include "canvas_core.h"
#include "model.h"

// Show clone dialog and handle cloning
void clone_dialog_open(CanvasData *canvas_data, ModelElement *element);

#endif
