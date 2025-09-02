// undo_manager.h
#ifndef UNDO_MANAGER_H
#define UNDO_MANAGER_H

#include <gtk/gtk.h>
#include <glib.h>
#include "canvas.h"

typedef enum {
    ACTION_CREATE_NOTE,
    ACTION_CREATE_PAPER_NOTE,
    ACTION_CREATE_CONNECTION,
    ACTION_MOVE_ELEMENT,
    ACTION_RESIZE_ELEMENT,
    ACTION_EDIT_TEXT,
    ACTION_DELETE_ELEMENT
} ActionType;

// Data structures for different action types
typedef struct {
    Element *element;
    double old_x, old_y;
    double new_x, new_y;
} MoveData;

typedef struct {
    Element *element;
    double old_width, old_height;
    double new_width, new_height;
} ResizeData;

typedef struct {
    Element *element;
    char *old_text;
    char *new_text;
} TextData;

typedef struct {
    Element *element;
    gpointer extra_data; // For element-specific data
} DeleteData;

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

// New functions for specific action types
void undo_manager_push_move_action(UndoManager *manager, Element *element,
                                  double old_x, double old_y,
                                  double new_x, double new_y);
void undo_manager_push_resize_action(UndoManager *manager, Element *element,
                                    double old_width, double old_height,
                                    double new_width, double new_height);
void undo_manager_push_text_action(UndoManager *manager, Element *element,
                                  const char *old_text, const char *new_text);
void undo_manager_push_delete_action(UndoManager *manager, Element *element);

void on_undo_clicked(GtkButton *button, gpointer user_data);
void on_redo_clicked(GtkButton *button, gpointer user_data);
void show_action_log(CanvasData *data);
void on_log_clicked(GtkButton *button, gpointer user_data);

#endif
