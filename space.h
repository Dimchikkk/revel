#ifndef SPACE_H
#define SPACE_H

#include "element.h"
#include <uuid/uuid.h>

typedef struct _CanvasData CanvasData;

// Space element (visual representation of a space)
typedef struct {
    Element base;
    gchar *name;              // Name of the space
    gchar *target_space_uuid; // UUID of the target space this element represents
} SpaceElement;

// Function declarations
SpaceElement *space_element_create(int x, int y, int z, int width, int height,
                                   const gchar *name,
                                   const gchar *target_space_uuid,
                                   CanvasData *data);

#endif
