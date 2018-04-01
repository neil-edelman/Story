/** 2016 Neil Edelman, distributed under the terms of the MIT License;
 see readme.txt, or \url{ https://opensource.org/licenses/MIT }.

 {<T>Pool} is a dynamic array that stores unordered {<T>}, which must be set
 using {POOL_TYPE}. Removing an element is done lazily through a linked-list
 internal to the pool; as such, indices will remain the same throughout the
 lifetime of the data. You cannot shrink the capacity of this data type, only
 cause it to grow. Resizing incurs amortised cost, done though a Fibonacci
 sequence. VV!

 Intended for use in backing polymorphic data-types. {<T>Pool} is not
 synchronised. The preprocessor macros are all undefined at the end of the file
 for convenience.

 @param POOL_NAME
 This literally becomes {<T>}. As it's used in function names, this should
 comply with naming rules and be unique; required.

 @param POOL_TYPE
 The type associated with {<T>}. Has to be a valid type, accessible to the
 compiler at the time of inclusion; required.

 @param POOL_MIGRATE_EACH
 Optional function implementing {<PT>Migrate}. On memory move, this definition
 will call {POOL_MIGRATE_EACH} with all {<T>} in {<T>Pool}. Use when your data
 is self-referential, like a linked-list.

 @param POOL_MIGRATE_ALL
 Optional type {<A>}. When one may have pointers to the data that is contained
 in the {Pool} outside the data that can be accessed by the pool. It adds an
 element to the constructor, {<PT>MigrateAll migrate_all}, as well as it's
 constant parameter, {<A> all}. This usually is the parent of an agglomeration
 that includes and has references into the pool. This has the responsibility to
 call \see{<T>MigratePointer} or some migrate function for all references.

 @param POOL_MIGRATE_UPDATE
 Optional type association with {<U>}. If set, the function
 \see{<T>PoolUpdateNew} becomes available, intended for a local iterator
 update on memory move.

 @param POOL_TO_STRING
 Optional print function implementing {<T>ToString}; makes available
 \see{<T>PoolToString}.

 @param POOL_DEBUG
 Prints information to {stderr}. Requires {POOL_TO_STRING}.

 @param POOL_TEST
 Unit testing framework using {<T>PoolTest}, included in a separate header,
 {../test/PoolTest.h}. Must be defined equal to a (random) filler function,
 satisfying {<T>Action}. If {NDEBUG} is not defined, turns on {assert} private
 function integrity testing. Requires {POOL_TO_STRING}.

 @title		Pool.h
 @std		C89
 @author	Neil
 @version	2018-02 Errno instead of custom errors.
 @since		2017-12 Introduced {POOL_PARENT} for type-safety.
			2017-10 Replaced {PoolIsEmpty} by {PoolElement}, much more useful.
			2017-10 Renamed Pool; made migrate automatic.
			2017-07 Made migrate simpler.
			2017-05 Split {List} from {Pool}; much simpler.
			2017-01 Almost-redundant functions simplified.
			2016-11 Multi-index.
			2016-08 Permute. */



#include <stddef.h>	/* ptrdiff_t */
#include <stdlib.h>	/* malloc realloc free qsort */
#include <assert.h>	/* assert */
#include <string.h>	/* memcpy (memmove strerror strcpy memcmp in PoolTest.h) */
#include <errno.h>	/* errno */
#ifdef POOL_TO_STRING /* <-- print */
#include <stdio.h>	/* snprintf */
#endif /* print --> */
#ifdef POOL_DEBUG /* <-- debug */
#include <stdarg.h>	/* for print debug */
#endif /* debug --> */



/* Check defines. */
#ifndef POOL_NAME
#error Pool generic POOL_NAME undefined.
#endif
#ifndef POOL_TYPE
#error Pool generic POOL_TYPE undefined.
#endif
#if (defined(POOL_DEBUG) || defined(POOL_TEST)) && !defined(POOL_TO_STRING)
#error Pool: POOL_DEBUG and POOL_TEST require POOL_TO_STRING.
#endif
#if !defined(POOL_TEST) && !defined(NDEBUG)
#define POOL_NDEBUG
#define NDEBUG
#endif



