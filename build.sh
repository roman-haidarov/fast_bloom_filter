#!/bin/bash
set -e

echo "Building FastBloomFilter..."
echo ""

# Clean
echo "Cleaning..."
rm -f *.gem
rm -rf lib/fast_bloom_filter/*.{so,bundle}
cd ext/fast_bloom_filter && make clean 2>/dev/null || true && cd ../..

# Compile
echo "Compiling C extension..."
cd ext/fast_bloom_filter
ruby extconf.rb
make
cd ../..

# Copy (detect .so or .bundle)
echo "Copying library..."
mkdir -p lib/fast_bloom_filter
if [ -f ext/fast_bloom_filter/fast_bloom_filter.bundle ]; then
    cp ext/fast_bloom_filter/fast_bloom_filter.bundle lib/fast_bloom_filter/
    echo "Copied .bundle file (macOS)"
elif [ -f ext/fast_bloom_filter/fast_bloom_filter.so ]; then
    cp ext/fast_bloom_filter/fast_bloom_filter.so lib/fast_bloom_filter/
    echo "Copied .so file (Linux)"
else
    echo "Error: No compiled extension found!"
    exit 1
fi

# Test (avoid Rails plugin conflicts)
echo "Running tests..."
ruby -I lib test/fast_bloom_filter_test.rb

# Build gem
echo "Building gem..."
gem build fast_bloom_filter.gemspec

echo ""
echo "Done! Gem: fast_bloom_filter-1.0.0.gem"
echo ""
echo "To test manually:"
echo "  ruby demo.rb"
echo ""
echo "To install:"
echo "  gem install fast_bloom_filter-1.0.0.gem"
