/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2014 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2007 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Voltaire All rights reserved.
 * Copyright (c) 2019      Intel, Inc.  All rights reserved.
 * Copyright (c) 2020      Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2021      Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "prte_config.h"

#include "constants.h"
#include "src/class/prte_list.h"

/*
 *  List classes
 */

static void prte_list_item_construct(prte_list_item_t *);
static void prte_list_item_destruct(prte_list_item_t *);

PRTE_CLASS_INSTANCE(prte_list_item_t, prte_object_t, prte_list_item_construct,
                    prte_list_item_destruct);

static void prte_list_construct(prte_list_t *);
static void prte_list_destruct(prte_list_t *);

PRTE_CLASS_INSTANCE(prte_list_t, prte_object_t, prte_list_construct, prte_list_destruct);

/*
 *
 *      prte_list_link_item_t interface
 *
 */

static void prte_list_item_construct(prte_list_item_t *item)
{
    item->prte_list_next = item->prte_list_prev = NULL;
    item->item_free = 1;
#if PRTE_ENABLE_DEBUG
    item->prte_list_item_refcount = 0;
    item->prte_list_item_belong_to = NULL;
#endif
}

static void prte_list_item_destruct(prte_list_item_t *item)
{
#if PRTE_ENABLE_DEBUG
    assert(0 == item->prte_list_item_refcount);
    assert(NULL == item->prte_list_item_belong_to);
#endif /* PRTE_ENABLE_DEBUG */
}

/*
 *
 *      prte_list_list_t interface
 *
 */

static void prte_list_construct(prte_list_t *list)
{
#if PRTE_ENABLE_DEBUG
    /* These refcounts should never be used in assertions because they
       should never be removed from this list, added to another list,
       etc.  So set them to sentinel values. */

    PRTE_CONSTRUCT(&(list->prte_list_sentinel), prte_list_item_t);
    list->prte_list_sentinel.prte_list_item_refcount = 1;
    list->prte_list_sentinel.prte_list_item_belong_to = list;
#endif

    list->prte_list_sentinel.prte_list_next = &list->prte_list_sentinel;
    list->prte_list_sentinel.prte_list_prev = &list->prte_list_sentinel;
    list->prte_list_length = 0;
}

/*
 * Reset all the pointers to be NULL -- do not actually destroy
 * anything.
 */
static void prte_list_destruct(prte_list_t *list)
{
    prte_list_construct(list);
}

/*
 * Insert an item at a specific place in a list
 */
bool prte_list_insert(prte_list_t *list, prte_list_item_t *item, long long idx)
{
    /* Adds item to list at index and retains item. */
    int i;
    volatile prte_list_item_t *ptr, *next;

    if (idx >= (long long) list->prte_list_length) {
        return false;
    }

    if (0 == idx) {
        prte_list_prepend(list, item);
    } else {
#if PRTE_ENABLE_DEBUG
        /* Spot check: ensure that this item is previously on no
           lists */

        assert(0 == item->prte_list_item_refcount);
#endif
        /* pointer to element 0 */
        ptr = list->prte_list_sentinel.prte_list_next;
        for (i = 0; i < idx - 1; i++)
            ptr = ptr->prte_list_next;

        next = ptr->prte_list_next;
        item->prte_list_next = next;
        item->prte_list_prev = ptr;
        next->prte_list_prev = item;
        ptr->prte_list_next = item;

#if PRTE_ENABLE_DEBUG
        /* Spot check: ensure this item is only on the list that we
           just insertted it into */

        prte_atomic_add(&(item->prte_list_item_refcount), 1);
        assert(1 == item->prte_list_item_refcount);
        item->prte_list_item_belong_to = list;
#endif
    }

    list->prte_list_length++;
    return true;
}

static void prte_list_transfer(prte_list_item_t *pos, prte_list_item_t *begin,
                               prte_list_item_t *end)
{
    volatile prte_list_item_t *tmp;

    if (pos != end) {
        /* remove [begin, end) */
        end->prte_list_prev->prte_list_next = pos;
        begin->prte_list_prev->prte_list_next = end;
        pos->prte_list_prev->prte_list_next = begin;

        /* splice into new position before pos */
        tmp = pos->prte_list_prev;
        pos->prte_list_prev = end->prte_list_prev;
        end->prte_list_prev = begin->prte_list_prev;
        begin->prte_list_prev = tmp;
#if PRTE_ENABLE_DEBUG
        {
            volatile prte_list_item_t *item = begin;
            while (pos != item) {
                item->prte_list_item_belong_to = pos->prte_list_item_belong_to;
                item = item->prte_list_next;
                assert(NULL != item);
            }
        }
#endif /* PRTE_ENABLE_DEBUG */
    }
}

void prte_list_join(prte_list_t *thislist, prte_list_item_t *pos, prte_list_t *xlist)
{
    if (0 != prte_list_get_size(xlist)) {
        prte_list_transfer(pos, prte_list_get_first(xlist), prte_list_get_end(xlist));

        /* fix the sizes */
        thislist->prte_list_length += xlist->prte_list_length;
        xlist->prte_list_length = 0;
    }
}

void prte_list_splice(prte_list_t *thislist, prte_list_item_t *pos, prte_list_t *xlist,
                      prte_list_item_t *first, prte_list_item_t *last)
{
    size_t change = 0;
    prte_list_item_t *tmp;

    if (first != last) {
        /* figure out how many things we are going to move (have to do
         * first, since last might be end and then we wouldn't be able
         * to run the loop)
         */
        for (tmp = first; tmp != last; tmp = prte_list_get_next(tmp)) {
            change++;
        }

        prte_list_transfer(pos, first, last);

        /* fix the sizes */
        thislist->prte_list_length += change;
        xlist->prte_list_length -= change;
    }
}

int prte_list_sort(prte_list_t *list, prte_list_item_compare_fn_t compare)
{
    prte_list_item_t *item;
    prte_list_item_t **items;
    size_t i, index = 0;

    if (0 == list->prte_list_length) {
        return PRTE_SUCCESS;
    }
    items = (prte_list_item_t **) malloc(sizeof(prte_list_item_t *) * list->prte_list_length);

    if (NULL == items) {
        return PRTE_ERR_OUT_OF_RESOURCE;
    }

    while (NULL != (item = prte_list_remove_first(list))) {
        items[index++] = item;
    }

    qsort(items, index, sizeof(prte_list_item_t *), (int (*)(const void *, const void *)) compare);
    for (i = 0; i < index; i++) {
        prte_list_append(list, items[i]);
    }
    free(items);
    return PRTE_SUCCESS;
}
