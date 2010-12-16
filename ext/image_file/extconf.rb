require 'mkmf'
require 'rubygems'
require 'pkg-config'

ver = RUBY_VERSION.split(/\./).join.to_i
$defs << "-DRUBY_VERSION=#{ver}"

$CFLAGS << ' -Wall -Wextra -Wshadow'

dir_config('jpeg')
have_header('jpeglib.h')
have_library('jpeg')

if PKGConfig.have_package('cairo', 1, 2, 0)
  unless have_header('rb_cairo.h')
    if cairo = Gem.searcher.find('cairo')
      extconf_rb = File.expand_path(cairo.extensions[0], cairo.full_gem_path)
      rb_cairo_h = File.expand_path('../rb_cairo.h', extconf_rb)
      $CFLAGS << " -I#{File.dirname(rb_cairo_h)}"
      have_header('rb_cairo.h')
    end
  end
end

create_makefile('image_file')
