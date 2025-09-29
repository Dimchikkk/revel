#ifndef UNDO_MANAGER_H
#define UNDO_MANAGER_H

#include <gtk/gtk.h>
#include <glib.h>
#include "model.h"  // Include model instead of canvas

typedef struct _UndoManager UndoManager;

typedef enum {
  ACTION_CREATE_ELEMENT,
  ACTION_MOVE_ELEMENT,
  ACTION_RESIZE_ELEMENT,
  ACTION_EDIT_TEXT,
  ACTION_CHANGE_COLOR,
  ACTION_DELETE_ELEMENT,
} ActionType;

// Data structures for different action types
typedef struct {
  ModelElement *element;
  int old_x, old_y;
  int new_x, new_y;
} MoveData;

typedef struct {
  ModelElement *element;
  int old_width, old_height;
  int new_width, new_height;
} ResizeData;

typedef struct {
  ModelElement *element;
  char *old_text;
  char *new_text;
} TextData;

typedef struct {
  ModelElement *element;
  double old_r, old_g, old_b, old_a;
  double new_r, new_g, new_b, new_a;
} ColorData;

typedef struct {
  ModelElement *element;
  ModelState initial_state;
} CreateData;

typedef struct {
  ModelElement *element;
  ModelState previous_state;
} DeleteData;

typedef struct _Action {
  ActionType type;
  gpointer data;
  char *description;
  GDateTime *timestamp;
} Action;

struct _UndoManager {
  GList *undo_stack;
  GList *redo_stack;
  GList *action_log;
  GtkWidget *log_window;
  GtkListStore *log_store;
  Model *model;  // Reference to the model
};

UndoManager* undo_manager_new(Model *model);
void undo_manager_free(UndoManager *manager);
void undo_manager_push_action(UndoManager *manager, ActionType type, gpointer data, const char *description);
gboolean undo_manager_can_undo(UndoManager *manager);
gboolean undo_manager_can_redo(UndoManager *manager);
void undo_manager_undo(UndoManager *manager);
void undo_manager_redo(UndoManager *manager);
void undo_manager_reset(UndoManager *manager);

void undo_manager_push_create_action(UndoManager *manager, ModelElement *element);
void undo_manager_push_move_action(UndoManager *manager, ModelElement *element,
                                   int old_x, int old_y,
                                   int new_x, int new_y);
void undo_manager_push_resize_action(UndoManager *manager, ModelElement *element,
                                     int old_width, int old_height,
                                     int new_width, int new_height);
void undo_manager_push_text_action(UndoManager *manager, ModelElement *element,
                                   const char *old_text, const char *new_text);
void undo_manager_push_color_action(UndoManager *manager, ModelElement *element,
                                    double old_r, double old_g, double old_b, double old_a,
                                    double new_r, double new_g, double new_b, double new_a);
void undo_manager_push_delete_action(UndoManager *manager, ModelElement *element);

void on_undo_clicked(GtkButton *button, gpointer user_data);
void on_redo_clicked(GtkButton *button, gpointer user_data);
void show_action_log(CanvasData *data);
void on_log_clicked(GtkButton *button, gpointer user_data);
void undo_manager_print_stacks(UndoManager *manager);
void undo_manager_remove_actions_for_element(UndoManager *manager, ModelElement *element);

#endif
