// SPDX-License-Identifier: GPL-2.0
/*
 * The vendor libs are linked against uClibc 0.9.33, whose ctype tables are
 * the data symbols __ctype_b and __ctype_tolower; uClibc-ng 1.0.x only has
 * the __ctype_*_loc() functions. Preload this to provide them.
 */
#include <ctype.h>

const unsigned short *__ctype_b;
const short *__ctype_tolower;
const short *__ctype_toupper;

__attribute__((constructor)) static void ctype_shim_init(void)
{
	__ctype_b = *__ctype_b_loc();
	__ctype_tolower = *__ctype_tolower_loc();
	__ctype_toupper = *__ctype_toupper_loc();
}
