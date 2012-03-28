
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t  *list;

    void *slice = ngx_palloc(pool, sizeof(ngx_list_t) +
	sizeof(ngx_list_part_t) + n * size);
    if (slice == NULL) {
        return NULL;
    }
    list = (ngx_list_t *) slice;
    /* discard real pointer size in arithmetic */
    list->partp = (ngx_list_part_t *)((char *) list
	+ sizeof(ngx_list_t));
    list->partp->elts = (void *)((char *) list->partp +
	sizeof(ngx_list_part_t));
    list->partp->nelts = 0;
    list->partp->next = NULL;
    list->partp->prev = NULL;
    
    ngx_memcpy(&list->part, list->partp, sizeof(
	ngx_list_part_t));
    
    list->last = list->partp;
    list->intl.last_old = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return list;
}


void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {
	ngx_list_part_t *prev;
	size_t size = sizeof(ngx_list_part_t) + l->nalloc * l->size;
	
	if ( (u_char *)((char *) l->pool->d.last + size) >= l->pool->d.end )
	    return NULL;

        /* the last part is full, allocate a new list part */

	void *slice = ngx_palloc(l->pool, size);
        if (slice == NULL) {
            return NULL;
        }

	prev = last;
	last = (ngx_list_part_t *) slice;
        last->elts = (void *)((char *) last + sizeof(ngx_list_part_t));

        last->nelts = 0;
        last->next = NULL;
	last->prev = prev;

        l->last->next = last;
        l->last = last;

	if (l->intl.last_old) {
	    l->intl.last_old->next = last;
	    l->intl.last_old = NULL;
	}
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;
    
    if (l->intl.last_old)
	l->intl.last_old->nelts = last->nelts;
    
    return elt;
}
