#ifndef MMM_HASHSET
#define MMM_HASHSET

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mmm_debug.h"
#include "mmm_arena.h"

typedef struct {
    arena *elems;  // elems live in this arena. Arena may be resized as necessary.
/*  array of offsets of the element's location in the `elems` arena. To be specific, the element can be found at `set->elems->ptr + set->offsets[hash]`.
    The value of 'hash' may be 0.
    However, 0 is used as a sentinel value for the offset itself, indicating an unused slot (SET_ELEM_NOT_FOUND)
    (uint32_t) -1 is also a sentinel value, indicating a deleted slot (SET_ELEM_DELETED)
*/
    uint32_t *offsets;  
                     
/*  elem_size < 0: elems are dynamically sized, and the first `abs(elem_size)` bytes indicate the size of the element.
                   For example, if elem_size = -2, the first 2 bytes give the size of the element.
    elem_size == 0: elems are null-terminated strings.
    elem_size > 0: elems occupy a fixed number of bytes, given by `elem_size`
*/
    int32_t elem_size; 
    uint32_t size;  // no. of elements
    uint32_t load;  // no. of slots used in the hash table, including slots marked with DELETED.
    uint32_t capacity;  // always a power of 2
    uint32_t *occupied;  // dense array of encountered hashes. To be checked when iterating over the set.
    uint32_t occ_idx;  // the next available index of `occupied`. Equivalent to the 'used size' of the `occupied` array.
    uint32_t occ_packed;  // a boolean indicating if the `occupied` array has been packed by the iterator.
    uint32_t (*hash_func)(void *);  // the hash function
} hashset;

typedef struct {
    hashset *set;
    uint32_t dense_idx;  // the "left" pointer. The next explored elem is relocated to this position.
    uint32_t explorer_idx;  // the "right" pointer. If left pointer reaches a free slot, right pointer explores until it finds an elem.
    uint32_t num_iterations;
} hashset_iterator;

#define LOG2CEIL(X) (8 * sizeof(uint32_t) - __builtin_clzl(X) - 1 + (__builtin_popcount(X) != 1))
#define SET_ELEM_DELETED ((uint32_t) -1)
#define SET_ELEM_NOT_FOUND 0
#define SET_GET_ELEM_ACTUAL_SIZE \
    uint32_t actual_size = 0; \
    do { for (int32_t i = 0; i < -elem_size; i++) actual_size += (*(uint8_t *)(elem++)) << (8 * i); } while (0)

#define FOR_ITEM_IN_SET(I, S, code) do { \
    hashset_iterator iter = get_hashset_iterator(S); \
    for (void *I = set_iter_next(&iter); I; I = set_iter_next(&iter)) {code} \
} while (0)

void set_resize(hashset *set);

hashset *new_set(uint32_t capacity, int32_t elem_size, uint32_t (*hash_func)(void *)) {

    uint32_t old_capacity = capacity;
    capacity = 1 << LOG2CEIL(capacity);
    DEBUG_PRINTF("new_set(): Creating hashset of capacity %d\n", capacity);
    
    hashset *set = malloc(sizeof(hashset));
    if (set == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for hashset struct!\n", sizeof(hashset));
        return NULL;
    }

    set->offsets = calloc(capacity, sizeof(*set->offsets));
    if (set->offsets == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for offsets array!\n", capacity * sizeof(*set->offsets));
        free(set);
        return NULL;
    }

    set->occupied = calloc(capacity, sizeof(*set->occupied));
    if (set->occupied == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for occupied array!\n", capacity * sizeof(*set->occupied));
        free(set->offsets);
        free(set);
        return NULL;
    }
    set->occ_idx = 0;
    set->occ_packed = 1;

    uint32_t arena_size = capacity * (elem_size <= 0 ? 256 : elem_size);  // default variable arena size is 256 bytes per key
    set->elems = new_arena(arena_size);
    if (set->elems == NULL) {
        printf("new_set(): Failed to initialize elems arena of %zu bytes.\n", arena_size);
        free(set->occupied);
        free(set->offsets);
        free(set);
        return NULL;
    }

    a_malloc(1, set->elems);  // if set->offsets[hash] == 0, that indicates no elem at that hash value.
    set->size = set->load = 0;
    set->capacity = capacity;
    set->elem_size = elem_size;
    set->hash_func = hash_func;
    return set;
}

