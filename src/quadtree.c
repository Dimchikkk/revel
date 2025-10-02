#include "quadtree.h"
#include <stdlib.h>
#include <math.h>

static QuadTreeNode* quadtree_node_new(double x, double y, double width, double height, int depth) {
    QuadTreeNode *node = g_new0(QuadTreeNode, 1);
    node->bounds.x = x;
    node->bounds.y = y;
    node->bounds.width = width;
    node->bounds.height = height;
    node->elements = NULL;
    node->depth = depth;
    for (int i = 0; i < 4; i++) {
        node->children[i] = NULL;
    }
    return node;
}

static void quadtree_node_free(QuadTreeNode *node) {
    if (!node) return;

    g_list_free(node->elements);
    for (int i = 0; i < 4; i++) {
        quadtree_node_free(node->children[i]);
    }
    g_free(node);
}

static gboolean bounds_intersects_element(QuadTreeBounds *bounds, Element *element) {
    // Calculate element's actual bounding box (accounting for rotation)
    double elem_x = element->x;
    double elem_y = element->y;
    double elem_width = element->width;
    double elem_height = element->height;

    // If element is rotated, calculate the axis-aligned bounding box
    if (element->rotation_degrees != 0.0) {
        double cx = elem_x + elem_width / 2.0;
        double cy = elem_y + elem_height / 2.0;
        double angle = element->rotation_degrees * M_PI / 180.0;
        double cos_a = cos(angle);
        double sin_a = sin(angle);

        // Get the four corners of the rotated rectangle
        double corners_x[4], corners_y[4];
        double half_w = elem_width / 2.0;
        double half_h = elem_height / 2.0;

        // Top-left
        corners_x[0] = cx + (-half_w * cos_a - (-half_h) * sin_a);
        corners_y[0] = cy + (-half_w * sin_a + (-half_h) * cos_a);
        // Top-right
        corners_x[1] = cx + (half_w * cos_a - (-half_h) * sin_a);
        corners_y[1] = cy + (half_w * sin_a + (-half_h) * cos_a);
        // Bottom-right
        corners_x[2] = cx + (half_w * cos_a - half_h * sin_a);
        corners_y[2] = cy + (half_w * sin_a + half_h * cos_a);
        // Bottom-left
        corners_x[3] = cx + (-half_w * cos_a - half_h * sin_a);
        corners_y[3] = cy + (-half_w * sin_a + half_h * cos_a);

        // Find axis-aligned bounding box
        double min_x = corners_x[0], max_x = corners_x[0];
        double min_y = corners_y[0], max_y = corners_y[0];
        for (int i = 1; i < 4; i++) {
            if (corners_x[i] < min_x) min_x = corners_x[i];
            if (corners_x[i] > max_x) max_x = corners_x[i];
            if (corners_y[i] < min_y) min_y = corners_y[i];
            if (corners_y[i] > max_y) max_y = corners_y[i];
        }

        elem_x = min_x;
        elem_y = min_y;
        elem_width = max_x - min_x;
        elem_height = max_y - min_y;
    }

    double elem_right = elem_x + elem_width;
    double elem_bottom = elem_y + elem_height;
    double bounds_right = bounds->x + bounds->width;
    double bounds_bottom = bounds->y + bounds->height;

    return !(elem_x > bounds_right || elem_right < bounds->x ||
             elem_y > bounds_bottom || elem_bottom < bounds->y);
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

    // Add to this node
    node->elements = g_list_prepend(node->elements, element);

    // Subdivide if necessary
    if (g_list_length(node->elements) > QUADTREE_MAX_ELEMENTS &&
        node->depth < QUADTREE_MAX_DEPTH) {
        quadtree_node_subdivide(node);

        // Move all elements to children
        GList *elements = node->elements;
        node->elements = NULL;

        for (GList *l = elements; l != NULL; l = l->next) {
            Element *elem = (Element*)l->data;
            for (int i = 0; i < 4; i++) {
                quadtree_node_insert(node->children[i], elem);
            }
        }
        g_list_free(elements);
    }
}

static void quadtree_node_query_point(QuadTreeNode *node, double x, double y, GList **results) {
    if (!bounds_contains_point(&node->bounds, x, y)) {
        return;
    }

    // Add elements from this node
    for (GList *l = node->elements; l != NULL; l = l->next) {
        *results = g_list_prepend(*results, l->data);
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
