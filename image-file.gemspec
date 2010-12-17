# -*- encoding: utf-8 -*-

$LOAD_PATH.unshift File.expand_path('../lib', __FILE__)
require 'image_file/version'

Gem::Specification.new do |s|
  s.name        = 'image-file'
  s.version     = ImageFile::Version::STRING
  s.platform    = Gem::Platform::RUBY
  s.authors     = ['Kenta Murata']
  s.email       = 'mrkn@mrkn.jp'
  s.homepage    = 'http://github.com/mrkn/image-file'
  s.summary     = "image-file-#{ImageFile::Version::STRING}"
  s.description = 'A library for handling image files'

  s.rubygems_version   = '1.3.7'
  #s.rubyforge_project = 'image-file'

  s.files            = `git ls-files`.split("\n")
  s.test_files       = `git ls-files -- {spec,features}/*`.split("\n")
  s.executables      = `git ls-files -- bin/*`.split("\n").map {|f| File.basename(f) }
  s.extensions       = `git ls-files -- ext/*`.split("\n").grep(/extconf\.rb\Z/)
  s.extra_rdoc_files = []
  s.rdoc_options     = ['--charset=UTF-8']
  s.require_path     = 'lib'
end
