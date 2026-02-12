/*
 * FastBloomFilter v2 - Scalable Bloom Filter implementation for Ruby
 * Copyright (c) 2026
 *
 * Based on: "Scalable Bloom Filters" (Almeida et al., 2007)
 *
 * Instead of requiring upfront capacity, the filter grows automatically
 * by adding new layers when the current one fills up. Each layer has a
 * tighter error rate so the total FPR stays within the user's target.
 *
 * Growth factor starts at 2x and gradually decreases (like Go slices).
 *
 * Compatible with Ruby >= 2.7
 */

#include <ruby.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Single Bloom Filter layer                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *bits;
    size_t   size;        /* bytes */
    size_t   capacity;    /* max elements for this layer */
    size_t   count;       /* elements inserted so far */
    int      num_hashes;
} BloomLayer;

/* ------------------------------------------------------------------ */
/*  Scalable Bloom Filter (chain of layers)                           */
/* ------------------------------------------------------------------ */

typedef struct {
    BloomLayer **layers;
    size_t  num_layers;
    size_t  layers_cap;      /* allocated slots in layers[] */

    double  error_rate;      /* user-requested total FPR */
    double  tightening;      /* r — each layer multiplies FPR by this */
    size_t  initial_capacity;

    size_t  total_count;     /* elements across all layers */
} ScalableBloom;

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

#define DEFAULT_ERROR_RATE      0.01
#define DEFAULT_INITIAL_CAP     8192
#define DEFAULT_TIGHTENING      0.85
#define FILL_RATIO_THRESHOLD    0.5
#define MAX_HASHES              20
#define MIN_HASHES              1

/* Growth factor: starts at ~2x, approaches 1.25x for large filters.
 * Formula mirrors Go's slice growth strategy.                        */
static double growth_factor(size_t num_layers) {
    if (num_layers < 4)  return 2.0;
    if (num_layers < 8)  return 1.75;
    if (num_layers < 12) return 1.5;
    return 1.25;
}

/* ------------------------------------------------------------------ */
/*  MurmurHash3 — 32-bit (unchanged from v1)                         */
/* ------------------------------------------------------------------ */

static uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const int nblocks = len / 4;
    const uint32_t *blocks = (const uint32_t *)(key);

    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h ^= k1;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    const uint8_t *tail = (const uint8_t *)(key + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16; /* fall through */
        case 2: k1 ^= tail[1] << 8;  /* fall through */
        case 1: k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h ^= k1;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/* ------------------------------------------------------------------ */
/*  Bit helpers                                                       */
/* ------------------------------------------------------------------ */

static inline void set_bit(uint8_t *bits, size_t pos) {
    bits[pos / 8] |= (1 << (pos % 8));
}

static inline int get_bit(const uint8_t *bits, size_t pos) {
    return (bits[pos / 8] & (1 << (pos % 8))) != 0;
}

/* ------------------------------------------------------------------ */
/*  Layer lifecycle                                                   */
/* ------------------------------------------------------------------ */

static BloomLayer *layer_create(size_t capacity, double error_rate) {
    BloomLayer *layer = (BloomLayer *)calloc(1, sizeof(BloomLayer));
    if (!layer) return NULL;

    double ln2    = 0.693147180559945309417;
    double ln2_sq = ln2 * ln2;

    size_t bits_count = (size_t)(-(double)capacity * log(error_rate) / ln2_sq);
    if (bits_count < 64) bits_count = 64;  /* sane minimum */

    layer->size      = (bits_count + 7) / 8;
    layer->capacity  = capacity;
    layer->count     = 0;
    layer->num_hashes = (int)((bits_count / (double)capacity) * ln2);

    if (layer->num_hashes < MIN_HASHES) layer->num_hashes = MIN_HASHES;
    if (layer->num_hashes > MAX_HASHES) layer->num_hashes = MAX_HASHES;

    layer->bits = (uint8_t *)calloc(layer->size, sizeof(uint8_t));
    if (!layer->bits) {
        free(layer);
        return NULL;
    }

    return layer;
}

static void layer_free(BloomLayer *layer) {
    if (layer) {
        free(layer->bits);
        free(layer);
    }
}

static inline int layer_is_full(const BloomLayer *layer) {
    return layer->count >= layer->capacity;
}

static void layer_add(BloomLayer *layer, const char *data, size_t len) {
    size_t bits_count = layer->size * 8;

    /* Kirsch–Mitzenmacher: 2 hashes instead of k */
    uint32_t h1 = murmur3_32((const uint8_t *)data, len, 0x9747b28c);
    uint32_t h2 = murmur3_32((const uint8_t *)data, len, 0x5bd1e995);

    for (int i = 0; i < layer->num_hashes; i++) {
        uint32_t combined = h1 + (uint32_t)i * h2;
        set_bit(layer->bits, combined % bits_count);
    }
    layer->count++;
}

static int layer_include(const BloomLayer *layer, const char *data, size_t len) {
    size_t bits_count = layer->size * 8;

    /* Kirsch–Mitzenmacher: 2 hashes instead of k */
    uint32_t h1 = murmur3_32((const uint8_t *)data, len, 0x9747b28c);
    uint32_t h2 = murmur3_32((const uint8_t *)data, len, 0x5bd1e995);

    for (int i = 0; i < layer->num_hashes; i++) {
        uint32_t combined = h1 + (uint32_t)i * h2;
        if (!get_bit(layer->bits, combined % bits_count))
            return 0;
    }
    return 1;
}

static size_t layer_bits_set(const BloomLayer *layer) {
    size_t count = 0;
    for (size_t i = 0; i < layer->size; i++) {
        uint8_t b = layer->bits[i];
        while (b) { count += b & 1; b >>= 1; }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/*  Scalable filter helpers                                           */
/* ------------------------------------------------------------------ */

/* Error rate for the i-th layer (0-indexed):
 *   layer_fpr(i) = error_rate * (1 - r) * r^i
 * Sum converges to error_rate.                                       */
static double layer_error_rate(double total_fpr, double r, size_t index) {
    return total_fpr * (1.0 - r) * pow(r, (double)index);
}

static BloomLayer *scalable_add_layer(ScalableBloom *sb) {
    size_t new_cap;
    if (sb->num_layers == 0) {
        new_cap = sb->initial_capacity;
    } else {
        double gf = growth_factor(sb->num_layers);
        new_cap = (size_t)(sb->layers[sb->num_layers - 1]->capacity * gf);
    }

    double fpr = layer_error_rate(sb->error_rate, sb->tightening, sb->num_layers);
    if (fpr < 1e-15) fpr = 1e-15;  /* floor to avoid log(0) */

    BloomLayer *layer = layer_create(new_cap, fpr);
    if (!layer) return NULL;

    /* Grow layers array if needed */
    if (sb->num_layers >= sb->layers_cap) {
        size_t new_slots = sb->layers_cap == 0 ? 4 : sb->layers_cap * 2;
        BloomLayer **tmp = (BloomLayer **)realloc(sb->layers,
                                                   new_slots * sizeof(BloomLayer *));
        if (!tmp) { layer_free(layer); return NULL; }
        sb->layers     = tmp;
        sb->layers_cap = new_slots;
    }

    sb->layers[sb->num_layers++] = layer;
    return layer;
}

/* ------------------------------------------------------------------ */
/*  Ruby GC integration                                               */
/* ------------------------------------------------------------------ */

static void bloom_free_scalable(void *ptr) {
    ScalableBloom *sb = (ScalableBloom *)ptr;
    for (size_t i = 0; i < sb->num_layers; i++) {
        layer_free(sb->layers[i]);
    }
    free(sb->layers);
    free(sb);
}

static size_t bloom_memsize_scalable(const void *ptr) {
    const ScalableBloom *sb = (const ScalableBloom *)ptr;
    size_t total = sizeof(ScalableBloom);
    total += sb->layers_cap * sizeof(BloomLayer *);
    for (size_t i = 0; i < sb->num_layers; i++) {
        total += sizeof(BloomLayer) + sb->layers[i]->size;
    }
    return total;
}

static const rb_data_type_t scalable_bloom_type = {
    "ScalableBloomFilter",
    {NULL, bloom_free_scalable, bloom_memsize_scalable},
    NULL, NULL,
    RUBY_TYPED_FREE_IMMEDIATELY
};

/* ------------------------------------------------------------------ */
/*  Ruby methods                                                      */
/* ------------------------------------------------------------------ */

static VALUE bloom_alloc(VALUE klass) {
    ScalableBloom *sb = (ScalableBloom *)calloc(1, sizeof(ScalableBloom));
    if (!sb) rb_raise(rb_eNoMemError, "failed to allocate ScalableBloom");

    return TypedData_Wrap_Struct(klass, &scalable_bloom_type, sb);
}

/*
 * call-seq:
 *   Filter.new                                  # defaults: error_rate 0.01, initial_capacity 1024
 *   Filter.new(error_rate: 0.001)
 *   Filter.new(error_rate: 0.01, initial_capacity: 10_000)
 *
 * No upfront capacity needed — the filter grows automatically.
 *
 * Ruby 2.7+ compatible: keyword arguments are parsed manually from
 * a trailing Hash argument. The rb_scan_args ":" format requires
 * Ruby 3.2+, so we handle it ourselves for broad compatibility.
 */
static VALUE bloom_initialize(int argc, VALUE *argv, VALUE self) {
    VALUE opts = Qnil;

    if (argc == 0) {
        /* Filter.new — all defaults */
    } else if (argc == 1 && RB_TYPE_P(argv[0], T_HASH)) {
        /* Filter.new(error_rate: 0.01, ...) — keyword args as hash */
        opts = argv[0];
    } else {
        rb_raise(rb_eArgError,
                 "wrong number of arguments (given %d, expected 0 or keyword arguments)",
                 argc);
    }

    double error_rate       = DEFAULT_ERROR_RATE;
    size_t initial_capacity = DEFAULT_INITIAL_CAP;
    double tightening       = DEFAULT_TIGHTENING;

    if (!NIL_P(opts)) {
        VALUE v;

        v = rb_hash_aref(opts, ID2SYM(rb_intern("error_rate")));
        if (!NIL_P(v)) error_rate = NUM2DBL(v);

        v = rb_hash_aref(opts, ID2SYM(rb_intern("initial_capacity")));
        if (!NIL_P(v)) initial_capacity = (size_t)NUM2LONG(v);

        v = rb_hash_aref(opts, ID2SYM(rb_intern("tightening")));
        if (!NIL_P(v)) tightening = NUM2DBL(v);
    }

    if (error_rate <= 0 || error_rate >= 1)
        rb_raise(rb_eArgError, "error_rate must be between 0 and 1 (exclusive)");
    if (initial_capacity == 0)
        rb_raise(rb_eArgError, "initial_capacity must be positive");
    if (tightening <= 0 || tightening >= 1)
        rb_raise(rb_eArgError, "tightening must be between 0 and 1 (exclusive)");

    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);

    sb->error_rate       = error_rate;
    sb->initial_capacity = initial_capacity;
    sb->tightening       = tightening;
    sb->total_count      = 0;

    /* Create first layer */
    if (!scalable_add_layer(sb))
        rb_raise(rb_eNoMemError, "failed to allocate initial layer");

    return self;
}

/*
 * call-seq:
 *   filter.add("element")
 *   filter << "element"
 */
static VALUE bloom_add(VALUE self, VALUE str) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);

    Check_Type(str, T_STRING);

    BloomLayer *active = sb->layers[sb->num_layers - 1];

    /* Grow if current layer is full */
    if (layer_is_full(active)) {
        active = scalable_add_layer(sb);
        if (!active)
            rb_raise(rb_eNoMemError, "failed to allocate new layer");
    }

    layer_add(active, RSTRING_PTR(str), RSTRING_LEN(str));
    sb->total_count++;

    return Qtrue;
}

