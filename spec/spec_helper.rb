SPEC_DIR = File.expand_path('..', __FILE__)

Dir['spec/support/**/*.rb'].map {|f| require_relative f }

def in_editor?
  %w[VIM EMACS TM_MODE].any? {|var| ENV.has_key?(var) }
end

RSpec.configure do |c|
  c.color_enabled = !in_editor?
  c.filter_run(focused:true)
  c.run_all_when_everything_filtered = true
end

require 'image_file'
require 'cairo'
