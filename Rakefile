require 'bundler'
Bundler.setup
Bundler::GemHelper.install_tasks

require 'rake/extensiontask'
Rake::ExtensionTask.new('image_file')

require 'rspec/core/rake_task'

desc "Run all examples"
RSpec::Core::RakeTask.new(:spec)

task :default => [:spec]
task :spec => [:compile]