/* After this block, the preprocessor replaces T with POOL_TYPE, T_(X) with
 POOL_NAMEX, PT_(X) with POOL_U_NAME_X, and T_NAME with the string
 version. http://stackoverflow.com/questions/16522341/pseudo-generics-in-c */
#ifdef CAT
#undef CAT
#endif
#ifdef CAT_
#undef CAT_
#endif
#ifdef PCAT
#undef PCAT
#endif
#ifdef PCAT_
#undef PCAT_
#endif
#ifdef A
#undef A
#endif
#ifdef U
#undef U
#endif
#ifdef T
#undef T
#endif
#ifdef T_
#undef T_
#endif
#ifdef PT_
#undef PT_
#endif
#ifdef T_NAME
#undef T_NAME
#endif
#ifdef QUOTE
#undef QUOTE
#endif
#ifdef QUOTE_
#undef QUOTE_
#endif
#define CAT_(x, y) x ## y
#define CAT(x, y) CAT_(x, y)
#define PCAT_(x, y) x ## _ ## y
#define PCAT(x, y) PCAT_(x, y)
#define QUOTE_(name) #name
#define QUOTE(name) QUOTE_(name)
#define T_(thing) CAT(POOL_NAME, thing)
#define PT_(thing) PCAT(pool, PCAT(POOL_NAME, thing))
#define T_NAME QUOTE(POOL_NAME)

/* Troubles with this line? check to ensure that {POOL_TYPE} is a valid type,
 whose definition is placed above {#include "Pool.h"}. */
typedef POOL_TYPE PT_(Type);
#define T PT_(Type)



/* Constants across multiple includes in the same translation unit. */
#ifndef POOL_H /* <-- POOL_H */
#define POOL_H
static const size_t pool_fibonacci6 = 8;
static const size_t pool_fibonacci7 = 13;
/* Is a node that is not part of the deleted list; a different kind of null. */
static const size_t pool_void       = (size_t)-1;
/* Is a node on the edge of the deleted list. */
static const size_t pool_null       = (size_t)-2;
/* Maximum size; smaller then any special values. */
static const size_t pool_max        = (size_t)-3;
/* Removed offset queue. */
struct PoolX { size_t prev, next; };
#endif /* POOL_H --> */

/* One time in the same translation unit. */
#ifndef MIGRATE /* <-- migrate */
#define MIGRATE
/** Contains information about a {realloc}. */
struct Migrate;
struct Migrate {
	const void *begin, *end; /* Old pointers. */
	ptrdiff_t delta;
};
#endif /* migrate --> */



/** This is the migrate function for {<T>}. This definition is about the
 {POOL_TYPE} type, that is, it is without the prefix {Pool}; to avoid namespace
 collisions, this is private, meaning the name is mangled. If you want this
 definition, re-declare it. */
typedef void (*PT_(Migrate))(T *const data,
	const struct Migrate *const migrate);
#ifdef POOL_MIGRATE_EACH /* <-- migrate */
/* Check that {POOL_MIGRATE_EACH} is a function implementing {<PT>Migrate},
 whose definition is placed above {#include "Pool.h"}. */
static const PT_(Migrate) PT_(migrate_each) = (POOL_MIGRATE_EACH);
#endif /* migrate --> */

#ifdef POOL_MIGRATE_ALL /* <-- all */
/* Troubles with this line? check to ensure that {POOL_MIGRATE_ALL} is a
 valid type, whose definition is placed above {#include "Pool.h"}. */
typedef POOL_MIGRATE_ALL PT_(MigrateAllType);
#define A PT_(MigrateAllType)
/** Function call on {realloc} if {POOL_MIGRATE_ALL} is defined. This
 definition is about the {POOL_MIGRATE_ALL} type. */
typedef void (*PT_(MigrateAll))(A *const all,
	const struct Migrate *const migrate);
#endif /* all --> */

#ifdef POOL_MIGRATE_UPDATE /* <-- update */
/* Troubles with this line? check to ensure that {POOL_MIGRATE_UPDATE} is a
 valid type, whose definition is placed above {#include "Pool.h"}. */
