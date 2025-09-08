#include "element.h"
#include "paper_note.h"
#include "note.h"

void element_draw(Element *element, cairo_t *cr, gboolean is_selected) {
    if (element && element->vtable && element->vtable->draw) {
        element->vtable->draw(element, cr, is_selected);
    }
}

void element_get_connection_point(Element *element, int point, int *cx, int *cy) {
    if (element && element->vtable && element->vtable->get_connection_point) {
        element->vtable->get_connection_point(element, point, cx, cy);
    }
}

int element_pick_resize_handle(Element *element, int x, int y) {
    if (element && element->vtable && element->vtable->pick_resize_handle) {
        return element->vtable->pick_resize_handle(element, x, y);
    }
    return -1;
}

int element_pick_connection_point(Element *element, int x, int y) {
    if (element && element->vtable && element->vtable->pick_connection_point) {
        return element->vtable->pick_connection_point(element, x, y);
    }
    return -1;
}

void element_start_editing(Element *element, GtkWidget *overlay) {
    if (element && element->vtable && element->vtable->start_editing) {
        element->vtable->start_editing(element, overlay);
    }
}

void element_finish_editing(Element *element) {
    if (element && element->vtable && element->vtable->finish_editing) {
        element->vtable->finish_editing(element);
    }
}

void element_update_position(Element *element, int x, int y, int z) {
    element->x = x;
    element->y = y;
    element->z = z;
    if (element && element->vtable && element->vtable->update_position) {
        element->vtable->update_position(element, x, y, z);
    }
}

void element_update_size(Element *element, int width, int height) {
    element->width = width;
    element->height = height;
    if (element && element->vtable && element->vtable->update_size) {
        element->vtable->update_size(element, width, height);
    }
}

void element_free(Element *element) {
    if (element && element->vtable && element->vtable->free) {
        element->vtable->free(element);
    }
}

void element_bring_to_front(Element *element, int *next_z) {
    element->z = (*next_z)++;
}