/*
 * call-seq:
 *   filter.include?("element")   #=> true / false
 *   filter.member?("element")    #=> true / false
 *
 * Checks all layers. Returns true if ANY layer says "possibly yes".
 */
static VALUE bloom_include(VALUE self, VALUE str) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);

    Check_Type(str, T_STRING);

    const char *data = RSTRING_PTR(str);
    size_t len       = RSTRING_LEN(str);

    /* Check from newest to oldest — most elements are in recent layers */
    for (size_t i = sb->num_layers; i > 0; i--) {
        if (layer_include(sb->layers[i - 1], data, len))
            return Qtrue;
    }

    return Qfalse;
}

/*
 * Reset all layers, keep only one fresh layer.
 */
static VALUE bloom_clear(VALUE self) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);

    for (size_t i = 0; i < sb->num_layers; i++) {
        layer_free(sb->layers[i]);
    }
    sb->num_layers  = 0;
    sb->total_count = 0;

    if (!scalable_add_layer(sb))
        rb_raise(rb_eNoMemError, "failed to allocate layer after clear");

    return Qnil;
}

/*
 * Detailed statistics for the whole filter and each layer.
 */
static VALUE bloom_stats(VALUE self) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);

    size_t total_bytes    = 0;
    size_t total_bits     = 0;
    size_t total_bits_set = 0;

    VALUE layers_ary = rb_ary_new_capa((long)sb->num_layers);

    for (size_t i = 0; i < sb->num_layers; i++) {
        BloomLayer *l = sb->layers[i];
        size_t bs = layer_bits_set(l);
        size_t tb = l->size * 8;

        total_bytes    += l->size;
        total_bits     += tb;
        total_bits_set += bs;

        VALUE lh = rb_hash_new();
        rb_hash_aset(lh, ID2SYM(rb_intern("layer")),      LONG2NUM(i));
        rb_hash_aset(lh, ID2SYM(rb_intern("capacity")),    LONG2NUM(l->capacity));
        rb_hash_aset(lh, ID2SYM(rb_intern("count")),       LONG2NUM(l->count));
        rb_hash_aset(lh, ID2SYM(rb_intern("size_bytes")),  LONG2NUM(l->size));
        rb_hash_aset(lh, ID2SYM(rb_intern("num_hashes")),  INT2NUM(l->num_hashes));
        rb_hash_aset(lh, ID2SYM(rb_intern("bits_set")),    LONG2NUM(bs));
        rb_hash_aset(lh, ID2SYM(rb_intern("total_bits")),  LONG2NUM(tb));
        rb_hash_aset(lh, ID2SYM(rb_intern("fill_ratio")),  DBL2NUM((double)bs / tb));
        rb_hash_aset(lh, ID2SYM(rb_intern("error_rate")),
                     DBL2NUM(layer_error_rate(sb->error_rate, sb->tightening, i)));

        rb_ary_push(layers_ary, lh);
    }

    VALUE hash = rb_hash_new();
    rb_hash_aset(hash, ID2SYM(rb_intern("total_count")),    LONG2NUM(sb->total_count));
    rb_hash_aset(hash, ID2SYM(rb_intern("num_layers")),     LONG2NUM(sb->num_layers));
    rb_hash_aset(hash, ID2SYM(rb_intern("total_bytes")),    LONG2NUM(total_bytes));
    rb_hash_aset(hash, ID2SYM(rb_intern("total_bits")),     LONG2NUM(total_bits));
    rb_hash_aset(hash, ID2SYM(rb_intern("total_bits_set")), LONG2NUM(total_bits_set));
    rb_hash_aset(hash, ID2SYM(rb_intern("fill_ratio")),     DBL2NUM((double)total_bits_set / total_bits));
    rb_hash_aset(hash, ID2SYM(rb_intern("error_rate")),     DBL2NUM(sb->error_rate));
    rb_hash_aset(hash, ID2SYM(rb_intern("layers")),         layers_ary);

    return hash;
}

