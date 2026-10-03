#include "List.h"

/* PUBLIC FUNCTIONS */

List * appendToList(List * list, void * element) {
	ListNode * node = calloc(1, sizeof(ListNode));
	node->element = element;
	node->next = NULL;
	if (list->last == NULL) {
		list->first = node;
	}
	else {
		list->last->next = node;
	}
	list->last = node;
	++list->size;
	return list;
}

List * createList(void) {
	return calloc(1, sizeof(List));
}

void destroyList(List * list, ElementDestructor destructor) {
	if (list != NULL) {
		ListNode * node = list->first;
		while (node != NULL) {
			ListNode * next = node->next;
			if (destructor != NULL) {
				destructor(node->element);
			}
			free(node);
			node = next;
		}
		free(list);
	}
}