typedef POOL_MIGRATE_UPDATE PT_(MigrateUpdateType);
#define U PT_(MigrateUpdateType)
#endif /* update --> */

#ifdef POOL_TO_STRING /* <-- string */
/** Responsible for turning {<T>} (the first argument) into a 12 {char}
 null-terminated output string (the second.) Used for {POOL_TO_STRING}. */
typedef void (*PT_(ToString))(const T *, char (*const)[12]);
/* Check that {POOL_TO_STRING} is a function implementing {<PT>ToString}, whose
 definition is placed above {#include "Pool.h"}. */
static const PT_(ToString) PT_(to_string) = (POOL_TO_STRING);
#endif /* string --> */

#ifdef POOL_TEST /* <-- test */
/* Operates by side-effects only. Used only for {POOL_TEST}. */
typedef void (*PT_(Action))(T *const data);
#endif /* test --> */



/* Pool nodes containing the data. */
struct PT_(Node) {
	T data;
	struct PoolX x;
};

/** The pool. To instantiate, see \see{<T>Pool}. */
struct T_(Pool);
struct T_(Pool) {
	struct PT_(Node) *nodes;
	size_t capacity[2]; /* Fibonacci, [0] is the capacity, [1] is next. */
	size_t size; /* Including removed. */
	struct PoolX removed;
#ifdef POOL_MIGRATE_ALL /* <-- all */
	PT_(MigrateAll) migrate_all; /* Called to update on resizing. */
	A *all; /* Migrate parameter. */
#endif /* all --> */
};



/** Private: {container_of}. */
static struct PT_(Node) *PT_(node_hold_data)(T *const data) {
	return (struct PT_(Node) *)(void *)
		((char *)data - offsetof(struct PT_(Node), data));
}

/** Debug messages from pool functions; turn on using {POOL_DEBUG}. */
static void PT_(debug)(struct T_(Pool) *const pool,
	const char *const fn, const char *const fmt, ...) {
#ifdef POOL_DEBUG
	/* \url{ http://c-faq.com/varargs/vprintf.html } */
	va_list argp;
	fprintf(stderr, "Pool<" T_NAME ">#%p.%s: ", (void *)pool, fn);
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
#else
	(void)(pool), (void)(fn), (void)(fmt);
#endif
}

/* * Ensures capacity.
 @return Success; otherwise, {errno} may be set.
 @throws ERANGE: Tried allocating more then can fit in {size_t}.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say. */
static int PT_(reserve)(struct T_(Pool) *const pool,
	const size_t min_capacity
#ifdef POOL_MIGRATE_UPDATE /* <-- update */
	, U **const update_ptr
#endif /* update --> */
	) {
	size_t c0, c1;
	struct PT_(Node) *nodes;
	const size_t max_size = pool_max / sizeof *nodes;
	assert(pool && pool->size <= pool->capacity[0]
		&& pool->capacity[0] <= pool->capacity[1]
		&& pool->capacity[1] <= pool_max
		&& pool->removed.next == pool->removed.prev
		&& pool->removed.next == pool_null);
	if(pool->capacity[0] >= min_capacity) return 1;
	if(max_size < min_capacity) return errno = ERANGE, 0;
	c0 = pool->capacity[0];
	c1 = pool->capacity[1];
	while(c0 < min_capacity) {
		c0 ^= c1, c1 ^= c0, c0 ^= c1, c1 += c0;
		if(c1 <= c0 || c1 > max_size) c1 = max_size;
	}
	if(!(nodes = realloc(pool->nodes, c0 * sizeof *pool->nodes))) return 0;
	PT_(debug)(pool, "reserve", "nodes#%p[%lu] -> #%p[%lu].\n",
		(void *)pool->nodes, (unsigned long)pool->capacity[0], (void *)nodes,
		(unsigned long)c0);
#if defined(POOL_MIGRATE_EACH) || defined(POOL_MIGRATE_ALL) \
	|| defined(POOL_MIGRATE_UPDATE) /* <-- migrate */
	if(pool->nodes != nodes) {
		/* Migrate data; violates pedantic strict-ANSI? */
		struct Migrate migrate;
		migrate.begin = pool->nodes;
		migrate.end   = (const char *)pool->nodes + pool->size * sizeof *nodes;
		migrate.delta = (const char *)nodes - (const char *)pool->nodes;
		PT_(debug)(pool, "reserve", "calling migrate.\n");
#ifdef POOL_MIGRATE_EACH /* <-- each: Self-referential data. */
		{
			struct PT_(Node) *e, *end;
			for(e = nodes, end = e + pool->size; e < end; e++) {
				if(e->x.prev != pool_void) continue; /* It's on removed list. */
				assert(e->x.next == pool_void);
				PT_(migrate_each)(&e->data, &migrate);
			}
		}
#endif /* each --> */
#ifdef POOL_MIGRATE_ALL /* <-- all: Random references. */
		if(pool->migrate_all) {
			assert(pool->all);
			pool->migrate_all(pool->all, &migrate);
		}
#endif /* all --> */
#ifdef POOL_MIGRATE_UPDATE /* <-- update: Usually iterator. */
		if(update_ptr) {
			const void *const u = *update_ptr;
			if(u >= migrate.begin && u < migrate.end)
				*(char **)update_ptr += migrate.delta;
		}
#endif /* update --> */
	}
#endif /* migrate --> */
	pool->nodes = nodes;
	pool->capacity[0] = c0;
	pool->capacity[1] = c1;
	return 1;
}

