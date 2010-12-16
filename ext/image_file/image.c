#include "internal.h"

#ifdef HAVE_RB_CAIRO_H
# include <rb_cairo.h>
#endif

VALUE cImageFileImage = Qnil;

static ID id_ARGB32;
static ID id_RGB24;
static ID id_RGB16_565;

struct image_data {
    VALUE buffer;
    rb_image_file_image_pixel_format_t pixel_format;
    long width;
    long height;
    long stride;
};

static void
image_mark(void* ptr)
{
    struct image_data* image = (struct image_data*)ptr;
    rb_gc_mark(image->buffer);
}

static void
image_free(void* ptr)
{
    struct image_data* image = (struct image_data*)ptr;
    image->buffer = Qnil;
    xfree(image);
}

static size_t
image_memsize(void const* ptr)
{
    return ptr ? sizeof(struct image_data) : 0;
}

static rb_data_type_t const image_data_type = {
    "image_file::image",
#if RUBY_VERSION >= 193
    {
#endif
	image_mark,
	image_free,
	image_memsize,
#if RUBY_VERSION >= 193
    },
#endif
};

static VALUE
image_alloc(VALUE const klass)
{
    struct image_data* image;
    VALUE obj = TypedData_Make_Struct(klass, struct image_data, &image_data_type, image);
    image->buffer = Qnil;
    image->pixel_format = RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID;
    image->width = 0;
    image->height = 0;
    image->stride = 0;
    return obj;
}

static inline struct image_data*
get_image_data(VALUE const obj)
{
    struct image_data* image;
    TypedData_Get_Struct(obj, struct image_data, &image_data_type, image);
    return image;
}

#ifdef HAVE_RB_CAIRO_H
static cairo_format_t
pixel_format_to_cairo_format(rb_image_file_image_pixel_format_t const pf)
{
    switch (pf) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID:
	    break;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	    return CAIRO_FORMAT_ARGB32;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    return CAIRO_FORMAT_RGB24;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    return CAIRO_FORMAT_RGB16_565;

	default:
	    break;
    }
    rb_bug("unknown pixel format (%d)", pf);
    return -1;
}
#endif /* HAVE_RB_CAIRO_H */

static inline int
pixel_format_size(rb_image_file_image_pixel_format_t const pf)
{
    switch (pf) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID:
	    return -1;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    return 4;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    return 2;

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return -1;
}

static long
minimum_buffer_size(rb_image_file_image_pixel_format_t const pf, long const st, long const ht)
{
    long len = st * ht;

    assert(st > 0);
    assert(ht > 0);

    switch (pf) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID:
	    return -1;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	    return len * 4;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    return len * 4;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    return len * 2;

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return -1;
}

VALUE
rb_image_file_image_pixel_format_to_symbol(rb_image_file_image_pixel_format_t const pf)
{
    switch (pf) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID:
	    return Qnil;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	    return ID2SYM(id_ARGB32);

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    return ID2SYM(id_RGB24);

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    return ID2SYM(id_RGB16_565);

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return Qnil;
}

static inline rb_image_file_image_pixel_format_t
id_to_pixel_format(ID const id)
{
    if (id == id_ARGB32)
	return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32;

    if (id == id_RGB24)
	return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24;

    if (id == id_RGB16_565)
	return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565;

    return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID;
}

rb_image_file_image_pixel_format_t
rb_image_file_image_symbol_to_pixel_format(VALUE symbol)
{
    Check_Type(symbol, T_SYMBOL);
    return id_to_pixel_format(SYM2ID(symbol));
}

static rb_image_file_image_pixel_format_t
check_pixel_format(VALUE const pixel_format)
{
    rb_image_file_image_pixel_format_t pf;

    Check_Type(pixel_format, T_SYMBOL);
    pf = id_to_pixel_format(SYM2ID(pixel_format));
    if (RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID == pf)
	rb_raise(rb_eArgError, "unknown pixel format");

    return pf;
}

