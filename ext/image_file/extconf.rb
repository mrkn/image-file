require 'mkmf'

ver = RUBY_VERSION.split(/\./).join.to_i
$defs << "-DRUBY_VERSION=#{ver}"

create_makefile('image_file')
