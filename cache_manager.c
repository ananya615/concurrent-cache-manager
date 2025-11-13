#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ===============================
   CONCURRENT CACHE IMPLEMENTATION
   =============================== */

typedef struct node {
    char *key;
    char *value;
    struct node *prev;
    struct node *next;
    struct node *hnext; // next in hash bucket
} node;

typedef struct cache_t {
    size_t capacity;
    size_t size;
    size_t hsize;
    node **htable;
    node *head; // most recently used
    node *tail; // least recently used
    pthread_rwlock_t lock;
} cache_t;

/* djb2 string hash */
static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static node *node_create(const char *key, const char *value) {
    node *n = calloc(1, sizeof(node));
    if (!n) return NULL;
    n->key = strdup(key);
    n->value = strdup(value);
    if (!n->key || !n->value) {
        free(n->key);
        free(n->value);
        free(n);
        return NULL;
    }
    return n;
}

static void node_free(node *n) {
    if (!n) return;
    free(n->key);
    free(n->value);
    free(n);
}

static void dll_remove(cache_t *c, node *n) {
    if (n->prev) n->prev->next = n->next;
    else c->head = n->next;
    if (n->next) n->next->prev = n->prev;
    else c->tail = n->prev;
    n->prev = n->next = NULL;
}

static void dll_push_head(cache_t *c, node *n) {
    n->prev = NULL;
    n->next = c->head;
    if (c->head) c->head->prev = n;
    c->head = n;
    if (!c->tail) c->tail = n;
}

static void htable_remove(cache_t *c, node *n) {
    unsigned long hv = hash_str(n->key) % c->hsize;
    node *cur = c->htable[hv];
    node *prev = NULL;
    while (cur) {
        if (cur == n) {
            if (prev) prev->hnext = cur->hnext;
            else c->htable[hv] = cur->hnext;
            cur->hnext = NULL;
            return;
        }
        prev = cur;
        cur = cur->hnext;
    }
}

static void htable_insert(cache_t *c, node *n) {
    unsigned long hv = hash_str(n->key) % c->hsize;
    n->hnext = c->htable[hv];
    c->htable[hv] = n;
}

cache_t *cache_create(size_t capacity) {
    if (capacity == 0) return NULL;
    cache_t *c = calloc(1, sizeof(cache_t));
    if (!c) return NULL;
    c->capacity = capacity;
    c->size = 0;
    c->hsize = capacity * 2 + 1;
    c->htable = calloc(c->hsize, sizeof(node*));
    if (!c->htable) { free(c); return NULL; }
    pthread_rwlock_init(&c->lock, NULL);
    c->head = c->tail = NULL;
    return c;
}

int cache_put(cache_t *cache, const char *key, const char *value) {
    if (!cache || !key || !value) return -1;
    pthread_rwlock_wrlock(&cache->lock);

    unsigned long hv = hash_str(key) % cache->hsize;
    node *cur = cache->htable[hv];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            char *nv = strdup(value);
            if (!nv) { pthread_rwlock_unlock(&cache->lock); return -1; }
            free(cur->value);
            cur->value = nv;
            dll_remove(cache, cur);
            dll_push_head(cache, cur);
            pthread_rwlock_unlock(&cache->lock);
            return 0;
        }
        cur = cur->hnext;
    }

    node *n = node_create(key, value);
    if (!n) { pthread_rwlock_unlock(&cache->lock); return -1; }

    htable_insert(cache, n);
    dll_push_head(cache, n);
    cache->size++;

    if (cache->size > cache->capacity) {
        node *ev = cache->tail;
        if (ev) {
            dll_remove(cache, ev);
            htable_remove(cache, ev);
            cache->size--;
            node_free(ev);
        }
    }

    pthread_rwlock_unlock(&cache->lock);
    return 0;
}

