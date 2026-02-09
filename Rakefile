require "bundler/gem_tasks"
require "rake/testtask"
require "rake/extensiontask"

Rake::ExtensionTask.new("fast_bloom_filter") do |ext|
  ext.lib_dir = "lib/fast_bloom_filter"
  ext.ext_dir = "ext/fast_bloom_filter"
end

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/*_test.rb"]
end

task default: [:compile, :test]

task :console => :compile do
  require "irb"
  require "fast_bloom_filter"
  ARGV.clear
  IRB.start
end
