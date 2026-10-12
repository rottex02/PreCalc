#ifndef BIGINT_IO_H
#define BIGINT_IO_H

#include <stdio.h>
#include "bigint.h"

/* -------------------------------------------------------------------------
 * Lossless String <-> BigInteger Conversions
 * ------------------------------------------------------------------------- */
int   bigint_from_string(BigInteger *a, const char *str);
char *bigint_to_string(const BigInteger *a);

/* -------------------------------------------------------------------------
 * String Utilities
 * ------------------------------------------------------------------------- */
void  trim_whitespace(char *str);
char *read_input_line(FILE *stream);

#endif /* BIGINT_IO_H */

