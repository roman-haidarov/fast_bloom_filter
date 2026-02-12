#!/usr/bin/env ruby
require "./lib/fast_bloom_filter"
require 'benchmark'

puts "\n#{'=' * 70}"
puts "FastBloomFilter v2 Demo - Scalable Bloom Filter"
puts "#{'=' * 70}\n\n"

# 1. Basic ops - NO CAPACITY NEEDED!
puts "1. Basic Operations (Scalable - No Capacity Needed!)"
puts "-" * 70
bloom = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 100)
bloom.add_all(["alice@example.com", "bob@test.com"])

[["alice@example.com", "exists"], ["notfound@test.com", "does NOT exist"]].each do |email, status|
  result = bloom.include?(email)
  puts "  #{result ? '✓' : '✗'} #{email.ljust(30)} => #{result} (#{status})"
end

puts "\n  Filter info: #{bloom.inspect}"

# 2. Scalability Demo
puts "\n2. Scalability Demo - Watch it Grow!"
puts "-" * 70
bloom2 = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 100)

[100, 500, 1000, 5000, 10000].each do |n|
  # Add elements up to n
  current = bloom2.count
  (current...n).each { |i| bloom2.add("item#{i}") }
  
  s = bloom2.stats
  puts "  After #{n.to_s.rjust(5)} items: #{s[:num_layers]} layers, " \
       "#{(s[:total_bytes]/1024.0).round(2)}KB, " \
       "fill=#{(s[:fill_ratio]*100).round(2)}%"
end

# 3. Layer Details
puts "\n3. Layer-by-Layer Statistics"
puts "-" * 70
s = bloom2.stats
s[:layers].each do |layer|
  puts "  Layer #{layer[:layer]}: " \
       "capacity=#{layer[:capacity]}, " \
       "count=#{layer[:count]}, " \
       "#{layer[:size_bytes]}B, " \
       "hashes=#{layer[:num_hashes]}, " \
       "fill=#{(layer[:fill_ratio]*100).round(2)}%"
end

# 4. Performance
puts "\n4. Performance (100K elements)"
puts "-" * 70

items = 100_000.times.map { |i| "user#{i}@test.com" }
check = 10_000.times.map { |i| "check#{i}@test.com" }

bloom3 = FastBloomFilter::Filter.new(error_rate: 0.01, initial_capacity: 1024)
time_add = Benchmark.realtime { items.each { |e| bloom3.add(e) } }
time_check = Benchmark.realtime { check.each { |e| bloom3.include?(e) } }

require 'set'
set = Set.new
time_add_set = Benchmark.realtime { items.each { |e| set.add(e) } }
time_check_set = Benchmark.realtime { check.each { |e| set.include?(e) } }

puts "                 Bloom Filter      Ruby Set        Speedup"
puts "  Add:          #{(time_add*1000).round(2)}ms       #{(time_add_set*1000).round(2)}ms      #{(time_add_set/time_add).round(2)}x"
puts "  Check:        #{(time_check*1000).round(2)}ms       #{(time_check_set*1000).round(2)}ms      #{(time_check_set/time_check).round(2)}x"
puts "  Memory:       #{(bloom3.stats[:total_bytes]/1024.0).round(2)}KB       ~#{(100_000*20/1024.0).round(2)}KB      #{(100_000*20.0/bloom3.stats[:total_bytes]).round(1)}x"

puts "\n  Final filter: #{bloom3.stats[:num_layers]} layers, #{bloom3.count} elements"

# 5. Merge Demo
puts "\n5. Merge Filters"
puts "-" * 70
f1 = FastBloomFilter::Filter.new(error_rate: 0.01)
f2 = FastBloomFilter::Filter.new(error_rate: 0.01)

f1.add("item1")
f1.add("item2")
f2.add("item3")
f2.add("item4")

puts "  Filter 1: #{f1.count} items, #{f1.num_layers} layers"
puts "  Filter 2: #{f2.count} items, #{f2.num_layers} layers"

f1.merge!(f2)
puts "  After merge: #{f1.count} items, #{f1.num_layers} layers"
puts "  Contains 'item3'? #{f1.include?('item3')}"

puts "\n#{'=' * 70}"
puts "Demo complete! v2 features:"
puts "  ✓ No upfront capacity needed"
puts "  ✓ Automatic scaling with multiple layers"
puts "  ✓ Configurable error rate per layer"
puts "  ✓ Memory-efficient growth strategy"
puts "#{'=' * 70}\n"
