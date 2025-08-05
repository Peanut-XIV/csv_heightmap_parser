#ifndef __CUSTOM_DTYPES_H
#include "../include/custom_dtypes.h"
#endif

void init_ReadBufferStruct(ReadBuffer *rb, const RowLayout* row_lo, const Config* cf);

void init_CompBufferStruct(CompBuffer *cb, const RowLayout *row_lo, const Config *cf);

void init_ProcValBufferStruct(ProcValBuffer *pvb, const RowLayout *row_lo, const Config *cf);