/*
 * Number of elements inserted.
 */
static VALUE bloom_count(VALUE self) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);
    return LONG2NUM(sb->total_count);
}

/*
 * Number of layers currently allocated.
 */
static VALUE bloom_num_layers(VALUE self) {
    ScalableBloom *sb;
    TypedData_Get_Struct(self, ScalableBloom, &scalable_bloom_type, sb);
    return LONG2NUM(sb->num_layers);
}

/*
 * Merge another scalable filter into this one.
 * Appends all layers from `other` (copies the bit arrays).
 */
static VALUE bloom_merge(VALUE self, VALUE other) {
    ScalableBloom *sb1, *sb2;
    TypedData_Get_Struct(self,  ScalableBloom, &scalable_bloom_type, sb1);
    TypedData_Get_Struct(other, ScalableBloom, &scalable_bloom_type, sb2);

    for (size_t i = 0; i < sb2->num_layers; i++) {
        BloomLayer *src = sb2->layers[i];

        /* Create a copy of the layer */
        BloomLayer *copy = (BloomLayer *)calloc(1, sizeof(BloomLayer));
        if (!copy) rb_raise(rb_eNoMemError, "failed to allocate layer copy");

        copy->size       = src->size;
        copy->capacity   = src->capacity;
        copy->count      = src->count;
        copy->num_hashes = src->num_hashes;
        copy->bits       = (uint8_t *)malloc(src->size);
        if (!copy->bits) { free(copy); rb_raise(rb_eNoMemError, "failed to allocate bits"); }
        memcpy(copy->bits, src->bits, src->size);

        /* Append to layers array */
        if (sb1->num_layers >= sb1->layers_cap) {
            size_t new_slots = sb1->layers_cap == 0 ? 4 : sb1->layers_cap * 2;
            BloomLayer **tmp = (BloomLayer **)realloc(sb1->layers,
                                                       new_slots * sizeof(BloomLayer *));
            if (!tmp) { layer_free(copy); rb_raise(rb_eNoMemError, "realloc failed"); }
            sb1->layers     = tmp;
            sb1->layers_cap = new_slots;
        }
        sb1->layers[sb1->num_layers++] = copy;
    }

    sb1->total_count += sb2->total_count;
    return self;
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */

void Init_fast_bloom_filter(void) {
    VALUE mFastBloomFilter = rb_define_module("FastBloomFilter");
    VALUE cFilter = rb_define_class_under(mFastBloomFilter, "Filter", rb_cObject);

    rb_define_alloc_func(cFilter, bloom_alloc);
    rb_define_method(cFilter, "initialize",  bloom_initialize, -1);
    rb_define_method(cFilter, "add",         bloom_add,        1);
    rb_define_method(cFilter, "<<",          bloom_add,        1);
    rb_define_method(cFilter, "include?",    bloom_include,    1);
    rb_define_method(cFilter, "member?",     bloom_include,    1);
    rb_define_method(cFilter, "clear",       bloom_clear,      0);
    rb_define_method(cFilter, "stats",       bloom_stats,      0);
    rb_define_method(cFilter, "count",       bloom_count,      0);
    rb_define_method(cFilter, "size",        bloom_count,      0);
    rb_define_method(cFilter, "num_layers",  bloom_num_layers, 0);
    rb_define_method(cFilter, "merge!",      bloom_merge,      1);
}
