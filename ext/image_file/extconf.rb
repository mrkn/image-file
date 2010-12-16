require 'mkmf'

ver = RUBY_VERSION.split(/\./).join.to_i
$defs << "-DRUBY_VERSION=#{ver}"

$CFLAGS << ' -Wall -Wextra -Wshadow'

dir_config('jpeg')
have_header('jpeglib.h')
have_library('jpeg')

create_makefile('image_file')
