#include "minias.h"

static void vwarn(const char *fmt, va_list ap) {
  vfprintf(stderr, fmt, ap);
  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    putc(' ', stderr);
    perror(NULL);
  } else {
    putc('\n', stderr);
  }
}

void lfatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "%ld: ", curlineno);
  va_start(ap, fmt);
  vwarn(fmt, ap);
  va_end(ap);
  exit(1);
}

void fatal(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vwarn(fmt, ap);
  va_end(ap);
  exit(1);
}

void unreachable(void) { lfatal("BUG: unexpected internal condition"); }

void *xmalloc(size_t n) {
  void *p;

  p = malloc(n);
  if (!p)
    fatal("malloc:");

  return p;
}

void *zalloc(size_t n) {
  void *p;

  p = malloc(n);
  if (!p)
    fatal("malloc:");
  memset(p, 0, n);
  return p;
}

void *xrealloc(void *p, size_t n) {
  p = realloc(p, n);
  if (!p)
    fatal("realloc:");

  return p;
}

void *xreallocarray(void *p, size_t n, size_t m) {
  p = reallocarray(p, n, m);
  if (!p)
    fatal("reallocarray:");

  return p;
}

char *xmemdup(const char *s, size_t n) {
  char *p;

  p = xmalloc(n);
  memcpy(p, s, n);

  return p;
}

char *xstrdup(const char *s) { return xmemdup(s, strlen(s) + 1); }

const char *internstring(const char *s) {
  size_t idx, len;
  const char *interned;
  static const char *cache[4096] = {0};

  len = strlen(s);
  idx = murmurhash64a(s, len) % sizeof(cache)/sizeof(cache[0]);
  interned = cache[idx];
  if (interned && strcmp(s, cache[idx]) == 0) {
    return interned;
  }
  interned = xstrdup(s);
  cache[idx] = interned;
  return interned;
}

void htabkey(struct hashtablekey *k, const char *s, size_t n) {
  k->str = s;
  k->len = n;
  k->hash = murmurhash64a(s, n);
}

struct hashtable *mkhtab(size_t cap) {
  struct hashtable *h;
  size_t i;

  assert(!(cap & (cap - 1)));
  h = xmalloc(sizeof(*h));
  h->len = 0;
  h->cap = cap;
  h->keys = xreallocarray(NULL, cap, sizeof(h->keys[0]));
  h->vals = xreallocarray(NULL, cap, sizeof(h->vals[0]));
  for (i = 0; i < cap; ++i)
    h->keys[i].str = NULL;

  return h;
}

void delhtab(struct hashtable *h, void del(void *)) {
  size_t i;

  if (!h)
    return;
  if (del) {
    for (i = 0; i < h->cap; ++i) {
      if (h->keys[i].str)
        del(h->vals[i]);
    }
  }
  free(h->keys);
  free(h->vals);
  free(h);
}

static bool keyequal(struct hashtablekey *k1, struct hashtablekey *k2) {
  if (k1->hash != k2->hash || k1->len != k2->len)
    return false;
  return memcmp(k1->str, k2->str, k1->len) == 0;
}

static size_t keyindex(struct hashtable *h, struct hashtablekey *k) {
  size_t i;

  i = k->hash & (h->cap - 1);
  while (h->keys[i].str && !keyequal(&h->keys[i], k))
    i = (i + 1) & (h->cap - 1);
  return i;
}

void **htabput(struct hashtable *h, struct hashtablekey *k) {
  struct hashtablekey *oldkeys;
  void **oldvals;
  size_t i, j, oldcap;

  if (h->cap / 2 < h->len) {
    oldkeys = h->keys;
    oldvals = h->vals;
    oldcap = h->cap;
    h->cap *= 2;
    h->keys = xreallocarray(NULL, h->cap, sizeof(h->keys[0]));
    h->vals = xreallocarray(NULL, h->cap, sizeof(h->vals[0]));
    for (i = 0; i < h->cap; ++i)
      h->keys[i].str = NULL;
    for (i = 0; i < oldcap; ++i) {
      if (oldkeys[i].str) {
        j = keyindex(h, &oldkeys[i]);
        h->keys[j] = oldkeys[i];
        h->vals[j] = oldvals[i];
      }
    }
    free(oldkeys);
    free(oldvals);
  }
  i = keyindex(h, k);
  if (!h->keys[i].str) {
    h->keys[i] = *k;
    h->vals[i] = NULL;
    ++h->len;
  }

  return &h->vals[i];
}

void *htabget(struct hashtable *h, struct hashtablekey *k) {
  size_t i;

  i = keyindex(h, k);
  return h->keys[i].str ? h->vals[i] : NULL;
}

uint64_t murmurhash64a(const void *ptr, size_t len) {
  const uint64_t seed = 0xdecafbaddecafbadull;
  const uint64_t m = 0xc6a4a7935bd1e995ull;
  uint64_t h, k, n;
  const uint8_t *p, *end;
  int r = 47;

  h = seed ^ (len * m);
  n = len & ~0x7ull;
  end = ptr;
  end += n;
  for (p = ptr; p != end; p += 8) {
    memcpy(&k, p, sizeof(k));

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  switch (len & 0x7) {
  case 7:
    h ^= (uint64_t)p[6] << 48; /* fallthrough */
  case 6:
    h ^= (uint64_t)p[5] << 40; /* fallthrough */
  case 5:
    h ^= (uint64_t)p[4] << 32; /* fallthrough */
  case 4:
    h ^= (uint64_t)p[3] << 24; /* fallthrough */
  case 3:
    h ^= (uint64_t)p[2] << 16; /* fallthrough */
  case 2:
    h ^= (uint64_t)p[1] << 8; /* fallthrough */
  case 1:
    h ^= (uint64_t)p[0];
    h *= m;
  }

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}