/** We are very lazy and we just enqueue the removed so that later data can
 overwrite it.
 @param n: Must be a valid index.
 @fixme Change the order of the data to fill the start first. Tricky, but cache
 performance will probably go up? Probably impossible to do in {O(1)}. */
static void PT_(enqueue_removed)(struct T_(Pool) *const pool, const size_t n) {
	struct PT_(Node) *const node = pool->nodes + n;
	assert(pool && n < pool->size);
	/* Cannot be part of the removed pool already. */
	assert(node->x.prev == pool_void && node->x.next == pool_void);
	if((node->x.prev = pool->removed.prev) == pool_null) {
		/* The first {<PT>Node} removed. */
		assert(pool->removed.next == pool_null);
		pool->removed.prev = pool->removed.next = n;
	} else {
		/* Stick it on the end. */
		struct PT_(Node) *const last = pool->nodes + pool->removed.prev;
		assert(last->x.next == pool_null);
		last->x.next = pool->removed.prev = n;
	}
	node->x.next = pool_null;
}

/** Dequeues a removed node, or if the queue is empty, returns null. */
static struct PT_(Node) *PT_(dequeue_removed)(struct T_(Pool) *const pool) {
	struct PT_(Node) *node;
	size_t n;
	assert(pool &&
		(pool->removed.next == pool_null) == (pool->removed.prev == pool_null));
	if((n = pool->removed.next) == pool_null) return 0; /* No nodes removed. */
	node = pool->nodes + n;
	assert(node->x.prev == pool_null && node->x.next != pool_void);
	if((pool->removed.next = node->x.next) == pool_null) {
		/* The last element. */
		pool->removed.prev = pool->removed.next = pool_null;
	} else {
		struct PT_(Node) *const next = pool->nodes + node->x.next;
		assert(node->x.next < pool->size && next->x.prev == n);
		next->x.prev = pool_null;
	}
	node->x.prev = node->x.next = pool_void;
	return node;
}

/** Gets rid of the removed node at the tail of the list. Each remove has
 potentially one delete in the worst case, it just gets amotized a bit. */
static void PT_(trim_removed)(struct T_(Pool) *const pool) {
	struct PT_(Node) *node, *prev, *next;
	size_t n;
	assert(pool);
	while(pool->size
		&& (node = pool->nodes + (n = pool->size - 1))->x.prev != pool_void) {
		assert(node->x.next != pool_void);
		if(node->x.prev == pool_null) { /* First. */
			assert(pool->removed.next == n);
			pool->removed.next = node->x.next;
		} else {
			assert(node->x.prev < pool->size);
			prev = pool->nodes + node->x.prev;
			prev->x.next = node->x.next;
		}
		if(node->x.next == pool_null) { /* Last. */
			assert(pool->removed.prev == n);
			pool->removed.prev = node->x.prev;
		} else {
			assert(node->x.next < pool->size);
			next = pool->nodes + node->x.next;
			next->x.prev = node->x.prev;
		}
		pool->size--;
	}
}

