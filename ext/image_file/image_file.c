#include <ruby.h>

VALUE rb_image_file_mImageFile = Qnil;
#define mImageFile rb_image_file_mImageFile

void
Init_image_file(void)
{
    mImageFile = rb_define_module("ImageFile");
}
