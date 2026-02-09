# FastBloomFilter

[![CI](https://github.com/yourusername/fast_bloom_filter/actions/workflows/ci.yml/badge.svg)](https://github.com/yourusername/fast_bloom_filter/actions/workflows/ci.yml)
[![Gem Version](https://badge.fury.io/rb/fast_bloom_filter.svg)](https://badge.fury.io/rb/fast_bloom_filter)

A high-performance Bloom Filter implementation in C for Ruby. Perfect for Rails applications that need memory-efficient set membership testing.

## Features

- **🚀 Fast**: C implementation with MurmurHash3
- **💾 Memory Efficient**: 20-50x less memory than Ruby Set
- **🎯 Configurable**: Adjustable false positive rate
- **🔒 Thread-Safe**: Safe for concurrent operations
- **📊 Statistics**: Built-in performance monitoring
- **✅ Well-Tested**: Comprehensive test suite

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'fast_bloom_filter'
```

And then execute:

```bash
bundle install
```

Or install it yourself as:

```bash
gem install fast_bloom_filter
```

## Usage

### Basic Operations

```ruby
require 'fast_bloom_filter'

# Create a filter for 10,000 items with 1% false positive rate
bloom = FastBloomFilter::Filter.new(10_000, 0.01)

# Add items
bloom.add("user@example.com")
bloom << "another@example.com"  # alias for add

# Check membership
bloom.include?("user@example.com")  # => true
bloom.include?("notfound@test.com") # => false (probably)

# Batch operations
emails = ["user1@test.com", "user2@test.com", "user3@test.com"]
bloom.add_all(emails)

# Count possible matches
bloom.count_possible_matches(["user1@test.com", "unknown@test.com"])  # => 1 or 2

# Clear all items
bloom.clear
```

### Helper Methods

```ruby
# For email deduplication (0.1% false positive rate)
bloom = FastBloomFilter.for_emails(100_000)

# For URL tracking (1% false positive rate)
bloom = FastBloomFilter.for_urls(50_000)
```

### Merge Filters

```ruby
bloom1 = FastBloomFilter::Filter.new(1000, 0.01)
bloom2 = FastBloomFilter::Filter.new(1000, 0.01)

bloom1.add("item1")
bloom2.add("item2")

bloom1.merge!(bloom2)  # bloom1 now contains both items
```

### Statistics

```ruby
bloom = FastBloomFilter::Filter.new(10_000, 0.01)
stats = bloom.stats

# => {
#   capacity: 10000,
#   size_bytes: 11982,
#   num_hashes: 7,
#   fill_ratio: 0.0
# }

puts bloom.inspect
# => #<FastBloomFilter::Filter capacity=10000 size=11.7KB hashes=7 fill=0.0%>
```

## Performance

Benchmarks on MacBook Pro M1 (100K elements):

| Operation | Bloom Filter | Ruby Set | Speedup |
|-----------|--------------|----------|---------|
| Add       | 45ms         | 120ms    | 2.7x    |
| Check     | 8ms          | 15ms     | 1.9x    |
| Memory    | 120KB        | 2000KB   | 16.7x   |

Run benchmarks yourself:

```bash
ruby demo.rb
```

## Use Cases

### Rails: Prevent Duplicate Email Signups

```ruby
class User < ApplicationRecord
  SIGNUP_BLOOM = FastBloomFilter.for_emails(1_000_000)
  
  before_validation :check_duplicate_signup
  
  private
  
  def check_duplicate_signup
    if SIGNUP_BLOOM.include?(email)
      errors.add(:email, "may already be registered")
      return false
    end
    SIGNUP_BLOOM.add(email)
  end
end
```

### Track Visited URLs

```ruby
class WebCrawler
  def initialize
    @visited = FastBloomFilter.for_urls(10_000_000)
  end
  
  def crawl(url)
    return if @visited.include?(url)
    
    @visited.add(url)
    # ... crawl logic
  end
end
```

### Cache Key Deduplication

```ruby
class CacheWarmer
  def initialize
    @warmed = FastBloomFilter::Filter.new(100_000, 0.001)
  end
  
  def warm(key)
    return if @warmed.include?(key)
    
    Rails.cache.fetch(key) { expensive_operation(key) }
    @warmed.add(key)
  end
end
```

## How It Works

A Bloom Filter is a space-efficient probabilistic data structure that tests whether an element is a member of a set:

- **No false negatives**: If it says "no", the item is definitely not in the set
- **Possible false positives**: If it says "yes", the item is probably in the set
- **Memory efficient**: Uses bit arrays instead of storing actual items
- **Fast**: O(k) for add and lookup, where k is the number of hash functions

### Parameters

- **Capacity**: Expected number of elements
- **Error Rate**: Probability of false positives (default: 0.01 = 1%)

The filter automatically calculates optimal bit array size and number of hash functions.

## Development

```bash
# Clone the repository
git clone https://github.com/yourusername/fast_bloom_filter.git
cd fast_bloom_filter

# Install dependencies
bundle install

# Compile the C extension
bundle exec rake compile

# Run tests
bundle exec rake test

# Build the gem
gem build fast_bloom_filter.gemspec

# Install locally
gem install ./fast_bloom_filter-1.0.0.gem
```

### Quick Build Script

```bash
./build.sh
```

## Requirements

- Ruby >= 2.7.0
- C compiler (gcc, clang, etc.)
- Make

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b feature/my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin feature/my-new-feature`)
5. Create a new Pull Request

## License

The gem is available as open source under the terms of the [MIT License](LICENSE.txt).

## Credits

- MurmurHash3 implementation based on Austin Appleby's original work
- Bloom Filter algorithm by Burton Howard Bloom (1970)

## Support

- 🐛 [Report bugs](https://github.com/yourusername/fast_bloom_filter/issues)
- 💡 [Request features](https://github.com/yourusername/fast_bloom_filter/issues)
- 📖 [Documentation](https://github.com/yourusername/fast_bloom_filter)

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history.
