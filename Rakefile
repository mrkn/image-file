require 'bundler'
Bundler.setup

require 'rake/extensiontask'
Rake::ExtensionTask.new('image_file')

require 'rake'
require 'rspec/core/rake_task'

desc "Run all examples"
RSpec::Core::RakeTask.new(:spec)

task :default => [:spec]
task :spec => [:compile]