static void
process_arguments_of_image_initialize(int const argc, VALUE* const argv,
	VALUE* buffer_ptr,
	rb_image_file_image_pixel_format_t* pixel_format_ptr,
	long* width_ptr,
	long* height_ptr,
	long* stride_ptr
	)
{
    VALUE params;
    VALUE buffer = Qnil;
    VALUE pixel_format = Qnil;
    VALUE width = Qnil;
    VALUE height = Qnil;
    VALUE stride = Qnil;

    rb_image_file_image_pixel_format_t pf;
    long wd, ht, st;
    long min_len;

    ID id_pixel_format, id_data, id_width, id_height, id_row_stride;
    CONST_ID(id_data,  "data");
    CONST_ID(id_pixel_format,  "pixel_format");
    CONST_ID(id_width,  "width");
    CONST_ID(id_height, "height");
    CONST_ID(id_row_stride,  "row_stride");

    rb_scan_args(argc, argv, "01", &params);
    if (TYPE(params) == T_HASH) {
	buffer = rb_hash_lookup(params, ID2SYM(id_data));
	pixel_format = rb_hash_lookup(params, ID2SYM(id_pixel_format));
	width = rb_hash_lookup(params, ID2SYM(id_width));
	height = rb_hash_lookup(params, ID2SYM(id_height));
	stride = rb_hash_lookup(params, ID2SYM(id_row_stride));
    }

    if (!NIL_P(buffer)) {
	Check_Type(buffer, T_STRING);
	buffer = rb_str_dup(buffer);
    }

    if (NIL_P(pixel_format))
	rb_raise(rb_eArgError, "missing image pixel format");
    if (TYPE(pixel_format) == T_STRING)
	pixel_format = rb_str_intern(pixel_format);
    pf = check_pixel_format(pixel_format);

    if (NIL_P(width))
	rb_raise(rb_eArgError, "missing image width");
    wd = NUM2LONG(width);
    if (wd <= 0)
	rb_raise(rb_eArgError, "zero or negative image width");

    if (NIL_P(height))
	rb_raise(rb_eArgError, "missing image height");
    ht = NUM2LONG(height);
    if (ht <= 0)
	rb_raise(rb_eArgError, "zero or negative image height");

    if (NIL_P(stride)) {
#ifdef HAVE_RB_CAIRO_H
	st = cairo_format_stride_for_width(pixel_format_to_cairo_format(pf), (int)wd) / pixel_format_size(pf);
	stride = INT2NUM(st);
#else
	stride = width;
#endif
    }
    st = NUM2LONG(stride);
    if (st <= 0)
	rb_raise(rb_eArgError, "zero or negative image row-stride");
    else if (st < wd) {
	rb_warning("the given row-stride is less than the given image width.");
	st = wd;
    }

    min_len = minimum_buffer_size(pf, st, ht);
    if (NIL_P(buffer)) {
	buffer = rb_str_new(NULL, min_len);
    }
    else if (RSTRING_LEN(buffer) < min_len) {
	void rb_str_modify_expand(VALUE, long);
	rb_warning("the size of the given data is too short for the given size of image");
	rb_str_modify_expand(buffer, min_len - RSTRING_LEN(buffer));
    }

    *buffer_ptr = buffer;
    *pixel_format_ptr = pf;
    *width_ptr = wd;
    *height_ptr = ht;
    *stride_ptr = st;
}

static VALUE
image_initialize(int argc, VALUE* argv, VALUE obj)
{
    struct image_data* image;
    VALUE buffer;
    rb_image_file_image_pixel_format_t pf;
    long wd, ht, st;

    process_arguments_of_image_initialize(argc, argv, &buffer, &pf, &wd, &ht, &st);
    assert(!NIL_P(buffer));
    assert(pf != RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID);
    assert(wd > 0);
    assert(ht > 0);
    assert(st >= wd);

    image = get_image_data(obj);
    image->buffer = buffer;
    image->pixel_format = pf;
    image->width = wd;
    image->height = ht;
    image->stride = st;

    return obj;
}

VALUE
rb_image_file_image_get_buffer(VALUE obj)
{
    struct image_data* image = get_image_data(obj);
    return image->buffer;
}

static VALUE
image_get_pixel_format(VALUE obj)
{
    struct image_data* image = get_image_data(obj);
    return rb_image_file_image_pixel_format_to_symbol(image->pixel_format);
}

static VALUE
image_get_width(VALUE obj)
{
    struct image_data* image = get_image_data(obj);
    return LONG2NUM(image->width);
}

static VALUE
image_get_height(VALUE obj)
{
    struct image_data* image = get_image_data(obj);
    return LONG2NUM(image->height);
}

static VALUE
image_get_row_stride(VALUE obj)
{
    struct image_data* image = get_image_data(obj);
    return LONG2NUM(image->stride);
}


#ifdef HAVE_RB_CAIRO_H
static cairo_user_data_key_t const cairo_data_key = {};

static void
image_surface_did_destroyed(void* data)
{
    xfree(data);
}

static VALUE
image_create_cairo_surface(VALUE obj)
{
    cairo_surface_t* cairo_surface;
    cairo_format_t cairo_format;
    unsigned char* data;
    VALUE surface;
    struct image_data* image = get_image_data(obj);

    data = xmalloc(sizeof(unsigned char)*RSTRING_LEN(image->buffer));
    MEMCPY(data, RSTRING_PTR(image->buffer), unsigned char, RSTRING_LEN(image->buffer));

    cairo_format = pixel_format_to_cairo_format(image->pixel_format);
    cairo_surface = cairo_image_surface_create_for_data(
	    data, cairo_format, (int)image->width, (int)image->height,
	    (int)image->stride*pixel_format_size(image->pixel_format));
    cairo_surface_set_user_data(cairo_surface, &cairo_data_key, data, image_surface_did_destroyed);
    surface = CRSURFACE2RVAL_WITH_DESTROY(cairo_surface);
    return surface;
}
#endif

void
rb_image_file_Init_image_file_image(void)
{
    cImageFileImage = rb_define_class_under(mImageFile, "Image", rb_cObject);
    rb_define_alloc_func(cImageFileImage, image_alloc);
    rb_define_method(cImageFileImage, "initialize", image_initialize, -1);

    rb_define_method(cImageFileImage, "pixel_format", image_get_pixel_format, 0);
    rb_define_method(cImageFileImage, "width", image_get_width, 0);
    rb_define_method(cImageFileImage, "height", image_get_height, 0);
    rb_define_method(cImageFileImage, "row_stride", image_get_row_stride, 0);

#ifdef HAVE_RB_CAIRO_H
    rb_define_method(cImageFileImage, "create_cairo_surface", image_create_cairo_surface, 0);
#endif

    CONST_ID(id_ARGB32, "ARGB32");
    CONST_ID(id_RGB24, "RGB24");
    CONST_ID(id_RGB16_565, "RGB16_565");
}
