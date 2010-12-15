#include "internal.h"

#undef EXTERN
#include <jpeglib.h>
#include <jerror.h>

static size_t const INPUT_BUFFER_SIZE = 4096U;

VALUE cImageFileJpegReader = Qnil;
VALUE eImageFileJpegReaderError = Qnil;

static ID id_GRAYSCALE;
static ID id_RGB;
static ID id_YCbCr;
static ID id_CMYK;
static ID id_YCCK;

static VALUE
color_space_to_symbol(J_COLOR_SPACE const color_space)
{
    switch (color_space) {
	case JCS_UNKNOWN:
	    return Qnil;

	case JCS_GRAYSCALE:
	    return ID2SYM(id_GRAYSCALE);

	case JCS_RGB:
	    return ID2SYM(id_RGB);

	case JCS_YCbCr:
	    return ID2SYM(id_YCbCr);

	case JCS_CMYK:
	    return ID2SYM(id_CMYK);

	case JCS_YCCK:
	    return ID2SYM(id_YCCK);

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return Qnil;
}

static J_COLOR_SPACE
id_to_color_space(ID const id)
{
    if (id == id_GRAYSCALE)
	return JCS_GRAYSCALE;
    if (id == id_RGB)
	return JCS_RGB;
    if (id == id_YCbCr)
	return JCS_YCbCr;
    if (id == id_CMYK)
	return JCS_CMYK;
    if (id == id_YCCK)
	return JCS_YCCK;
    return JCS_UNKNOWN;
}

struct jpeg_reader_data {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr error;
    VALUE source;
    VALUE buffer;
    unsigned read_header: 1;
    unsigned close_source: 1;
    unsigned start_of_file: 1;
};

static void
jpeg_reader_mark(void* ptr)
{
    struct jpeg_reader_data* reader = (struct jpeg_reader_data*)ptr;
    rb_gc_mark(reader->source);
    rb_gc_mark(reader->buffer);
}

static void
jpeg_reader_free(void* ptr)
{
    struct jpeg_reader_data* reader = (struct jpeg_reader_data*)ptr;

    ID id_close;
    CONST_ID(id_close, "close");

#if 0
    if (reader->close_source && rb_respond_to(reader->source, id_close)) {
	rb_funcall(reader->source, id_close, 0);
    }
#endif
    reader->source = Qnil;
    jpeg_destroy_decompress(&reader->cinfo);
    xfree(ptr);
}

static size_t
jpeg_reader_memsize(void const* ptr)
{
    return ptr ? sizeof(struct jpeg_reader_data) : 0;
}

static rb_data_type_t const jpeg_reader_data_type = {
    "image_file::jpeg_reader",
#if RUBY_VERSION >= 193
    {
#endif
	jpeg_reader_mark,
	jpeg_reader_free,
	jpeg_reader_memsize,
#if RUBY_VERSION >= 193
    },
#endif
};

static VALUE
jpeg_reader_alloc(VALUE klass)
{
    struct jpeg_reader_data* reader;
    VALUE obj = TypedData_Make_Struct(
	    klass, struct jpeg_reader_data, &jpeg_reader_data_type, reader);
    reader->source = Qnil;
    reader->buffer = Qnil;
    reader->close_source = 0;
    reader->read_header = 0;
    return obj;
}

static struct jpeg_reader_data*
get_jpeg_reader_data(VALUE obj)
{
    struct jpeg_reader_data* reader;
    TypedData_Get_Struct(obj, struct jpeg_reader_data, &jpeg_reader_data_type, reader);
    return reader;
}

static void
error_exit(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX];
    (* cinfo->err->format_message)(cinfo, message);
    rb_raise(eImageFileJpegReaderError, message);
}

static void
output_message(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX];
    (* cinfo->err->format_message)(cinfo, message);
    rb_warning(message);
}

static struct jpeg_error_mgr*
init_error_mgr(struct jpeg_error_mgr* err)
{
    jpeg_std_error(err);
    err->error_exit = error_exit;
    err->output_message = output_message;
    return err;
}

static void
init_source(j_decompress_ptr cinfo)
{
    VALUE obj;
    struct jpeg_reader_data* reader;

    assert(cinfo != NULL);

    obj = (VALUE)cinfo->client_data;
    reader = get_jpeg_reader_data(obj);
    reader->start_of_file = 1;
}

static boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
    VALUE obj, rv;
    struct jpeg_reader_data* reader;
    ID id_read;

    assert(cinfo != NULL);

    obj = (VALUE)cinfo->client_data;
    reader = get_jpeg_reader_data(obj);

    CONST_ID(id_read, "read");
    reader->buffer = rb_funcall(reader->source, id_read, 1, INT2FIX(INPUT_BUFFER_SIZE));
    if (NIL_P(reader->buffer)) { /* EOF */
	if (reader->start_of_file) { /* empty file */
	    ERREXIT(cinfo, JERR_INPUT_EMPTY);
	}
	WARNMS(cinfo, JWRN_JPEG_EOF);
	/* Insert fake EOI marker */
	reader->buffer = rb_str_tmp_new(2);
	RSTRING_PTR(reader->buffer)[0] = (JOCTET) 0xFF;
	RSTRING_PTR(reader->buffer)[1] = (JOCTET) JPEG_EOI;
    }

    cinfo->src->next_input_byte = RSTRING_PTR(reader->buffer);
    cinfo->src->bytes_in_buffer = RSTRING_LEN(reader->buffer);
    reader->start_of_file = 0;

    return TRUE;
}

static void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    if (num_bytes > 0) {
	while (num_bytes > (long)cinfo->src->bytes_in_buffer) {
	    num_bytes -= (long)cinfo->src->bytes_in_buffer;
	    (* cinfo->src->fill_input_buffer)(cinfo);
	}
	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
    }
}

void
term_source(j_decompress_ptr cinfo)
{
    /* nothing to do */
}

static void
init_source_mgr(struct jpeg_reader_data* reader)
{
    struct jpeg_source_mgr* src = NULL;

    assert(reader != NULL);

    if (reader->cinfo.src == NULL) {
	reader->cinfo.src = (struct jpeg_source_mgr*)
	    (* reader->cinfo.mem->alloc_small)(
		    (j_common_ptr)&reader->cinfo,
		    JPOOL_PERMANENT,
		    sizeof(struct jpeg_source_mgr));
    }

    src = (struct jpeg_source_mgr*)reader->cinfo.src;
    src->init_source = init_source;
    src->fill_input_buffer = fill_input_buffer;
    src->skip_input_data = skip_input_data;
    src->resync_to_restart = jpeg_resync_to_restart;
    src->term_source = term_source;
    src->bytes_in_buffer = 0;
    src->next_input_byte = NULL;
}

static VALUE
jpeg_reader_initialize(VALUE obj, VALUE source)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    reader->cinfo.err = init_error_mgr(&reader->error);
    jpeg_create_decompress(&reader->cinfo);
    init_source_mgr(reader);
    reader->source = source;
    reader->cinfo.client_data = (void*)obj;
    return obj;
}

static VALUE
jpeg_reader_s_open(VALUE klass, VALUE path)
{
    struct jpeg_reader_data* reader;
    VALUE obj, io;

    ID id_new;
    CONST_ID(id_new, "new");

    io = rb_file_open_str(path, "rb");
    obj = rb_funcall(klass, id_new, 1, io);
    reader = get_jpeg_reader_data(obj);
    reader->close_source = 1;

    return obj;
}

static VALUE
jpeg_reader_source_will_be_closed(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    return reader->close_source ? Qtrue : Qfalse;
}

static inline void
read_header(struct jpeg_reader_data* reader)
{
    assert(reader != NULL);
    if (!reader->read_header) {
	jpeg_read_header(&reader->cinfo, TRUE);
	reader->read_header = 1;
    }
}

static VALUE
jpeg_reader_get_image_width(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return ULONG2NUM(reader->cinfo.image_width);
}

static VALUE
jpeg_reader_get_image_height(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return ULONG2NUM(reader->cinfo.image_height);
}

static VALUE
jpeg_reader_get_num_components(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return LONG2NUM(reader->cinfo.num_components);
}

static VALUE
jpeg_reader_get_jpeg_color_space(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return color_space_to_symbol(reader->cinfo.jpeg_color_space);
}

void
rb_image_file_Init_image_file_jpeg_reader(void)
{
    cImageFileJpegReader = rb_define_class_under(mImageFile, "JpegReader", rb_cObject);
    rb_define_alloc_func(cImageFileJpegReader, jpeg_reader_alloc);
    rb_define_singleton_method(cImageFileJpegReader, "open", jpeg_reader_s_open, 1);
    rb_define_method(cImageFileJpegReader, "initialize", jpeg_reader_initialize, 1);
    rb_define_method(cImageFileJpegReader, "source_will_be_closed?", jpeg_reader_source_will_be_closed, 0);

    rb_define_method(cImageFileJpegReader, "image_width", jpeg_reader_get_image_width, 0);
    rb_define_method(cImageFileJpegReader, "image_height", jpeg_reader_get_image_height, 0);
    rb_define_method(cImageFileJpegReader, "num_components", jpeg_reader_get_num_components, 0);
    rb_define_method(cImageFileJpegReader, "jpeg_color_space", jpeg_reader_get_jpeg_color_space, 0);

    eImageFileJpegReaderError = rb_define_class_under(
	    cImageFileJpegReader, "Error", rb_eStandardError);

    CONST_ID(id_GRAYSCALE, "GRAYSCALE");
    CONST_ID(id_RGB, "RGB");
    CONST_ID(id_YCbCr, "YCbCr");
    CONST_ID(id_CMYK, "CMYK");
    CONST_ID(id_YCCK, "YCCK");
}