/** Destructor for {Pool}. Make sure that the pool's contents will not be
 accessed anymore.
 @param ppool: A reference to the object that is to be deleted; it will be set
 to null. If it is already null or it points to null, doesn't do anything.
 @order \Theta(1)
 @allow */
static void T_(Pool_)(struct T_(Pool) **const ppool) {
	struct T_(Pool) *pool;
	if(!ppool || !(pool = *ppool)) return;
	PT_(debug)(pool, "Delete", "erasing.\n");
	free(pool->nodes);
	free(pool);
	*ppool = 0;
}

/** Private constructor called from either \see{<T>Pool}.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say. */
static struct T_(Pool) *PT_(pool)(void) {
	struct T_(Pool) *pool;
	assert(pool_max < pool_null && pool_max < pool_void);
	if(!(pool = malloc(sizeof *pool))) return 0;
	pool->nodes        = 0;
	pool->capacity[0]  = pool_fibonacci6;
	pool->capacity[1]  = pool_fibonacci7;
	pool->size         = 0;
	pool->removed.prev = pool->removed.next = pool_null;
	if(!(pool->nodes = malloc(pool->capacity[0] * sizeof *pool->nodes)))
		{ T_(Pool_)(&pool); return 0; }
	PT_(debug)(pool, "New", "capacity %d.\n", pool->capacity[0]);
	return pool;
}

#ifdef POOL_MIGRATE_ALL /* <-- all */
/** Constructs an empty {Pool} with capacity Fibonacci6, which is 8. This is
 the constructor if {POOL_MIGRATE_ALL} is specified.
 @param migrate_all: The general {<PT>MigrateAll} function.
 @param all: The general migrate parameter; required if {migrate_all} is
 specified.
 @return A new {Pool} or null and {errno} may be set.
 @throws ERANGE: If one and not the other arguments is null.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say.
 @order \Theta(1)
 @allow */
static struct T_(Pool) *T_(Pool)(const PT_(MigrateAll) migrate_all,
	A *const all) {
	struct T_(Pool) *pool;
	if(!migrate_all ^ !all) { errno = ERANGE; return 0; }
	if(!(pool = PT_(pool)())) return 0; /* ENOMEM? */
	pool->migrate_all = migrate_all;
	pool->all         = all;
	return pool;
}
#else /* all --><-- !all */
/** Constructs an empty {Pool} with capacity Fibonacci6, which is 8.
 @return A new {Pool} or null and {errno} may be set.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say.
 @order \Theta(1)
 @allow */
static struct T_(Pool) *T_(Pool)(void) {
	return PT_(pool)(); /* ENOMEM? */
}
#endif /* all --> */

/** @param pool: If null, returns null.
 @return One element from the pool or null if the pool is empty. It selects
 the position deterministically.
 @order \Theta(1)
 @fixme Untested.
 @allow */
static T *T_(PoolElement)(const struct T_(Pool) *const pool) {
	if(!pool || !pool->size) return 0;
	return &pool->nodes[pool->size - 1].data;
}

/** Is {idx} a valid index for {pool}?
 @order \Theta(1)
 @allow */
static int T_(PoolIsElement)(struct T_(Pool) *const pool, const size_t idx) {
	struct PT_(Node) *data;
	if(!pool) return 0;
	if(idx >= pool->size
		|| (data = pool->nodes + idx, data->x.prev != pool_void)) return 0;
	return 1;
}

/** Gets an existing element by index. Causing something to be added to the
 {<T>Pool} may invalidate this pointer.
 @param pool: If {pool} is null, returns null.
 @param idx: Index.
 @return If failed, returns a null pointer {errno} will be set.
 @throws EDOM: {idx} out of bounds.
 @order \Theta(1)
 @allow */
