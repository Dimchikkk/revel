#ifndef SPACE_H
#define SPACE_H

#include "element.h"
#include <uuid/uuid.h>

typedef struct _CanvasData CanvasData;

// Space element (visual representation of a space)
typedef struct {
  Element base;
  gchar *text;
  double text_r, text_g, text_b, text_a;
  char* font_description;

} SpaceElement;

SpaceElement *space_element_create(ElementPosition position,
                                   ElementColor bg_color,
                                   ElementSize size,
                                   ElementText text,
                                   CanvasData *data);

#endif
