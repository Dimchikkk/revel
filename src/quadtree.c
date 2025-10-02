#include "quadtree.h"
#include <stdlib.h>
#include <math.h>

static QuadTreeNode* quadtree_node_new(double x, double y, double width, double height, int depth) {
    QuadTreeNode *node = g_new0(QuadTreeNode, 1);
    node->bounds.x = x;
    node->bounds.y = y;
    node->bounds.width = width;
    node->bounds.height = height;
    node->elements = g_ptr_array_new();
    node->depth = depth;
    for (int i = 0; i < 4; i++) {
        node->children[i] = NULL;
    }
    return node;
}

static void quadtree_node_free(QuadTreeNode *node) {
    if (!node) return;

    if (node->elements) {
        g_ptr_array_free(node->elements, TRUE);
    }
    for (int i = 0; i < 4; i++) {
        quadtree_node_free(node->children[i]);
    }
    g_free(node);
}

static gboolean bounds_intersects_element(QuadTreeBounds *bounds, Element *element) {
    double elem_x = element->x;
    double elem_y = element->y;
    double elem_width = element->width;
    double elem_height = element->height;

    // Fast path for non-rotated elements (most common case)
    if (element->rotation_degrees == 0.0) {
        double elem_right = elem_x + elem_width;
        double elem_bottom = elem_y + elem_height;
        double bounds_right = bounds->x + bounds->width;
        double bounds_bottom = bounds->y + bounds->height;
        return !(elem_x > bounds_right || elem_right < bounds->x ||
                 elem_y > bounds_bottom || elem_bottom < bounds->y);
    }

    // Slow path for rotated elements - calculate axis-aligned bounding box
    double cx = elem_x + elem_width / 2.0;
    double cy = elem_y + elem_height / 2.0;
    double angle = element->rotation_degrees * M_PI / 180.0;
    double cos_a = cos(angle);
    double sin_a = sin(angle);
    double half_w = elem_width / 2.0;
    double half_h = elem_height / 2.0;

    // Calculate four corners
    double dx1 = -half_w * cos_a + half_h * sin_a;
    double dy1 = -half_w * sin_a - half_h * cos_a;
    double dx2 = half_w * cos_a + half_h * sin_a;
    double dy2 = half_w * sin_a - half_h * cos_a;

    // Find min/max efficiently
    double min_x = cx + fmin(fmin(dx1, -dx1), fmin(dx2, -dx2));
    double max_x = cx + fmax(fmax(dx1, -dx1), fmax(dx2, -dx2));
    double min_y = cy + fmin(fmin(dy1, -dy1), fmin(dy2, -dy2));
    double max_y = cy + fmax(fmax(dy1, -dy1), fmax(dy2, -dy2));

    double bounds_right = bounds->x + bounds->width;
    double bounds_bottom = bounds->y + bounds->height;

    return !(min_x > bounds_right || max_x < bounds->x ||
             min_y > bounds_bottom || max_y < bounds->y);
}

static gboolean bounds_contains_point(QuadTreeBounds *bounds, double x, double y) {
    return x >= bounds->x && x <= bounds->x + bounds->width &&
           y >= bounds->y && y <= bounds->y + bounds->height;
}

static void quadtree_node_subdivide(QuadTreeNode *node) {
    if (node->children[0] != NULL) return;

    double half_width = node->bounds.width / 2.0;
    double half_height = node->bounds.height / 2.0;
    double x = node->bounds.x;
    double y = node->bounds.y;
    int new_depth = node->depth + 1;

    // NW, NE, SW, SE
    node->children[0] = quadtree_node_new(x, y, half_width, half_height, new_depth);
    node->children[1] = quadtree_node_new(x + half_width, y, half_width, half_height, new_depth);
    node->children[2] = quadtree_node_new(x, y + half_height, half_width, half_height, new_depth);
    node->children[3] = quadtree_node_new(x + half_width, y + half_height, half_width, half_height, new_depth);
}

static void quadtree_node_insert(QuadTreeNode *node, Element *element) {
    if (!bounds_intersects_element(&node->bounds, element)) {
        return;
    }

    // If we have children, try to insert into them
    if (node->children[0] != NULL) {
        for (int i = 0; i < 4; i++) {
            quadtree_node_insert(node->children[i], element);
        }
        return;
    }

    // Add to this node (O(1) operation)
    g_ptr_array_add(node->elements, element);

    // Subdivide if necessary (using len field is O(1))
    if (node->elements->len > QUADTREE_MAX_ELEMENTS &&
        node->depth < QUADTREE_MAX_DEPTH) {
        quadtree_node_subdivide(node);

        // Move all elements to children
        GPtrArray *elements = node->elements;
        node->elements = g_ptr_array_new();

        for (guint i = 0; i < elements->len; i++) {
            Element *elem = g_ptr_array_index(elements, i);
            for (int j = 0; j < 4; j++) {
                quadtree_node_insert(node->children[j], elem);
            }
        }
        g_ptr_array_free(elements, TRUE);
    }
}

static void quadtree_node_query_point(QuadTreeNode *node, double x, double y, GList **results) {
    if (!bounds_contains_point(&node->bounds, x, y)) {
        return;
    }

    // Add elements from this node
    for (guint i = 0; i < node->elements->len; i++) {
        *results = g_list_prepend(*results, g_ptr_array_index(node->elements, i));
    }

    // Query children
    if (node->children[0] != NULL) {
        for (int i = 0; i < 4; i++) {
            quadtree_node_query_point(node->children[i], x, y, results);
        }
    }
}

QuadTree* quadtree_new(double x, double y, double width, double height) {
    QuadTree *tree = g_new0(QuadTree, 1);
    tree->root = quadtree_node_new(x, y, width, height, 0);
    return tree;
}

void quadtree_free(QuadTree *tree) {
    if (!tree) return;
    quadtree_node_free(tree->root);
    g_free(tree);
}

void quadtree_insert(QuadTree *tree, Element *element) {
    if (!tree || !element) return;
    quadtree_node_insert(tree->root, element);
}

void quadtree_clear(QuadTree *tree) {
    if (!tree || !tree->root) return;

    // Save bounds before freeing the root
    double x = tree->root->bounds.x;
    double y = tree->root->bounds.y;
    double width = tree->root->bounds.width;
    double height = tree->root->bounds.height;

    quadtree_node_free(tree->root);
    tree->root = quadtree_node_new(x, y, width, height, 0);
}

GList* quadtree_query_point(QuadTree *tree, double x, double y) {
    if (!tree) return NULL;
    GList *results = NULL;
    quadtree_node_query_point(tree->root, x, y, &results);
    return results;
}
