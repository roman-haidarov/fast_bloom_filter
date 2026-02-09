/*
 * FastBloomFilter - High-performance Bloom Filter implementation for Ruby
 * Copyright (c) 2025
 */

#include <ruby.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Bloom Filter structure */
typedef struct {
    uint8_t *bits;      /* Bit array */
    size_t size;        /* Size in bytes */
    size_t capacity;    /* Expected number of elements */
    int num_hashes;     /* Number of hash functions */
} BloomFilter;

/* GC: Free memory */
static void bloom_free(void *ptr) {
    BloomFilter *bloom = (BloomFilter *)ptr;
    if (bloom->bits) {
        free(bloom->bits);
    }
    free(bloom);
}

/* GC: Report memory size */
static size_t bloom_memsize(const void *ptr) {
    const BloomFilter *bloom = (const BloomFilter *)ptr;
    return sizeof(BloomFilter) + bloom->size;
}

static const rb_data_type_t bloom_type = {
    "BloomFilter",
    {NULL, bloom_free, bloom_memsize},
    NULL, NULL,
    RUBY_TYPED_FREE_IMMEDIATELY
};

/*
 * MurmurHash3 32-bit implementation
 */
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
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
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

/* Set bit at position */
static inline void set_bit(uint8_t *bits, size_t pos) {
    bits[pos / 8] |= (1 << (pos % 8));
}

/* Get bit at position */
static inline int get_bit(const uint8_t *bits, size_t pos) {
    return (bits[pos / 8] & (1 << (pos % 8))) != 0;
}

/* Allocate BloomFilter object */
static VALUE bloom_alloc(VALUE klass) {
    BloomFilter *bloom = ALLOC(BloomFilter);
    bloom->bits = NULL;
    bloom->size = 0;
    bloom->capacity = 0;
    bloom->num_hashes = 0;
    
    return TypedData_Wrap_Struct(klass, &bloom_type, bloom);
}

/*
 * Initialize Bloom Filter
 * 
 * @param capacity [Integer] Expected number of elements
 * @param error_rate [Float] Desired false positive rate (default: 0.01)
 */
static VALUE bloom_initialize(int argc, VALUE *argv, VALUE self) {
    VALUE capacity_val, error_rate_val;
    rb_scan_args(argc, argv, "11", &capacity_val, &error_rate_val);
    
    long capacity = NUM2LONG(capacity_val);
    double error_rate = NIL_P(error_rate_val) ? 0.01 : NUM2DBL(error_rate_val);
    
    if (capacity <= 0) {
        rb_raise(rb_eArgError, "capacity must be positive");
    }
    
    if (error_rate <= 0 || error_rate >= 1) {
        rb_raise(rb_eArgError, "error_rate must be between 0 and 1");
    }
    
    BloomFilter *bloom;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom);
    
    /* Calculate optimal parameters */
    double ln2 = 0.693147180559945309417;
    double ln2_sq = ln2 * ln2;
    
    size_t bits_count = (size_t)(-(capacity * log(error_rate)) / ln2_sq);
    bloom->size = (bits_count + 7) / 8;
    bloom->capacity = capacity;
    bloom->num_hashes = (int)((bits_count / (double)capacity) * ln2);
    
    if (bloom->num_hashes < 1) bloom->num_hashes = 1;
    if (bloom->num_hashes > 10) bloom->num_hashes = 10;
    
    bloom->bits = (uint8_t *)calloc(bloom->size, sizeof(uint8_t));
    if (!bloom->bits) {
        rb_raise(rb_eNoMemError, "failed to allocate memory");
    }
    
    return self;
}

/*
 * Add element to filter
 */
static VALUE bloom_add(VALUE self, VALUE str) {
    BloomFilter *bloom;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom);
    
    Check_Type(str, T_STRING);
    
    const char *data = RSTRING_PTR(str);
    size_t len = RSTRING_LEN(str);
    size_t bits_count = bloom->size * 8;
    
    for (int i = 0; i < bloom->num_hashes; i++) {
        uint32_t hash = murmur3_32((const uint8_t *)data, len, i);
        size_t pos = hash % bits_count;
        set_bit(bloom->bits, pos);
    }
    
    return Qtrue;
}

