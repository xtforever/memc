#ifndef M_CSV_H
#define M_CSV_H
#include "m_tool.h"
int csv_split(int ln);
int csv_read_stream(FILE *fp);
int csv_reader(const char *filename);
void csv_freeptr(void *d);
void csv_free(int csv );
#endif
