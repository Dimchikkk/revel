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
    // Check if rotated element's bounding box intersects with bounds
    double elem_right = element->x + element->width;
    double elem_bottom = element->y + element->height;
    double bounds_right = bounds->x + bounds->width;
    double bounds_bottom = bounds->y + bounds->height;

    return !(element->x > bounds_right || elem_right < bounds->x ||
             element->y > bounds_bottom || elem_bottom < bounds->y);
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

static void quadtree_node_remove(QuadTreeNode *node, Element *element) {
    if (!bounds_intersects_element(&node->bounds, element)) {
        return;
    }

    // Remove from this node
    node->elements = g_list_remove(node->elements, element);

    // Remove from children
    if (node->children[0] != NULL) {
        for (int i = 0; i < 4; i++) {
            quadtree_node_remove(node->children[i], element);
        }
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

static void quadtree_node_query_rect(QuadTreeNode *node, double x, double y,
                                      double width, double height, GList **results) {
    QuadTreeBounds query_bounds = {x, y, width, height};

    // Check if node bounds intersect with query bounds
    double node_right = node->bounds.x + node->bounds.width;
    double node_bottom = node->bounds.y + node->bounds.height;
    double query_right = x + width;
    double query_bottom = y + height;

    if (node->bounds.x > query_right || node_right < x ||
        node->bounds.y > query_bottom || node_bottom < y) {
        return;
    }

    // Add elements from this node
    for (GList *l = node->elements; l != NULL; l = l->next) {
        Element *elem = (Element*)l->data;
        if (bounds_intersects_element(&query_bounds, elem)) {
            *results = g_list_prepend(*results, elem);
        }
    }

    // Query children
    if (node->children[0] != NULL) {
        for (int i = 0; i < 4; i++) {
            quadtree_node_query_rect(node->children[i], x, y, width, height, results);
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

void quadtree_remove(QuadTree *tree, Element *element) {
    if (!tree || !element) return;
    quadtree_node_remove(tree->root, element);
}

void quadtree_clear(QuadTree *tree) {
    if (!tree) return;
    quadtree_node_free(tree->root);
    tree->root = quadtree_node_new(tree->root->bounds.x, tree->root->bounds.y,
                                   tree->root->bounds.width, tree->root->bounds.height, 0);
}

GList* quadtree_query_point(QuadTree *tree, double x, double y) {
    if (!tree) return NULL;
    GList *results = NULL;
    quadtree_node_query_point(tree->root, x, y, &results);
    return results;
}

GList* quadtree_query_rect(QuadTree *tree, double x, double y, double width, double height) {
    if (!tree) return NULL;
    GList *results = NULL;
    quadtree_node_query_rect(tree->root, x, y, width, height, &results);
    return results;
}
