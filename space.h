#ifndef SPACE_H
#define SPACE_H

#include "element.h"
#include <uuid/uuid.h>

typedef struct Space Space;
typedef struct _CanvasData CanvasData;

struct Space {
    uuid_t uuid;
    char *name;
    GList *elements;  // Elements contained in this space
    Space *parent;    // Parent space (NULL for root)
};

// Space element (visual representation of a space)
typedef struct {
    Element base;
    Space *target_space;  // The space this element represents
} SpaceElement;

// Function declarations
Space* space_new(const char *name, Space *parent);
void space_free(Space *space);
SpaceElement* space_element_create(int x, int y, int width, int height,
                                  Space *target_space, int z_index, CanvasData *data);
void switch_to_space(CanvasData *data, Space *space);
void go_back_to_parent_space(CanvasData *data);

#endif
