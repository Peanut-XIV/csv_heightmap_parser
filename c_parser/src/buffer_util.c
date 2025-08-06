#include <unistd.h>
#include "../include/buffer_util.h"



void init_ReadBufferStruct(ReadBuffer *rb, const RowLayout* row_lo, const Config* cf) {
	rb->bytesize = row_lo->max_size * cf->tile_height * 2 * sizeof(char);
	rb->page_bytesize = getpagesize();
	// calculate pagecount for mmap
	// (X + Y - 1) / Y For rounding up instead of down
	rb->page_count = (rb->bytesize + rb->page_bytesize - 1) / rb->page_bytesize;
	rb->start = NULL;
}

void init_CompBufferStruct(CompBuffer *cb, const RowLayout *row_lo, const Config *cf) {
	cb->row_length = row_lo->field_count;
	cb->row_count = cf->tile_height * 2;
	cb-> bytesize = cb->row_length * cb->row_count * sizeof(float);
	cb->start = NULL;
}

void init_ProcValBufferStruct(ProcValBuffer *pvb, const RowLayout *row_lo, const Config *cf) {
	pvb->row_length = row_lo->field_count >> 1;
	pvb->row_count = cf->tile_height;
	pvb->bytesize = pvb->row_count * pvb->row_length * sizeof(float);
	pvb->start = NULL;
}

void init_FullFileBuffer(
	FullFileBuffer *ff,
	size_t row_length,
	size_t row_count,
	char field_size,
	char sep_size,
	char eol_size
) {
	ff->buffer = NULL;
	ff->row_length = row_length;
	ff->row_bytesize = row_length * (field_size + sep_size) - sep_size + eol_size;
	ff->bytesize = row_count * ff->row_bytesize;
	ff->row_count = row_count;
	ff->eol_size = eol_size;
}
