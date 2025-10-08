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
  MEDIA_TYPE_AUDIO,
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
  gboolean strikethrough;
  char* alignment;  // Supported values: "top-left", "top-center", "top-right", "center", "bottom-left", "bottom-right"
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
  int stroke_style;   // 0=solid, 1=dashed, 2=dotted
  int fill_style;     // 0=solid, 1=hachure, 2=cross-hatch
  ElementColor stroke_color;
} ElementShape;

typedef struct {
  ElementType type;
  ElementColor bg_color;
  ElementPosition position;
  ElementSize size;
  double rotation_degrees;
  ElementMedia media;
  ElementDrawing drawing;
  ElementConnection connection;
  ElementText text;
  ElementShape shape;
} ElementConfig;

// Forward declare ModelElement to avoid circular dependency
typedef struct _ModelElement ModelElement;

struct Element {
  ElementType type;
  ElementVTable *vtable;
  int x, y, z;
  int width, height;
  double rotation_degrees;  // Rotation angle in degrees (0-360)
  gboolean dragging;
  int drag_offset_x, drag_offset_y;
  gboolean resizing;
  int resize_edge;
  int resize_start_x, resize_start_y;
  int orig_x, orig_y, orig_width, orig_height;
  gboolean rotating;
  double rotation_start_angle;
  double orig_rotation;
  double bg_r, bg_g, bg_b, bg_a;
  CanvasData *canvas_data;

  // Animation properties
  gboolean animating;
  gint64 animation_start_time;
  double animation_alpha;

  // OPTIMIZATION: Reverse pointer to model element for O(1) lookups
  ModelElement *model_element;
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

// Rotation functions
void element_get_rotation_handle_position(Element *element, int *hx, int *hy);
int element_pick_rotation_handle(Element *element, int x, int y);
void element_draw_rotation_handle(Element *element, cairo_t *cr);

// Text alignment
// Supported alignment values: "top-left", "top-center", "top-right", "center", "bottom-left", "bottom-right"
typedef enum {
  TEXT_ALIGN_TOP_LEFT,
  TEXT_ALIGN_TOP_CENTER,
  TEXT_ALIGN_TOP_RIGHT,
  TEXT_ALIGN_CENTER,
  TEXT_ALIGN_BOTTOM_LEFT,
  TEXT_ALIGN_BOTTOM_RIGHT
} TextAlignment;

// Text alignment utilities (for backward compatibility with string-based alignment)
typedef enum {
  VALIGN_TOP,
  VALIGN_CENTER,
  VALIGN_BOTTOM
} VerticalAlign;

PangoAlignment element_get_pango_alignment(const char *alignment);
VerticalAlign element_get_vertical_alignment(const char *alignment);
const char* element_alignment_to_string(TextAlignment alignment);
TextAlignment element_string_to_alignment(const char *alignment);

#endif
