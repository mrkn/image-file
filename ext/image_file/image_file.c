#include "internal.h"

VALUE mImageFile = Qnil;

void
Init_image_file(void)
{
    mImageFile = rb_define_module("ImageFile");

    rb_image_file_Init_image_file_image();
}