static T *T_(PoolGetElement)(struct T_(Pool) *const pool, const size_t idx) {
	struct PT_(Node) *node;
	if(!pool) return 0;
	if(idx >= pool->size
		|| (node = pool->nodes + idx, node->x.prev != pool_void))
		{ errno = EDOM; return 0; }
	return &node->data;
}

/** Gets an index given {data}.
 @param data: If the element is not part of the {Pool}, behaviour is undefined.
 @return An index.
 @order \Theta(1)
 @fixme Untested.
 @allow */
static size_t T_(PoolGetIndex)(struct T_(Pool) *const pool, T *const data) {
	return PT_(node_hold_data)(data) - pool->nodes;
}

/** Increases the capacity of this Pool to ensure that it can hold at least
 the number of elements specified by {min_capacity}.
 @param pool: If {pool} is null, returns false.
 @return True if the capacity increase was viable; otherwise the pool is not
 touched and {errno} may be set.
 @throws ERANGE: Tried allocating more then can fit in {size_t} objects.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say.
 @order \Omega(1), O({capacity})
 @allow */
static int T_(PoolReserve)(struct T_(Pool) *const pool,
	const size_t min_capacity) {
	if(!pool) return 0;
	if(!PT_(reserve)(pool, min_capacity
#ifdef POOL_MIGRATE_UPDATE /* <-- update */
		, 0
#endif /* update --> */
		)) return 0; /* ERANGE, ENOMEM? */
	PT_(debug)(pool, "Reserve", "pool size to %u to contain %u.\n",
		pool->capacity[0], min_capacity);
	return 1;
}

/** Gets an uninitialised new element. May move the {Pool} to a new memory
 location to fit the new size.
 @param pool: If is null, returns null.
 @return A new, un-initialised, element, or null and {errno} may be set.
 @throws ERANGE: Tried allocating more then can fit in {size_t} objects.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say.
 @order amortised O(1)
 @allow */
static T *T_(PoolNew)(struct T_(Pool) *const pool) {
	struct PT_(Node) *node;
	if(!pool) return 0;
	if(!(node = PT_(dequeue_removed)(pool))) {
		if(!PT_(reserve)(pool, pool->size + 1
#ifdef POOL_MIGRATE_ALL /* <-- all */
			, 0
#endif /* all --> */
			)) return 0; /* ERANGE, ENOMEM? */
		node = pool->nodes + pool->size++;
		node->x.prev = node->x.next = pool_void;
	}
	PT_(debug)(pool, "New", "added.\n");
	return &node->data;
}

#ifdef POOL_MIGRATE_UPDATE /* <-- update */
/** Gets an uninitialised new element and updates the {update_ptr} if it is
 within the memory region that was changed. Must have {POOL_MIGRATE_UPDATE}
 defined. For example, when iterating a pointer and new element is needed that
 could change the pointer.
 @param pool: If {pool} is null, returns null.
 @param iterator_ptr: Pointer to update on memory move.
 @return A new, un-initialised, element, or null and {errno} may be set.
 @throws ERANGE: Tried allocating more then can fit in {size_t}.
 @throws ENOMEM?: {IEEE Std 1003.1-2001}; C standard does not say.
 @order amortised O(1)
 @fixme Untested.
 @allow */
static T *T_(PoolUpdateNew)(struct T_(Pool) *const pool,
	U **const update_ptr) {
	struct PT_(Node) *node;
	if(!pool) return 0;
	if(!(node = PT_(dequeue_removed)(pool))) {
		if(!PT_(reserve)(pool, pool->size + 1, update_ptr))
			return 0; /* ERANGE, ENOMEM? */
		node = pool->nodes + pool->size++;
		node->prev = node->next = pool_void;
	}
	PT_(debug)(pool, "New", "added.\n");
	return &node->data;
}
#endif /* update --> */

/** Removes {data} from {pool}.
 @param pool, data: If null, returns false.
 @return Success, otherwise {errno} will be set.
 @throws EDOM: {data} is not part of {pool}.
 @order amortised O(1)
 @allow */
