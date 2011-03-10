#ifndef	XLIST_H
#define	XLIST_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct xlist_node {
	void			*data;
	struct xlist_node	*next;
	struct xlist_node	*prev;
};

typedef void (*xlist_destructor_t)(void *data);

struct xlist_node *xlist_new(void *data);
void xlist_destroy(struct xlist_node *list, xlist_destructor_t destructor);
void xlist_append_item(struct xlist_node *list, struct xlist_node *item);
void xlist_remove_item(struct xlist_node *item);
struct xlist_node *xlist_shift(struct xlist_node *list);
int xlist_empty(const struct xlist_node *list);
size_t xlist_length(const struct xlist_node *list);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* XLIST_H */
