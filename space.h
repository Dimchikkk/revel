#ifndef SPACE_H
#define SPACE_H

#include "element.h"
#include <uuid/uuid.h>

typedef struct _CanvasData CanvasData;

// Space element (visual representation of a space)
typedef struct {
    Element base;
    gchar *name;              // Name of the space
} SpaceElement;

// Function declarations
SpaceElement *space_element_create(int x, int y, int z, int width, int height,
                                   const gchar *name,
                                   CanvasData *data);

#endif
