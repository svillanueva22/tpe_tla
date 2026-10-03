#ifndef LIST_HEADER
#define LIST_HEADER

#include <stdlib.h>

/**
 * A generic, singly-linked list with constant-time append. It keeps insertion
 * order, which is relevant for the AST (e.g., the declared order of teams,
 * tiebreak criteria or statements).
 */

typedef struct ListNode ListNode;

struct ListNode {
	void * element;
	ListNode * next;
};

typedef struct {
	ListNode * first;
	ListNode * last;
	unsigned int size;
} List;

/**
 * Destroys a single element of a list.
 */
typedef void (*ElementDestructor)(void * element);

/**
 * Appends an element at the end of the list, and returns the same list.
 */
List * appendToList(List * list, void * element);

/**
 * Creates a new empty list, using heap-memory.
 */
List * createList(void);

/**
 * Destroys the list and every element on it with the provided destructor. If
 * the destructor is NULL, the elements are not released. Accepts NULL.
 */
void destroyList(List * list, ElementDestructor destructor);

#endif