uint8_t set_elems_are_equal(void *item, void *elem, int32_t item_size, int32_t elem_size) {

    if (elem_size == 0) return strcmp(item, elem) == 0;
    else if (elem_size > 0) return item_size == elem_size && memcmp(item, elem, elem_size) == 0;
    SET_GET_ELEM_ACTUAL_SIZE;
    return item_size == actual_size && memcmp(item, elem, actual_size) == 0;
}

/**
 * Adds `item` to `set`, checking first if it already exists.
 * Returns the offset of the item in the arena if successfully added, else 0.
 */
uint32_t set_add(void *item, hashset *set, uint32_t item_size) {

    if (set->elem_size >= 0 && set->elem_size != item_size) {
        puts("set_add(): Item is not the correct size!");
        return 0;
    }

    if (set->load * 3 > set->capacity * 2) set_resize(set);

    uint32_t hash = (set->hash_func(item) * 11400714819323198485llu) & (set->capacity - 1);  // Fibonacci hash
    while (set->offsets[hash] != SET_ELEM_NOT_FOUND && set->offsets[hash] != SET_ELEM_DELETED) {
        if (set_elems_are_equal(item, set->elems->ptr + set->offsets[hash], item_size, set->elem_size)) {
            DEBUG_PRINTF("set_add(): Item already exists in set!");
            return 0;
        }
        hash = ((hash * 5) + 1) & (set->capacity - 1);
    }

    if (set->offsets[hash] != SET_ELEM_DELETED) {  // Adding element to fresh slot; load increases.
        set->load++;
        if (set->occ_packed) {  // if the `occupied` array is packed, we place the new hash at the right spot.
            set->occupied[set->occ_idx++] = set->occupied[set->size];
            set->occupied[set->size] = hash;  // We only add the hash to the dense array if it has not been added before.
        } else {                              // Hashes are never deleted from the dense array, only moved to the end.
            set->occupied[set->occ_idx++] = hash;  // if the `occupied` array is not packed, add the new hash to the end.
        }
    }

    set->size++;
    if (set->elem_size < 0) {  // variable element size
        set->offsets[hash] = a_malloc(item_size - set->elem_size, set->elems) - set->elems->ptr;
        if (set->offsets[hash] == 0) {
            puts("set_add(): Failed to allocate memory in arena!");
            return 0;
        }
        uint32_t item_size_copy = item_size;
        for (int32_t i = 0; i < -set->elem_size; i++) {
            *(uint8_t *)(set->elems->ptr + set->offsets[hash] + i) = item_size_copy & 0xFF;
            item_size_copy >>= 8;
        }
        memcpy(set->elems->ptr + set->offsets[hash] - set->elem_size, item, item_size);
    } else {
        if (item_size == 0) item_size = strlen(item) + 1;  // null-terminated strings
        set->offsets[hash] = a_malloc(item_size, set->elems) - set->elems->ptr;
        // TODO: check for NULL
        memcpy(set->elems->ptr + set->offsets[hash], item, item_size);
    }
    DEBUG_PRINTF("set_add(): adding item to hash index %d\n", hash);
    return set->offsets[hash];
}

/**
 * Removes `item` from `set` if it exists.
 * Returns the offset of the item in the arena if successfully deleted, else 0.
 */
uint32_t set_delete(void *item, hashset *set, uint32_t item_size) {

    uint32_t hash = (set->hash_func(item) * 11400714819323198485llu) & (set->capacity - 1);  // Fibonacci hash
    while (set->offsets[hash] != SET_ELEM_NOT_FOUND) {
        if (set->offsets[hash] != SET_ELEM_DELETED && set_elems_are_equal(item, set->elems->ptr + set->offsets[hash], item_size, set->elem_size)) {
            DEBUG_PRINTF("set_delete(): Deleting item at hash %d\n", hash);
            uint32_t result = set->offsets[hash];
            set->offsets[hash] = SET_ELEM_DELETED;
            set->occ_packed = 0;
            set->size--;
            return result;
        }
        hash = ((hash * 5) + 1) & (set->capacity - 1);
    }
    DEBUG_PRINTF("set_delete(): Item not found!");
    return 0;
}

/**
 * Searches for `item` in `set`.
 * Returns the offset of the item in the arena if it exists, else 0.
 */
