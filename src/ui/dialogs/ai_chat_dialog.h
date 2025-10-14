#ifndef AI_CHAT_DIALOG_H
#define AI_CHAT_DIALOG_H

#include <gtk/gtk.h>
#include "canvas.h"

void ai_chat_dialog_present(CanvasData *data);
void ai_chat_dialog_toggle(GtkToggleButton *button, gpointer user_data);
void ai_chat_dialog_close(CanvasData *data);

#endif
