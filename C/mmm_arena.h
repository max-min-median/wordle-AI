#ifndef MMM_ARENA
#define MMM_ARENA

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mmm_debug.h"

typedef struct {
    void *ptr;
    char name[16];
    size_t capacity;
    size_t current;
    void ***client_ptrs;
    size_t client_ptr_idx;
    size_t client_ptr_array_size;
    uint64_t auto_resize;
} arena;

#define MAX_ARENAS 100
#define LOG2(X) (8 * sizeof(uint32_t) - __builtin_clzl(X) - 1)

arena *arenas[MAX_ARENAS];
size_t num_arenas = 0;
size_t arena_name_idx = 0;

void a_cleanup(void) {
    for (int i = 0; i < MAX_ARENAS; i++) {
        if (arenas[i] != NULL) {
            DEBUG_PRINTF("a_cleanup(): Freeing arena '%s'...\n", arenas[i]->name);
            free(arenas[i]->ptr);
            free(arenas[i]->client_ptrs);
            free(arenas[i]);
            arenas[i] = NULL;
        }
    }
}

arena *new_arena(size_t capacity) {
    static uint8_t registered_cleanup = 0;
    if (!registered_cleanup) {
        if (atexit(a_cleanup) != 0)
            puts("new_arena(): Failed to register a_cleanup()");
        else {
            DEBUG_PRINTF("new_arena(): Registered a_cleanup()\n");
            registered_cleanup = 1;
        }
    }

    if (num_arenas == MAX_ARENAS) {
        puts("new_arena(): Arena limit reached!");
        return NULL;
    }
    // DEBUG_PRINTF("new_arena(): Creating arena #%d (%zu bytes) at %p\n", idx, capacity);
    arena *ar = arenas[num_arenas] = (arena *) malloc(sizeof(arena));
    if (ar == NULL) {
        printf("new_arena(): Failed to allocate %zu bytes for arena metadata!\n", sizeof(arena));
        return NULL;
    }
    ar->ptr = malloc(capacity);
    if (ar->ptr == NULL) {
        free(ar);
        arenas[num_arenas] = NULL;
        printf("new_arena(): Failed to allocate %zu bytes for arena!\n", capacity);
        return NULL;
    }
    ar->capacity = capacity;
    ar->current = 0;
    ar->client_ptr_array_size = 4;
    ar->client_ptrs = malloc(ar->client_ptr_array_size * sizeof(void **));
    ar->client_ptr_idx = 0;
    ar->auto_resize = 1;
    snprintf(ar->name, 16, "ar_%03d", arena_name_idx++);
    DEBUG_PRINTF("new_arena(): Created arena '%s' (%zu bytes) at %p\n", ar->name, capacity, ar->ptr);
    return arenas[num_arenas++];
}

/*
// for testing purposes
void *movealloc(void *mem, size_t size) {
    void *new = malloc(size);
    memcpy(new, mem, size >> 1);
    free(mem);
    return new;
}
*/

arena *arena_copy(arena *ar) {
    arena *copy = new_arena(ar->capacity);
    if (copy == NULL) {
        printf("arena_copy(): Failed to create new arena!\n");
        return NULL;
    }
    copy->current = ar->current;
    copy->auto_resize = ar->auto_resize;
    memcpy(copy->ptr, ar->ptr, ar->current);
    DEBUG_PRINTF("copy_arena(): Copied arena '%s' (%zu bytes) -> '%s'\n", ar->name, ar->capacity, copy->name);
    return copy;
}

void arena_resize(arena *ar, size_t size) {
    void *old_ptr = ar->ptr;
    ar->ptr = realloc(ar->ptr, size);  // change realloc to movealloc to force movement of arena during resize
    if (ar->ptr == NULL) {
        puts("arena_resize(): Unable to resize arena");
        ar->ptr = old_ptr;
    } else {
        // DEBUG_PRINTF("arena_resize(): Arena has expanded from %zu -> %zu\n", ar->capacity, size);
        printf("arena_resize(): Arena has expanded from %zu -> %zu\n", ar->capacity, size);
        ar->capacity = size;
        if (ar->ptr != old_ptr) {
            printf("arena_resize(): Arena has shifted: %p -> %p\n", old_ptr, ar->ptr);
            // DEBUG_PRINTF("arena_resize(): Arena has shifted: %p -> %p\n", old_ptr, ar->ptr);
            for (int i = 0; i < ar->client_ptr_idx; i++) {
                DEBUG_PRINTF("arena_resize(): Moving previous client pointer: %p ", *ar->client_ptrs[i]);
                (*ar->client_ptrs[i]) += ar->ptr - old_ptr;
                DEBUG_PRINTF("-> %p\n", *ar->client_ptrs[i]);
            }
        }
    }
}

void *a_malloc(size_t requested, arena *ar) {
    if (ar->current + requested > ar->capacity) {
        if (ar->auto_resize) {
            size_t new_capacity = 1 << (LOG2(ar->current + requested) + 1);
            arena_resize(ar, new_capacity);
        } else {
            printf("a_malloc(): Arena has insufficient memory: %zu bytes requested, %zu bytes available.\n", requested, ar->capacity - ar->current);
            return NULL;
        }
    }
    DEBUG_PRINTF("a_malloc(): %d bytes allocated at %p\n", requested, ar->ptr + ar->current);
    ar->current += requested;
    return ar->ptr + (ar->current - requested);
}

void *a_calloc(size_t requested, arena *ar) {
    void *result = a_malloc(requested, ar);
    if (result != NULL) memset(result, 0, requested);
    return result;
}

void arena_free(arena *ar) {
    int idx = 0;
    while (idx < num_arenas && arenas[idx] != ar) idx++;
    if (idx == num_arenas) {
        puts("arena_free(): Arena not found or already freed!");
        return;
    }
    DEBUG_PRINTF("arena_free(): Freeing arena '%s'...\n", ar->name);
    free(ar->ptr);
    free(ar->client_ptrs);
    free(ar);
    arenas[idx] = arenas[--num_arenas];
    arenas[num_arenas] = NULL;
}

void arena_reset(arena *ar) {
    int idx = 0;
    while (idx < MAX_ARENAS && arenas[idx] != ar) idx++;
    if (idx == MAX_ARENAS) {
        puts("arena_reset(): Arena not found or already freed!");
        return;
    }
    DEBUG_PRINTF("arena_reset(): Resetting arena #%d...\n", idx);
    ar->current = 0;
}

void **arena_register_ptr(arena *ar, void **ptr) {
    if (ar->client_ptr_idx == ar->client_ptr_array_size) {
        void ***new_client_ptr_array = realloc(ar->client_ptrs, sizeof(void **) * ar->client_ptr_array_size * 2);
        if (new_client_ptr_array == NULL) {
            puts("arena_register_ptr(): Unable to realloc 'client_ptrs' array");
            return NULL;
        }
        DEBUG_PRINTF("arena_register_ptr(): Expanding 'client_ptrs' array [%d -> %d]\n", ar->client_ptr_array_size, ar->client_ptr_array_size * 2);
        ar->client_ptrs = new_client_ptr_array;
        ar->client_ptr_array_size *= 2;
    }
    DEBUG_PRINTF("arena_register_ptr(): Registering pointer %d at address %p (pointing to %p)\n", ar->client_ptr_idx, ptr, *ptr);
    ar->client_ptrs[ar->client_ptr_idx++] = ptr;
    return ptr;
}

#endif  // MMM_ARENA