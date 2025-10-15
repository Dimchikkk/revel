#ifndef QUADTREE_H
#define QUADTREE_H

#include <glib.h>
#include "elements/element.h"

#define QUADTREE_MAX_ELEMENTS 64
#define QUADTREE_MAX_DEPTH 16

typedef struct _QuadTreeNode QuadTreeNode;

typedef struct {
    double x, y, width, height;
} QuadTreeBounds;

struct _QuadTreeNode {
    QuadTreeBounds bounds;
    GPtrArray *elements;  // Array of Element*
    QuadTreeNode *children[4];  // NW, NE, SW, SE
    int depth;
};

typedef struct {
    QuadTreeNode *root;
} QuadTree;

// Create/destroy
QuadTree* quadtree_new(double x, double y, double width, double height);
void quadtree_free(QuadTree *tree);

// Operations
void quadtree_insert(QuadTree *tree, Element *element);
void quadtree_clear(QuadTree *tree);

// Query elements at a point (for picking)
GList* quadtree_query_point(QuadTree *tree, double x, double y);

#endif
