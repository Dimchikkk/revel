#include "space.h"
#include "element.h"
#include "canvas.h"
#include <stdlib.h>
#include <string.h>

void space_element_draw(Element *element, cairo_t *cr, gboolean is_selected) {
    SpaceElement *space_elem = (SpaceElement*)element;

    // Draw rectangle with rounded corners
    double radius = 20.0; // Corner radius
    double x = element->x;
    double y = element->y;
    double width = element->width;
    double height = element->height;

    // Create rounded rectangle path
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -G_PI_2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, G_PI_2);
    cairo_arc(cr, x + radius, y + height - radius, radius, G_PI_2, G_PI);
    cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);

    if (is_selected) {
        cairo_set_source_rgb(cr, 0.7, 0.7, 1.0);  // Light blue when selected
    } else {
        cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);  // Light blue
    }
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.8);  // Dark blue border
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);

    // Draw space name
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 12");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, space_elem->name, -1);

    // Set text width to fit within the rounded rectangle (with padding)
    pango_layout_set_width(layout, (width - 40) * PANGO_SCALE); // 20px padding on each side
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    // Center text within the element
    double text_x = x + (width - text_width) / 2;
    double text_y = y + (height - text_height) / 2;

    cairo_move_to(cr, text_x, text_y);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);  // Black text
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
}

// Implement other required functions (similar to note elements)
void space_element_get_connection_point(Element *element, int point, int *cx, int *cy) {
    // Same as note elements
    switch(point) {
    case 0: *cx = element->x + element->width/2; *cy = element->y; break;
    case 1: *cx = element->x + element->width; *cy = element->y + element->height/2; break;
    case 2: *cx = element->x + element->width/2; *cy = element->y + element->height; break;
    case 3: *cx = element->x; *cy = element->y + element->height/2; break;
    }
}

int space_element_pick_resize_handle(Element *element, int x, int y) {
    // Same as note elements
    int size = 8;
    struct { int px, py; } handles[4] = {
        {element->x, element->y},
        {element->x + element->width, element->y},
        {element->x + element->width, element->y + element->height},
        {element->x, element->y + element->height}
    };

    for (int i = 0; i < 4; i++) {
        if (abs(x - handles[i].px) <= size && abs(y - handles[i].py) <= size) {
            return i;
        }
    }
    return -1;
}

int space_element_pick_connection_point(Element *element, int x, int y) {
    // Same as note elements
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        space_element_get_connection_point(element, i, &cx, &cy);
        int dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < 36) return i;
    }
    return -1;
}

void space_element_update_position(Element *element, int x, int y, int z) {
    // Update position with z coordinate
    element->x = x;
    element->y = y;
    element->z = z;
}

void space_element_update_size(Element *element, int width, int height) {
    // Update size
    element->width = width;
    element->height = height;
}

void space_element_free(Element *element) {
    SpaceElement *space_elem = (SpaceElement*)element;
    g_free(space_elem->name);
    g_free(space_elem->target_space_uuid);
    g_free(space_elem);
}

void space_name_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    SpaceElement *space_elem = (SpaceElement*)user_data;

    if (response_id == GTK_RESPONSE_OK) {
        GtkWidget *entry = gtk_dialog_get_widget_for_response(dialog, GTK_RESPONSE_OK);
        const char *new_name = gtk_editable_get_text(GTK_EDITABLE(entry));

        // Update space name
        g_free(space_elem->name);
        space_elem->name = g_strdup(new_name);

        // Queue redraw
        if (space_elem->base.canvas_data && space_elem->base.canvas_data->drawing_area) {
            gtk_widget_queue_draw(space_elem->base.canvas_data->drawing_area);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

// currently editing is not enabled for spaces
void space_element_start_editing(Element *element, GtkWidget *overlay) {
    SpaceElement *space_elem = (SpaceElement*)element;
    CanvasData *data = space_elem->base.canvas_data;

    GtkRoot *root = gtk_widget_get_root(data->drawing_area);
    GtkWindow *window = GTK_WINDOW(root);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Edit Space Name",
        window,
        GTK_DIALOG_MODAL,
        "OK", GTK_RESPONSE_OK,
        "Cancel", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();

    gtk_editable_set_text(GTK_EDITABLE(entry), space_elem->name);

    gtk_box_append(GTK_BOX(content_area), entry);

    g_signal_connect(dialog, "response", G_CALLBACK(space_name_dialog_response), space_elem);

    gtk_window_present(GTK_WINDOW(dialog));
}

static ElementVTable space_element_vtable = {
    .draw = space_element_draw,
    .get_connection_point = space_element_get_connection_point,
    .pick_resize_handle = space_element_pick_resize_handle,
    .pick_connection_point = space_element_pick_connection_point,
    .start_editing = space_element_start_editing,
    .update_position = space_element_update_position,
    .update_size = space_element_update_size,
    .free = space_element_free
};

SpaceElement* space_element_create(int x, int y, int z, int width, int height,
                                   const gchar *name, const gchar *target_space_uuid,
                                   CanvasData *data) {
    SpaceElement *space_elem = g_new0(SpaceElement, 1);
    space_elem->base.type = ELEMENT_SPACE;
    space_elem->base.vtable = &space_element_vtable;
    space_elem->base.x = x;
    space_elem->base.y = y;
    space_elem->base.z = z;
    space_elem->base.width = width;
    space_elem->base.height = height;
    space_elem->base.canvas_data = data;
    space_elem->name = g_strdup(name);
    space_elem->target_space_uuid = g_strdup(target_space_uuid);
    return space_elem;
}
