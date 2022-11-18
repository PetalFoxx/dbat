/***************************************************************************
 *   File: htree.h                                                         *
 *  Usage: Generalized hash tree code for fast lookups                     *
 *                                                                         *
 * This code is released under the CircleMud License                       *
 * Written by Elie Rosenblum <fnord@cosanostra.net>                        *
 * Copyright (c) 7-Oct-2004                                                *
 ***************************************************************************/
#pragma once

#include "structs.h"



/* Magic constants: */
/* Don't change these unless you know what you're doing, the constants must
 * match */

#define HTREE_NODE_BITS 4
#define HTREE_NODE_SUBS (1 << HTREE_NODE_BITS)
#define HTREE_NODE_MASK (HTREE_NODE_SUBS - 1)
#define HTREE_MAX_DEPTH (((sizeof(IDXTYPE) * 8) / HTREE_NODE_BITS) + 1)

/* End of magic constants */

struct htree_node {
    IDXTYPE content;
    struct htree_node *parent;
    struct htree_node *subs[HTREE_NODE_SUBS];
};

extern struct htree_node *HTREE_NULL;
extern int htree_total_nodes;
extern int htree_depth_used;

extern void htree_shutdown();

struct htree_node *htree_init();

extern void htree_free(struct htree_node *root);

extern void htree_add(struct htree_node *root, IDXTYPE index, IDXTYPE content);

extern void htree_del(struct htree_node *root, IDXTYPE index);

extern IDXTYPE htree_find(struct htree_node *root, IDXTYPE index);

extern void htree_test();
