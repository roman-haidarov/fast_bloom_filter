# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-02-12

### 🚀 Major Release - Scalable Bloom Filter

This is a **breaking change** that transforms FastBloomFilter into a scalable, dynamic data structure.

### Added
- **Scalable Architecture**: Filter now grows automatically by adding layers
- **No Upfront Capacity**: No need to specify capacity - just set error_rate
- **Multi-Layer System**: Each layer has progressively tighter error rates
- **Smart Growth Strategy**: Growth factor starts at 2x and decreases (like Go slices)
- **Layer Statistics**: Detailed per-layer stats via `stats` method
- **New API**: `Filter.new(error_rate: 0.01, initial_capacity: 1024)`
- `num_layers` method to check how many layers are active
- Enhanced `merge!` to combine filters with all their layers

### Changed
- **BREAKING**: Constructor now uses keyword arguments: `Filter.new(error_rate: 0.01)` instead of `Filter.new(capacity, error_rate)`
- **BREAKING**: `stats` now returns multi-layer information with `:layers` array
- **BREAKING**: Helper methods changed: `for_emails(error_rate: 0.001)` instead of `for_emails(capacity)`
- Memory allocation is now dynamic and grows on-demand
- `inspect` output now shows layer count and total elements

### Technical Details
- Based on "Scalable Bloom Filters" (Almeida et al., 2007)
- Each layer uses error_rate * (1 - r) * r^i formula
- Default tightening factor (r) = 0.85
- Growth factors: 2x → 1.75x → 1.5x → 1.25x as layers increase
- Layers are checked from newest to oldest for better cache locality

### Migration Guide

**v1.x code:**
```ruby
bloom = FastBloomFilter::Filter.new(10_000, 0.01)
bloom = FastBloomFilter.for_emails(100_000)
```

**v2.x code:**
```ruby
bloom = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 1000)
bloom = FastBloomFilter.for_emails(error_rate: 0.001, initial_capacity: 10_000)
# Or simply:
bloom = FastBloomFilter::Filter.new(error_rate: 0.01)  # starts small, grows as needed
```

### Performance
- Same O(k) complexity for add/lookup
- Slightly higher memory overhead due to layer management
- Better memory efficiency for unknown/growing datasets
- No performance degradation as filter grows

---

## [1.0.0] - 2026-02-09

### Added
- Initial release of FastBloomFilter
- High-performance C implementation of Bloom Filter
- Basic operations: `add`, `include?`, `clear`
- Batch operations: `add_all`, `count_possible_matches`
- Merge functionality with `merge!`
- Statistics via `stats` method
- Helper methods: `for_emails`, `for_urls`
- Comprehensive test suite
- Performance benchmarks
- Full documentation

### Features
- 20-50x less memory usage compared to Ruby Set
- Configurable false positive rate
- Thread-safe operations
- Memory-efficient bit array implementation
- MurmurHash3 for fast hashing