char *cache_get(cache_t *cache, const char *key) {
    if (!cache || !key) return NULL;
    pthread_rwlock_rdlock(&cache->lock);
    unsigned long hv = hash_str(key) % cache->hsize;
    node *cur = cache->htable[hv];
    while (cur) {
        if (strcmp(cur->key, key) == 0) break;
        cur = cur->hnext;
    }
    if (!cur) { pthread_rwlock_unlock(&cache->lock); return NULL; }

    pthread_rwlock_unlock(&cache->lock);
    pthread_rwlock_wrlock(&cache->lock);

    hv = hash_str(key) % cache->hsize;
    cur = cache->htable[hv];
    while (cur) {
        if (strcmp(cur->key, key) == 0) break;
        cur = cur->hnext;
    }
    if (cur) {
        dll_remove(cache, cur);
        dll_push_head(cache, cur);
        char *ret = strdup(cur->value);
        pthread_rwlock_unlock(&cache->lock);
        return ret;
    }
    pthread_rwlock_unlock(&cache->lock);
    return NULL;
}

int cache_delete(cache_t *cache, const char *key) {
    if (!cache || !key) return -1;
    pthread_rwlock_wrlock(&cache->lock);
    unsigned long hv = hash_str(key) % cache->hsize;
    node *cur = cache->htable[hv];
    node *prev = NULL;
    while (cur) {
        if (strcmp(cur->key, key) == 0) break;
        prev = cur;
        cur = cur->hnext;
    }
    if (!cur) { pthread_rwlock_unlock(&cache->lock); return 1; }
    if (prev) prev->hnext = cur->hnext;
    else cache->htable[hv] = cur->hnext;
    dll_remove(cache, cur);
    node_free(cur);
    cache->size--;
    pthread_rwlock_unlock(&cache->lock);
    return 0;
}

void cache_destroy(cache_t *cache) {
    if (!cache) return;
    pthread_rwlock_wrlock(&cache->lock);
    for (size_t i = 0; i < cache->hsize; ++i) {
        node *cur = cache->htable[i];
        while (cur) {
            node *nx = cur->hnext;
            node_free(cur);
            cur = nx;
        }
    }
    free(cache->htable);
    pthread_rwlock_unlock(&cache->lock);
    pthread_rwlock_destroy(&cache->lock);
    free(cache);
}

/* ===============================
   TEST CODE (Multithreaded Demo)
   =============================== */

#define NUM_READERS 8
#define NUM_WRITERS 4
#define OPS_PER_THREAD 1000

typedef struct {
    cache_t *cache;
    int id;
} thread_arg_t;

void *reader_thread(void *arg) {
    thread_arg_t *t = (thread_arg_t*)arg;
    char key[32];
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        snprintf(key, sizeof(key), "key-%d", rand() % 100);
        char *val = cache_get(t->cache, key);
        if (val) free(val);
    }
    return NULL;
}

void *writer_thread(void *arg) {
    thread_arg_t *t = (thread_arg_t*)arg;
    char key[32], val[32];
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        snprintf(key, sizeof(key), "key-%d", rand() % 100);
        snprintf(val, sizeof(val), "val-%d-%d", t->id, i);
        cache_put(t->cache, key, val);
        if (i % 200 == 0) cache_delete(t->cache, key);
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    cache_t *cache = cache_create(50);
    assert(cache);

    pthread_t readers[NUM_READERS], writers[NUM_WRITERS];
    thread_arg_t rargs[NUM_READERS], wargs[NUM_WRITERS];

    for (int i = 0; i < NUM_WRITERS; i++) {
        wargs[i].cache = cache; wargs[i].id = i;
        pthread_create(&writers[i], NULL, writer_thread, &wargs[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        rargs[i].cache = cache; rargs[i].id = i;
        pthread_create(&readers[i], NULL, reader_thread, &rargs[i]);
    }

    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(writers[i], NULL);
    for (int i = 0; i < NUM_READERS; i++) pthread_join(readers[i], NULL);

    cache_destroy(cache);
    printf("✅ Test completed successfully!\\n");
    return 0;
}
