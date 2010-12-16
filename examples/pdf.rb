require 'image_file'
require 'cairo'

image = ImageFile::JpegReader.open(ARGV[0]).read_image

image_surface = image.create_cairo_surface

pdf = Cairo::PDFSurface.new(ARGV[1], image.width, image.height)
context = Cairo::Context.new(pdf)

context.set_source image_surface
context.paint
context.show_page

pdf.finish