static int T_(PoolRemove)(struct T_(Pool) *const pool, T *const data) {
	struct PT_(Node) *node;
	size_t n;
	if(!pool || !data) return 0;
	node = PT_(node_hold_data)(data);
	n = node - pool->nodes;
	if(node < pool->nodes || n >= pool->size || node->x.prev != pool_void)
		return errno = EDOM, 0;
	PT_(enqueue_removed)(pool, n);
	if(n >= pool->size - 1) PT_(trim_removed)(pool);
	PT_(debug)(pool, "Remove", "removing %lu.\n", (unsigned long)n);
	return 1;
}

/** Removes all from {pool}.
 @param pool: if null, does nothing.
 @order \Theta(1)
 @allow */
static void T_(PoolClear)(struct T_(Pool) *const pool) {
	if(!pool) return;
	pool->size = 0;
	pool->removed.prev = pool->removed.next = pool_null;
	PT_(debug)(pool, "Clear", "cleared.\n");
}

/** @param pool: If null, returns false.
 @return Whether the {<T>Pool} is empty.
 @order \Theta(1)
 @fixme Untested.
 @allow */
static int T_(PoolIsEmpty)(const struct T_(Pool) *const pool) {
	if(!pool) return 0;
	return pool->size == 0;
}

/** Use when the pool has pointers to another pool in the {MigrateAll} function
 of the other data type.
 @param pool: If null, does nothing.
 @param handler: If null, does nothing, otherwise has the responsibility of
 calling the other data type's migrate pointer function on all pointers
 affected by the {realloc}.
 @param migrate: If null, does nothing. Should only be called in a {Migrate}
 function; pass the {migrate} parameter.
 @order O({greatest size})
 @fixme Untested.
 @allow */
static void T_(PoolMigrateEach)(struct T_(Pool) *const pool,
	const PT_(Migrate) handler, const struct Migrate *const migrate) {
	struct PT_(Node) *node, *end;
	if(!pool || !migrate || !handler) return;
	for(node = pool->nodes, end = node + pool->size; node < end; node++)
		if(node->x.prev == pool_void)
			assert(node->x.next == pool_void), handler(&node->data, migrate);
}

/** Passed a {migrate} parameter, allows pointers to the pool to be updated. It
 doesn't affect pointers not in the {realloc}ed region.
 @order \Omega(1)
 @fixme Untested.
 @allow */
static void T_(PoolMigratePointer)(T *const*const data_ptr,
	const struct Migrate *const migrate) {
	const void *ptr;
	if(!data_ptr
		|| !(ptr = *data_ptr)
		|| ptr < migrate->begin
		|| ptr >= migrate->end) return;
	*(char **)data_ptr += migrate->delta;
}

#ifdef POOL_TO_STRING /* <-- print */

#ifndef POOL_PRINT_THINGS /* <-- once inside translation unit */
#define POOL_PRINT_THINGS

static const char *const pool_cat_start     = "[ ";
static const char *const pool_cat_end       = " ]";
static const char *const pool_cat_alter_end = "...]";
static const char *const pool_cat_sep       = ", ";
static const char *const pool_cat_star      = "*";
static const char *const pool_cat_null      = "null";

struct Pool_SuperCat {
	char *print, *cursor;
	size_t left;
	int is_truncated;
};
static void pool_super_cat_init(struct Pool_SuperCat *const cat,
	char *const print, const size_t print_size) {
	cat->print = cat->cursor = print;
	cat->left = print_size;
	cat->is_truncated = 0;
	print[0] = '\0';
}
static void pool_super_cat(struct Pool_SuperCat *const cat,
	const char *const append) {
	size_t lu_took; int took;
	if(cat->is_truncated) return;
	took = sprintf(cat->cursor, "%.*s", (int)cat->left, append);
	if(took < 0)  { cat->is_truncated = -1; return; } /*implementation defined*/
	if(took == 0) { return; }
	if((lu_took = (size_t)took) >= cat->left)
		cat->is_truncated = -1, lu_took = cat->left - 1;
	cat->cursor += lu_took, cat->left -= lu_took;
}
#endif /* once --> */

/** Can print 4 things at once before it overwrites. One must pool
 {POOL_TO_STRING} to a function implementing {<T>ToString} to get this
 functionality.
 @return Prints {pool} in a static buffer.
 @order \Theta(1); it has a 255 character limit; every element takes some of it.
 @allow */
