#ifndef ELEMENT_H
#define ELEMENT_H

#include <gtk/gtk.h>

typedef struct _CanvasData CanvasData;
typedef struct Element Element;

typedef enum {
  ELEMENT_NOTE,
  ELEMENT_PAPER_NOTE,
  ELEMENT_CONNECTION,
  ELEMENT_SPACE,
  ELEMENT_MEDIA_FILE,
  ELEMENT_FREEHAND_DRAWING,
  ELEMENT_SHAPE,
  ELEMENT_INLINE_TEXT,
} ElementType;

typedef enum {
  MEDIA_TYPE_IMAGE,
  MEDIA_TYPE_VIDEO,
  MEDIA_TYPE_NONE
} MediaType;

typedef struct {
  void (*draw)(Element *element, cairo_t *cr, gboolean is_selected);
  void (*get_connection_point)(Element *element, int point, int *cx, int *cy);
  int (*pick_resize_handle)(Element *element, int x, int y);
  int (*pick_connection_point)(Element *element, int x, int y);
  void (*start_editing)(Element *element, GtkWidget *overlay);
  void (*update_position)(Element *element, int x, int y, int z);
  void (*update_size)(Element *element, int width, int height);
  void (*free)(Element *element);
} ElementVTable;

typedef struct {
    double r, g, b, a;
} ElementColor;

typedef struct {
    int x, y, z;
} ElementPosition;

typedef struct {
    int width, height;
} ElementSize;

typedef struct {
  MediaType type;
  unsigned char *image_data;
  int image_size;
  unsigned char *video_data;
  int  video_size;
  int duration;
} ElementMedia;

typedef struct {
  char* text;
  ElementColor text_color;
  char* font_description;
} ElementText;

typedef struct {
  Element* from_element;
  Element* to_element;
  char* from_element_uuid;
  char* to_element_uuid;
  int from_point;
  int to_point;
  int connection_type;
  int arrowhead_type;
} ElementConnection;

typedef struct {
  GArray* drawing_points;
  int stroke_width;
} ElementDrawing;

typedef struct {
  int shape_type;
  int stroke_width;
  gboolean filled;
} ElementShape;

typedef struct {
  ElementType type;
  ElementColor bg_color;
  ElementPosition position;
  ElementSize size;
  ElementMedia media;
  ElementDrawing drawing;
  ElementConnection connection;
  ElementText text;
  ElementShape shape;
} ElementConfig;

struct Element {
  ElementType type;
  ElementVTable *vtable;
  int x, y, z;
  int width, height;
  gboolean dragging;
  int drag_offset_x, drag_offset_y;
  gboolean resizing;
  int resize_edge;
  int resize_start_x, resize_start_y;
  int orig_x, orig_y, orig_width, orig_height;
  double bg_r, bg_g, bg_b, bg_a;
  CanvasData *canvas_data;

  // Animation properties
  gboolean animating;
  gint64 animation_start_time;
  double animation_alpha;
};

// Interface functions
void element_draw(Element *element, cairo_t *cr, gboolean is_selected);
void element_get_connection_point(Element *element, int point, int *cx, int *cy);
int element_pick_resize_handle(Element *element, int x, int y);
int element_pick_connection_point(Element *element, int x, int y);
void element_start_editing(Element *element, GtkWidget *overlay);
void element_update_position(Element *element, int x, int y, int z);
void element_update_size(Element *element, int width, int height);
void element_free(Element *element);
void element_bring_to_front(Element *element, int *next_z);

// Utility function to get human-readable name for element types
const char* element_get_type_name(ElementType type);

#endif