uint32_t set_has(void *item, hashset *set, uint32_t item_size) {

    uint32_t hash = (set->hash_func(item) * 11400714819323198485llu) & (set->capacity - 1);  // Fibonacci hash
    while (set->offsets[hash] != SET_ELEM_NOT_FOUND) {
        if (set->offsets[hash] != SET_ELEM_DELETED && set_elems_are_equal(item, set->elems->ptr + set->offsets[hash], item_size, set->elem_size)) {
            return set->offsets[hash];
        }
        hash = ((hash * 5) + 1) & (set->capacity - 1);
    }
    return 0;
}

hashset *set_copy(hashset *set) {
    hashset *copy = new_set(set->capacity, set->elem_size, set->hash_func);
    arena_free(copy->elems);
    copy->elems = arena_copy(set->elems);
    memcpy(copy->offsets, set->offsets, copy->capacity * sizeof(*copy->offsets));
    memcpy(copy->occupied, set->occupied, copy->capacity * sizeof(*copy->occupied));
    copy->occ_idx = set->occ_idx;
    copy->occ_packed = set->occ_packed;
    copy->size = set->size;
    copy->load = set->load;
    DEBUG_PRINTF("set_copy(): copied set at %p\n", set);
    return copy;
}

hashset_iterator get_hashset_iterator(hashset *set) {
    hashset_iterator it;
    it.dense_idx = it.explorer_idx = 0;
    it.set = set;
    it.num_iterations = set->size;
    return it;
}

// returns address of next elem, and also packs the `occupied` array. returns NULL if iteration has ended.
void *set_iter_next(hashset_iterator *it) {
    hashset *set = it->set;
    if (it->dense_idx >= it->num_iterations) { DEBUG_PRINTF("set_iter_next(): end of iteration\n"); set->occ_packed = (it->dense_idx == set->size); return NULL; }
    if (it->explorer_idx >= set->occ_idx) { DEBUG_PRINTF("set_iter_next(): iteration incomplete (1); unexpected end of array reached!\n"); return NULL; }
    if (set->offsets[set->occupied[it->dense_idx]] != SET_ELEM_DELETED) return set->elems->ptr + set->offsets[set->occupied[it->dense_idx++]];
    
    // Now we explore from it->explorer_idx onwards to locate next element.
    if (it->explorer_idx <= it->dense_idx) it->explorer_idx = it->dense_idx + 1;
    while (1) {
        if (it->explorer_idx >= set->occ_idx) { DEBUG_PRINTF("set_iter_next(): iteration incomplete (2); unexpected end of array reached!\n"); return NULL; }
        if (set->offsets[set->occupied[it->explorer_idx]] != SET_ELEM_DELETED) {
            DEBUG_PRINTF("set_iter_next(): swapping set->occupied [%d] and [%d]\n", it->explorer_idx, it->dense_idx);
            uint32_t temp = set->occupied[it->dense_idx];
            set->occupied[it->dense_idx] = set->occupied[it->explorer_idx];
            set->occupied[it->explorer_idx] = temp;
            return set->elems->ptr + set->offsets[set->occupied[it->dense_idx++]];
        }
        it->explorer_idx++;
    }
}

void *set_at(uint32_t index, hashset *set) {

    if (index >= set->size) { printf("set_at(): array index [%d] out of bounds (set->size == %d)", index, set->size); return NULL; }
    if (!set->occ_packed) FOR_ITEM_IN_SET(item, set, 1;);
    return set->elems->ptr + set->offsets[set->occupied[index]];
}

void set_free(hashset *set) {
    DEBUG_PRINTF("set_free(): Freeing set at address %p\n", set);
    free(set->offsets);
    free(set->occupied);
    arena_free(set->elems);
    free(set);
}

/**
 * Resizes the set to the power of 2 which is at least double of its current size.
 * Rehashes all keys which are still present (not deleted).
 */
void set_resize(hashset *set) {
    
    uint32_t old_size = set->size;
    uint32_t new_capacity = 1 << (LOG2CEIL(old_size) + 1);
    DEBUG_PRINTF("set_resize(): Resizing hashset: %zu -> %zu\n", set->capacity, new_capacity);
    
    hashset *resized_set = new_set(new_capacity, set->elem_size, set->hash_func);
    hashset_iterator iter = get_hashset_iterator(set);
    
    if (set->elem_size >= 0) {
        for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
            set_add(item, resized_set, resized_set->elem_size);
        }
    } else {
        for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
            set_add(item, resized_set, resized_set->elem_size);
        }
    }

    free(set->offsets);
    free(set->occupied);
    arena_free(set->elems);
    *set = *resized_set;
    free(resized_set);
}

#endif  // MMM_HASHSET