static const char *T_(PoolToString)(const struct T_(Pool) *const pool) {
	static char buffer[4][256];
	static unsigned buffer_i;
	struct Pool_SuperCat cat;
	int is_first = 1;
	char scratch[12];
	size_t i;
	assert(strlen(pool_cat_alter_end) >= strlen(pool_cat_end));
	assert(sizeof buffer > strlen(pool_cat_alter_end));
	pool_super_cat_init(&cat, buffer[buffer_i],
		sizeof *buffer / sizeof **buffer - strlen(pool_cat_alter_end));
	buffer_i++, buffer_i &= 3;
	if(!pool) {
		pool_super_cat(&cat, pool_cat_null);
		return cat.print;
	}
	pool_super_cat(&cat, pool_cat_start);
	for(i = 0; i < pool->size; i++) {
		if(pool->nodes[i].x.prev != pool_void) continue;
		if(!is_first) pool_super_cat(&cat, pool_cat_sep); else is_first = 0;
		PT_(to_string)(&pool->nodes[i].data, &scratch),
		scratch[sizeof scratch - 1] = '\0';
		pool_super_cat(&cat, scratch);
		if(cat.is_truncated) break;
	}
	sprintf(cat.cursor, "%s",
		cat.is_truncated ? pool_cat_alter_end : pool_cat_end);
	return cat.print; /* static buffer */
}

#endif /* print --> */

#ifdef POOL_TEST /* <-- test */
#include "../test/TestPool.h" /* Need this file if one is going to run tests. */
#endif /* test --> */

/* Prototype. */
static void PT_(unused_coda)(void);
/** This silences unused function warnings from the pre-processor, but allows
 optimisation, (hopefully.)
 \url{ http://stackoverflow.com/questions/43841780/silencing-unused-static-function-warnings-for-a-section-of-code } */
static void PT_(unused_set)(void) {
	T_(Pool_)(0);
#ifdef POOL_MIGRATE_ALL
	T_(Pool)(0, 0);
#else
	T_(Pool)();
#endif
	T_(PoolElement)(0);
	T_(PoolIsElement)(0, (size_t)0);
	T_(PoolGetElement)(0, (size_t)0);
	T_(PoolGetIndex)(0, 0);
	T_(PoolReserve)(0, (size_t)0);
	T_(PoolNew)(0);
#ifdef POOL_MIGRATE_UPDATE /* <-- update */
	T_(PoolUpdateNew)(0, 0);
#endif /* update --> */
	T_(PoolRemove)(0, 0);
	T_(PoolClear)(0);
	T_(PoolIsEmpty)(0);
	T_(PoolMigrateEach)(0, 0, 0);
	T_(PoolMigratePointer)(0, 0);
#ifdef POOL_TO_STRING
	T_(PoolToString)(0);
#endif
	PT_(unused_coda)();
}
/** {clang}'s pre-processor is not fooled if you have one function. */
static void PT_(unused_coda)(void) { PT_(unused_set)(); }



/* Un-define all macros. */
#undef CAT
#undef CAT_
#undef PCAT
#undef PCAT_
#undef T
#undef T_
#undef PT_
#undef T_NAME
#undef QUOTE
#undef QUOTE_
#ifdef A
#undef A
#endif
#ifdef U
#undef U
#endif
#undef POOL_NAME
#undef POOL_TYPE
#ifdef POOL_MIGRATE_EACH
#undef POOL_MIGRATE_EACH
#endif
#ifdef POOL_MIGRATE_ALL
#undef POOL_MIGRATE_ALL
#endif
#ifdef POOL_MIGRATE_UPDATE
#undef POOL_MIGRATE_UPDATE
#endif
#ifdef POOL_TO_STRING
#undef POOL_TO_STRING
#endif
#ifdef POOL_DEBUG
#undef POOL_DEBUG
#endif
#ifdef POOL_TEST
#undef POOL_TEST
#endif
#ifdef POOL_NDEBUG
#undef POOL_NDEBUG
#undef NDEBUG
#endif
