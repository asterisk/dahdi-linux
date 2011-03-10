#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <xlist.h>

struct xlist_node *xlist_new(void *data)
{
	struct xlist_node	*list;

	if((list = malloc(sizeof(*list))) == NULL)
		return NULL;
	list->next = list;
	list->prev = list;
	list->data = data;
	return list;
}

void xlist_destroy(struct xlist_node *list, xlist_destructor_t destructor)
{
	struct xlist_node	*curr;
	struct xlist_node	*next;

	if (! list)
		return;
	curr = list->next;
	while(curr != list) {
		next = curr->next;
		if(destructor)
			destructor(curr->data);
		memset(curr, 0, sizeof(*curr));
		free(curr);
		curr = next;
	}
	memset(list, 0, sizeof(*list));
	free(list);
}

void xlist_append_item(struct xlist_node *list, struct xlist_node *item)
{
	assert(list);
	assert(xlist_empty(item));
	item->next = list;
	item->prev = list->prev;
	list->prev->next = item;
	list->prev = item;
}

void xlist_prepend_item(struct xlist_node *list, struct xlist_node *item)
{
	assert(list);
	assert(xlist_empty(item));
	item->prev = list;
	item->next = list->next;
	list->next->prev = item;
	list->next = item;
}

void xlist_remove_item(struct xlist_node *item)
{
	assert(item);
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->next = item->prev = item;
}

struct xlist_node *xlist_shift(struct xlist_node *list)
{
	struct xlist_node	*item;

	if(!list)
		return NULL;
	if(xlist_empty(list))
		return NULL;
	item = list->next;
	xlist_remove_item(item);
	return item;
}

int xlist_empty(const struct xlist_node *list)
{
	assert(list);
	return list->next == list && list->prev == list;
}

size_t xlist_length(const struct xlist_node *list)
{
	struct xlist_node	*curr;
	size_t			count = 0;

	for(curr = list->next; curr != list; curr = curr->next)
		count++;
	return count;
}
