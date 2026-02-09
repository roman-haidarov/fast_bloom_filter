# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
