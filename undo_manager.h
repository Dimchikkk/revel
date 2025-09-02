#ifndef UNDO_MANAGER_H
#define UNDO_MANAGER_H

#include <gtk/gtk.h>
#include <glib.h>
#include "canvas.h"

typedef enum {
    ACTION_CREATE_NOTE,
    ACTION_CREATE_PAPER_NOTE,
    ACTION_CREATE_CONNECTION,
} ActionType;

typedef struct _Action {
    ActionType type;
    gpointer data;
    char *description;
    GDateTime *timestamp;
} Action;

typedef struct _UndoManager {
    GList *undo_stack;
    GList *redo_stack;
    GList *action_log;
    GtkWidget *log_window;
    GtkListStore *log_store;
} UndoManager;

UndoManager* undo_manager_new();
void undo_manager_free(UndoManager *manager);
void undo_manager_push_action(UndoManager *manager, ActionType type, gpointer data, const char *description);
gboolean undo_manager_can_undo(UndoManager *manager);
gboolean undo_manager_can_redo(UndoManager *manager);
void undo_manager_undo(CanvasData *data);
void undo_manager_redo(CanvasData *data);
void on_undo_clicked(GtkButton *button, gpointer user_data);
void on_redo_clicked(GtkButton *button, gpointer user_data);
void show_action_log(CanvasData *data);
void on_log_clicked(GtkButton *button, gpointer user_data);

#endif
