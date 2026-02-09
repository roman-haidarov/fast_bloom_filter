#!/usr/bin/env ruby
require "./lib/fast_bloom_filter"
require 'benchmark'

puts "\n#{'=' * 70}"
puts "FastBloomFilter Demo"
puts "#{'=' * 70}\n\n"

# 1. Basic ops
puts "1. Basic Operations"
puts "-" * 70
bloom = FastBloomFilter::Filter.new(10_000, 0.01)
bloom.add_all(["alice@example.com", "bob@test.com"])

[["alice@example.com", "exists"], ["notfound@test.com", "does NOT exist"]].each do |email, status|
  result = bloom.include?(email)
  puts "  #{result ? '✓' : '✗'} #{email.ljust(30)} => #{result} (#{status})"
end

# 2. Stats
puts "\n2. Statistics"
puts "-" * 70
s = bloom.stats
puts "  Capacity:  #{s[:capacity]} elements"
puts "  Memory:    #{s[:size_bytes]} bytes (~#{(s[:size_bytes]/1024.0).round(2)} KB)"
puts "  Hashes:    #{s[:num_hashes]}"
puts "  Fill:      #{(s[:fill_ratio]*100).round(4)}%"

# 3. Performance
puts "\n3. Performance (100K elements)"
puts "-" * 70

items = 100_000.times.map { |i| "user#{i}@test.com" }
check = 10_000.times.map { |i| "check#{i}@test.com" }

bloom3 = FastBloomFilter::Filter.new(100_000, 0.01)
time_add = Benchmark.realtime { items.each { |e| bloom3.add(e) } }
time_check = Benchmark.realtime { check.each { |e| bloom3.include?(e) } }

require 'set'
set = Set.new
time_add_set = Benchmark.realtime { items.each { |e| set.add(e) } }
time_check_set = Benchmark.realtime { check.each { |e| set.include?(e) } }

puts "                 Bloom Filter      Ruby Set        Speedup"
puts "  Add:          #{(time_add*1000).round(2)}ms       #{(time_add_set*1000).round(2)}ms      #{(time_add_set/time_add).round(2)}x"
puts "  Check:        #{(time_check*1000).round(2)}ms       #{(time_check_set*1000).round(2)}ms      #{(time_check_set/time_check).round(2)}x"
puts "  Memory:       #{(bloom3.stats[:size_bytes]/1024.0).round(2)}KB       ~#{(100_000*20/1024.0).round(2)}KB      #{(100_000*20.0/bloom3.stats[:size_bytes]).round(1)}x"

puts "\n#{'=' * 70}"
puts "Demo complete!"
puts "#{'=' * 70}\n"
