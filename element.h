#ifndef ELEMENT_H
#define ELEMENT_H

#include <gtk/gtk.h>
#include "vector.h"

typedef struct _CanvasData CanvasData;
typedef struct Element Element;

typedef enum {
  ELEMENT_NOTE,
  ELEMENT_PAPER_NOTE,
  ELEMENT_CONNECTION,
  ELEMENT_SPACE,
} ElementType;

typedef struct {
    void (*draw)(Element *element, cairo_t *cr, gboolean is_selected);
    void (*get_connection_point)(Element *element, int point, int *cx, int *cy);
    int (*pick_resize_handle)(Element *element, int x, int y);
    int (*pick_connection_point)(Element *element, int x, int y);
    void (*start_editing)(Element *element, GtkWidget *overlay);
    void (*finish_editing)(Element *element);
    void (*update_position)(Element *element, int x, int y, int z);
    void (*update_size)(Element *element, int width, int height);
    void (*free)(Element *element);
} ElementVTable;

struct Element {
    ElementType type;
    ElementVTable *vtable;
    int x, y, z;
    int width, height;
    gboolean hidden;
    gboolean dragging;
    int drag_offset_x, drag_offset_y;
    gboolean resizing;
    int resize_edge;
    int resize_start_x, resize_start_y;
    int orig_x, orig_y, orig_width, orig_height;
    CanvasData *canvas_data;
};

// Interface functions
void element_draw(Element *element, cairo_t *cr, gboolean is_selected);
void element_get_connection_point(Element *element, int point, int *cx, int *cy);
int element_pick_resize_handle(Element *element, int x, int y);
int element_pick_connection_point(Element *element, int x, int y);
void element_start_editing(Element *element, GtkWidget *overlay);
void element_finish_editing(Element *element);
void element_update_position(Element *element, int x, int y, int z);
void element_update_size(Element *element, int width, int height);
void element_free(Element *element);
void element_bring_to_front(Element *element, int *next_z);

#endif
