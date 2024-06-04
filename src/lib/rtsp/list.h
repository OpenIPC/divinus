#ifndef _RTSP_LIST_H_
#define _RTSP_LIST_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

struct list_t {
    int (*cleaner)(struct list_t *);
    struct list_t *next;
};

struct list_head_t {
    struct list_t *list;
};

#define list_upcast(p,l) ({ p = container_of(typeof(*p),l,list_entry);})

static inline void list_destroy(struct list_head_t *head);
static inline int list_add(struct list_head_t *head, struct list_t *elem);
static inline int list_push(struct list_head_t *head, struct list_t *elem);
static inline struct list_t *list_pop(struct list_head_t *head);
static inline int list_del(struct list_head_t *head,struct list_t *item);
static inline int list_map(struct list_head_t *head,int (*func)(struct list_t *,void *),void *param);
static inline int list_consume(struct list_head_t *head, int (*func)(struct list_t *,void *),void *param);
static inline int list_sweep(struct list_head_t *head, int (*is_dead)(struct list_t *));
static inline struct list_t *list_select(struct list_head_t *head, int (*cond)(struct list_t *,void *), void *state);
static inline int list_length(struct list_head_t *head);

static inline int _list_length_tc(struct list_t *list, int len)
{
    if(!list) return len; 
    return _list_length_tc(list->next, len + 1);
}

/* O(n):query length of the list by recursive tail call. 
   (potential performance penalty on -O0 compiler flag) */
static inline int list_length(struct list_head_t *head)
{
    return _list_length_tc(head->list,0);
}

/* O(n):query all the elems in given list by 'is_dead' callback,
   successively delete the  elem if 'is_dead' returned non-zero */
static inline int list_sweep(struct list_head_t *head, int (*is_dead)(struct list_t *))
{
    struct list_t *p,*prev=NULL;

    p = head->list;

    while(p) {
        if(is_dead(p)) {

            list_del(head,p);

            if(prev) {
                p = prev->next;
            } else {
                p = head->list;
            }
        } else {
            prev = p;
            p = p->next;
        }
    }

    return SUCCESS;
}
        
/* O(n):perform callback function 'func' with parameter 'param' iteratively,
   deleting processed entry on each step, head to tail. abort if failure detected */
static inline int list_consume(struct list_head_t *head, int (*func)(struct list_t *,void *),void *param)
{
    while(head->list != NULL){
        ASSERT(func(head->list,param) == SUCCESS, return FAILURE);
        list_del(head, head->list);
    }

    return SUCCESS;
}

/* O(n):perform callback function 'func' with parameter 'param' iteratively. */
static inline int list_map(struct list_head_t *head,int (*func)(struct list_t *,void *),void *param)
{
    struct list_t *p;
    p = head->list;
    while(p) {
        ASSERT(func(p,param)==SUCCESS,return FAILURE);
        p = p->next;
    }
    return SUCCESS;
}

/* O(n): macro version of list_map to inline callback function */
#define list_map_inline(__head,__func,__param) ({ \
    __label__ __done; \
    struct list_t *__p; \
    int __ret = FAILURE; \
    __p = (__head)->list; \
    while(__p) { \
        ASSERT(__func(__p,__param)==SUCCESS,goto __done); \
        __p = __p->next; \
    } \
    __ret = SUCCESS; \
__done: \
    __ret; \
})

/* O(n):apply list_del until destroy all elems.
   we assume all the deletion must succeed as long as an elem was given :p */
static inline void list_destroy(struct list_head_t *head)
{
    while(list_del(head, head->list)==SUCCESS);
}

/* O(n):deallocate a given elem. if deallocator is set, call it. otherwise
   do nothing (we suppose another allocator do handle it for goodness) */
static inline int list_del(struct list_head_t *head,struct list_t *item)
{
    DASSERT(head,return FAILURE);

    if(item == NULL) return FAILURE;

    struct list_t *list = head->list;
    struct list_t *prev= NULL;

    while(list!=NULL) {

        if(item == list){
            if(prev==NULL) {
                head->list = item->next;
            } else {
                prev->next = item->next;
            }

            if(item->cleaner != NULL) {
                ASSERT(item->cleaner(item)==SUCCESS,return FAILURE);
            }

            return SUCCESS;
        }

        prev = list;
        list = list->next;
    }

    ERR("cannot reacn\n");
    return FAILURE;
}

/* O(n):add  elem to list (FIFO). we do not alloc any memory here */
static inline int list_add(struct list_head_t *head, struct list_t *elem)
{
    struct list_t **pp_list = &(head->list);

    while(*pp_list!=NULL) {
        pp_list = &((*pp_list)->next);
    }

    *pp_list = elem;
    elem->next = NULL;

    return SUCCESS;
}

/* O(1):add  elem to list (LIFO). we do not alloc any memory here */
static inline int list_push(struct list_head_t *head, struct list_t *elem)
{
    struct list_t *old_list = head->list;
    head->list = elem;
    elem->next = old_list;

    return SUCCESS;
}

/* O(1):pop elem from list (LIFO). NOTE we simply detach an element, no clean, no free occurs*/
static inline struct list_t *list_pop(struct list_head_t *head)
{
    struct list_t *ret = NULL;
    
    ret = head->list;
    if(ret) {
        head->list = ret->next;
        ret->next = NULL;
    }

    return ret;
}

/* O(n):apply 'cond' callback to elems iteratively, if 'cond' returns non-zero first time, then return the elem as "selected" */
static inline struct list_t *list_select(struct list_head_t *head, int (*cond)(struct list_t *,void *), void *state)
{
    struct list_t *p;

    p = head->list;

    while(p) {
        if(cond(p,state)) {
            return  p;
        }
        p = p->next;
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif
#endif