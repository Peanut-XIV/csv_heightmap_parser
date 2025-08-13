#include "custom_dtypes.h"
#include <stdint.h>

void init_ReadBufferStruct(
	ReadBuffer *rb,
	const RowLayout* row_lo,
	const Config* cf
);

void init_CompBufferStruct(
	CompBuffer *cb,
	const RowLayout *row_lo,
	const Config *cf
);

void init_ProcValBufferStruct(
	ProcValBuffer *pvb,
	const RowLayout *row_lo,
	const Config *cf
);

void init_FullFileBuffer(
	FullFileBuffer *ff,
	int32_t row_length,
	int32_t row_count,
	short field_size,
	char sep_size,
	char eol_size
);