/*
 * Check if element might be in filter
 */
static VALUE bloom_include(VALUE self, VALUE str) {
    BloomFilter *bloom;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom);
    
    Check_Type(str, T_STRING);
    
    const char *data = RSTRING_PTR(str);
    size_t len = RSTRING_LEN(str);
    size_t bits_count = bloom->size * 8;
    
    for (int i = 0; i < bloom->num_hashes; i++) {
        uint32_t hash = murmur3_32((const uint8_t *)data, len, i);
        size_t pos = hash % bits_count;
        if (!get_bit(bloom->bits, pos)) {
            return Qfalse;
        }
    }
    
    return Qtrue;
}

/*
 * Clear all bits
 */
static VALUE bloom_clear(VALUE self) {
    BloomFilter *bloom;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom);
    
    memset(bloom->bits, 0, bloom->size);
    return Qnil;
}

/*
 * Get filter statistics
 */
static VALUE bloom_stats(VALUE self) {
    BloomFilter *bloom;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom);
    
    size_t bits_set = 0;
    size_t total_bits = bloom->size * 8;
    
    for (size_t i = 0; i < bloom->size; i++) {
        uint8_t byte = bloom->bits[i];
        while (byte) {
            bits_set += byte & 1;
            byte >>= 1;
        }
    }
    
    double fill_ratio = (double)bits_set / total_bits;
    
    VALUE hash = rb_hash_new();
    rb_hash_aset(hash, ID2SYM(rb_intern("capacity")), LONG2NUM(bloom->capacity));
    rb_hash_aset(hash, ID2SYM(rb_intern("size_bytes")), LONG2NUM(bloom->size));
    rb_hash_aset(hash, ID2SYM(rb_intern("num_hashes")), INT2NUM(bloom->num_hashes));
    rb_hash_aset(hash, ID2SYM(rb_intern("bits_set")), LONG2NUM(bits_set));
    rb_hash_aset(hash, ID2SYM(rb_intern("total_bits")), LONG2NUM(total_bits));
    rb_hash_aset(hash, ID2SYM(rb_intern("fill_ratio")), DBL2NUM(fill_ratio));
    
    return hash;
}

/*
 * Merge another filter
 */
static VALUE bloom_merge(VALUE self, VALUE other) {
    BloomFilter *bloom1, *bloom2;
    TypedData_Get_Struct(self, BloomFilter, &bloom_type, bloom1);
    TypedData_Get_Struct(other, BloomFilter, &bloom_type, bloom2);
    
    if (bloom1->size != bloom2->size || bloom1->num_hashes != bloom2->num_hashes) {
        rb_raise(rb_eArgError, "cannot merge filters with different parameters");
    }
    
    for (size_t i = 0; i < bloom1->size; i++) {
        bloom1->bits[i] |= bloom2->bits[i];
    }
    
    return self;
}

void Init_fast_bloom_filter(void) {
    VALUE mFastBloomFilter = rb_define_module("FastBloomFilter");
    VALUE cBloomFilter = rb_define_class_under(mFastBloomFilter, "Filter", rb_cObject);
    
    rb_define_alloc_func(cBloomFilter, bloom_alloc);
    rb_define_method(cBloomFilter, "initialize", bloom_initialize, -1);
    rb_define_method(cBloomFilter, "add", bloom_add, 1);
    rb_define_method(cBloomFilter, "<<", bloom_add, 1);
    rb_define_method(cBloomFilter, "include?", bloom_include, 1);
    rb_define_method(cBloomFilter, "member?", bloom_include, 1);
    rb_define_method(cBloomFilter, "clear", bloom_clear, 0);
    rb_define_method(cBloomFilter, "stats", bloom_stats, 0);
    rb_define_method(cBloomFilter, "merge!", bloom_merge, 1);
}
