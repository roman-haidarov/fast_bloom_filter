require 'fast_bloom_filter/version'

# Load the compiled extension (.so on Linux, .bundle on macOS)
begin
  require 'fast_bloom_filter/fast_bloom_filter'
rescue LoadError
  # Fallback for different extension names
  ext_dir = File.expand_path('../fast_bloom_filter', __FILE__)
  if File.exist?(File.join(ext_dir, 'fast_bloom_filter.bundle'))
    require File.join(ext_dir, 'fast_bloom_filter.bundle')
  elsif File.exist?(File.join(ext_dir, 'fast_bloom_filter.so'))
    require File.join(ext_dir, 'fast_bloom_filter.so')
  else
    raise LoadError, "Could not find compiled extension"
  end
end

module FastBloomFilter
  class Filter
    def add_all(items)
      items.each { |item| add(item.to_s) }
      self
    end
    
    def count_possible_matches(items)
      items.count { |item| include?(item.to_s) }
    end
    
    def inspect
      s = stats
      size_kb = (s[:size_bytes] / 1024.0).round(2)
      fill_pct = (s[:fill_ratio] * 100).round(2)
      
      "#<FastBloomFilter::Filter capacity=#{s[:capacity]} " \
      "size=#{size_kb}KB hashes=#{s[:num_hashes]} fill=#{fill_pct}%>"
    end
    
    def to_s
      inspect
    end
  end
  
  def self.for_emails(capacity, error_rate: 0.001)
    Filter.new(capacity, error_rate)
  end
  
  def self.for_urls(capacity, error_rate: 0.01)
    Filter.new(capacity, error_rate)
  end
end
