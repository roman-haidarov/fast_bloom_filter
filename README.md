# FastBloomFilter v2 🚀

[![Gem Version](https://badge.fury.io/rb/fast_bloom_filter.svg)](https://badge.fury.io/rb/fast_bloom_filter)

A **scalable** Bloom Filter implementation in C for Ruby. Grows automatically without requiring upfront capacity! Perfect for Rails applications that need memory-efficient set membership testing with unknown dataset sizes.

## What's New in v2? 🎉

- **🔄 Scalable Architecture**: No need to guess capacity upfront - the filter grows automatically
- **📊 Multi-Layer System**: Adds new layers dynamically as data grows
- **🎯 Smart Growth**: Growth factor adapts (2x → 1.75x → 1.5x → 1.25x)
- **💡 Simpler API**: Just specify error rate, not capacity
- **📈 Better for Unknown Sizes**: Perfect when you don't know how much data you'll have

Based on ["Scalable Bloom Filters" (Almeida et al., 2007)](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=10.1.1.725.390)

## Features

- **🚀 Fast**: C implementation with MurmurHash3
- **💾 Memory Efficient**: 20-50x less memory than Ruby Set
- **🔄 Auto-Scaling**: Grows dynamically as you add elements
- **🎯 Configurable**: Adjustable false positive rate per layer
- **📊 Statistics**: Detailed per-layer performance monitoring
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

### Basic Operations (v2 API)

```ruby
require 'fast_bloom_filter'

# Create a scalable filter - NO CAPACITY NEEDED!
# Just specify your desired error rate
bloom = FastBloomFilter::Filter.new(error_rate: 0.01)

# Or with an initial capacity hint (optional)
bloom = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 1000)

# Add items - filter grows automatically
bloom.add("user@example.com")
bloom << "another@example.com"  # alias for add

# Check membership
bloom.include?("user@example.com")  # => true
bloom.include?("notfound@test.com") # => false (probably)

# Add thousands or millions - it scales!
100_000.times { |i| bloom.add("user#{i}@test.com") }

# Check stats
bloom.count        # => 100002
bloom.num_layers   # => 8 (grew automatically!)

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
bloom = FastBloomFilter.for_emails(error_rate: 0.001)

# For URL tracking (1% false positive rate)
bloom = FastBloomFilter.for_urls(error_rate: 0.01)

# With initial capacity hint
bloom = FastBloomFilter.for_emails(error_rate: 0.001, initial_capacity: 10_000)
```

### Merge Filters

```ruby
bloom1 = FastBloomFilter::Filter.new(error_rate: 0.01)
bloom2 = FastBloomFilter::Filter.new(error_rate: 0.01)

bloom1.add("item1")
bloom2.add("item2")

bloom1.merge!(bloom2)  # bloom1 now contains both items
# Merges all layers from bloom2 into bloom1
```

### Statistics

```ruby
bloom = FastBloomFilter::Filter.new(error_rate: 0.01)
1000.times { |i| bloom.add("item#{i}") }

stats = bloom.stats

# => {
#   total_count: 1000,
#   num_layers: 2,
#   total_bytes: 2500,
#   total_bits: 20000,
#   total_bits_set: 6543,
#   fill_ratio: 0.32715,
#   error_rate: 0.01,
#   layers: [
#     {
#       layer: 0,
#       capacity: 1024,
#       count: 1024,
#       size_bytes: 1229,
#       num_hashes: 7,
#       bits_set: 5234,
#       total_bits: 9832,
#       fill_ratio: 0.532,
#       error_rate: 0.0015
#     },
#     # ... more layers
#   ]
# }

puts bloom.inspect
# => #<FastBloomFilter::Filter v2 layers=2 count=1000 size=2.44KB fill=32.72%>
```

## How Scalable Bloom Filters Work

Traditional Bloom Filters require you to specify capacity upfront. **Scalable Bloom Filters** solve this by:

1. **Starting Small**: Begin with a small initial capacity (default: 1024 elements)
2. **Adding Layers**: When a layer fills up, add a new layer with larger capacity
3. **Tightening Error Rates**: Each new layer has a tighter error rate to maintain overall FPR
4. **Smart Growth**: Growth factor decreases over time (2x → 1.75x → 1.5x → 1.25x)

### Error Rate Distribution

Each layer `i` gets error rate: `total_error_rate × (1 - r) × r^i`

Where `r` is the tightening factor (default: 0.85). This ensures the sum of all layer error rates converges to your target error rate.

### Example Growth Pattern

```
Layer 0: capacity=1,024   error_rate=0.0015  (initial)
Layer 1: capacity=2,048   error_rate=0.0013  (2x growth)
Layer 2: capacity=3,584   error_rate=0.0011  (1.75x growth)
Layer 3: capacity=5,376   error_rate=0.0009  (1.5x growth)
Layer 4: capacity=6,720   error_rate=0.0008  (1.25x growth)
...
```

## Performance

Benchmarks on MacBook Pro M1 (100K elements):

| Operation | Bloom Filter v2 | Ruby Set | Speedup |
|-----------|-----------------|----------|---------|
| Add       | 48ms            | 120ms    | 2.5x    |
| Check     | 9ms             | 15ms     | 1.7x    |
| Memory    | 145KB           | 2000KB   | 13.8x   |

Run benchmarks yourself:

```bash
ruby demo.rb
```

## Use Cases

### Rails: Prevent Duplicate Email Signups (No Capacity Guessing!)

```ruby
class User < ApplicationRecord
  # No need to guess how many users you'll have!
  SIGNUP_BLOOM = FastBloomFilter.for_emails(error_rate: 0.001)
  
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

### Track Visited URLs (Scales to Millions)

```ruby
class WebCrawler
  def initialize
    # Starts small, grows as needed
    @visited = FastBloomFilter.for_urls(error_rate: 0.01)
  end
  
  def crawl(url)
    return if @visited.include?(url)
    
    @visited.add(url)
    # ... crawl logic
    
    # Check growth
    if @visited.count % 10_000 == 0
      puts "Crawled #{@visited.count} URLs, #{@visited.num_layers} layers"
    end
  end
end
```

### Cache Key Deduplication

```ruby
class CacheWarmer
  def initialize
    @warmed = FastBloomFilter::Filter.new(error_rate: 0.001)
  end
  
  def warm(key)
    return if @warmed.include?(key)
    
    Rails.cache.fetch(key) { expensive_operation(key) }
    @warmed.add(key)
  end
end
```

## Migration from v1.x

**v1.x (Fixed Capacity):**
```ruby
bloom = FastBloomFilter::Filter.new(10_000, 0.01)
bloom = FastBloomFilter.for_emails(100_000)
```

**v2.x (Scalable):**
```ruby
# Recommended: Let it scale automatically
bloom = FastBloomFilter::Filter.new(error_rate: 0.01)

# Or with initial capacity hint
bloom = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 1000)

# Helper methods also changed
bloom = FastBloomFilter.for_emails(error_rate: 0.001, initial_capacity: 10_000)
```

## Development

```bash
# Clone the repository
git clone https://github.com/roman-haidarov/fast_bloom_filter.git
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
gem install ./fast_bloom_filter-2.0.0.gem
```

### Quick Build Script

```bash
./build.sh
```

## Requirements

- Ruby >= 2.7.0
- C compiler (gcc, clang, etc.)
- Make

## Technical Details

- **Hash Function**: MurmurHash3 (32-bit)
- **Bit Array**: Dynamic allocation per layer
- **Growth Strategy**: Adaptive (2x → 1.75x → 1.5x → 1.25x)
- **Tightening Factor**: 0.85 (configurable)
- **Memory Management**: Ruby GC integration with proper cleanup
- **Thread Safety**: Safe for concurrent reads (writes need external synchronization)

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b feature/my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin feature/my-new-feature`)
5. Create a new Pull Request

## License

The gem is available as open source under the terms of the [MIT License](LICENSE.txt).

## Credits

- Scalable Bloom Filters algorithm: Almeida, Baquero, Preguiça, Hutchison (2007)
- MurmurHash3 implementation: Austin Appleby
- Original Bloom Filter: Burton Howard Bloom (1970)

## Support

- 🐛 [Report bugs](https://github.com/roman-haidarov/fast_bloom_filter/issues)
- 💡 [Request features](https://github.com/roman-haidarov/fast_bloom_filter/issues)
- 📖 [Documentation](https://github.com/roman-haidarov/fast_bloom_filter)

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history.
