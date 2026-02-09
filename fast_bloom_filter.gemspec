require_relative 'lib/fast_bloom_filter/version'

Gem::Specification.new do |spec|
  spec.name          = "fast_bloom_filter"
  spec.version       = FastBloomFilter::VERSION
  spec.authors       = ["Roman Haydarov"]
  spec.email         = ["romnhajdarov@gmail.com"]

  spec.summary       = "High-performance Bloom Filter in C for Ruby"
  spec.description   = "Memory-efficient probabilistic data structure. 20-50x less memory than Set, perfect for Rails apps."
  spec.homepage      = "https://github.com/roman-haidarov/fast_bloom_filter"
  spec.license       = "MIT"
  spec.required_ruby_version = ">= 2.7.0"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage
  spec.metadata["changelog_uri"] = "#{spec.homepage}/blob/main/CHANGELOG.md"

  spec.files = Dir[
    'lib/**/*.rb',
    'ext/**/*.{c,h,rb}',
    'README.md',
    'LICENSE.txt',
    'CHANGELOG.md'
  ]

  spec.bindir        = "exe"
  spec.executables   = []
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/fast_bloom_filter/extconf.rb"]

  spec.add_development_dependency "bundler", "~> 2.0"
  spec.add_development_dependency "rake", "~> 13.0"
  spec.add_development_dependency "rake-compiler", "~> 1.2"
  spec.add_development_dependency "minitest", "~> 5.0"
end
