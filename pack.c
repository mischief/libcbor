#include <u.h>
#include <libc.h>

#include "cbor.h"

enum {
	END_ARRAY = -2,
	END_MAP = -3,
};

/*
 * u - unsigned int (u64int)
 * i - signed int (s64int)
 * b - byte array (length, uchar*)
 * s - string (length, char*)
 * [ - array start ([element, ...])
 * ] - array end ()
 * { - map start ([key, value, ...])
 * } - map end ()
 * t - tag (u64int, tagged...)
 * N - null ()
 * f - float (float)
 * d - double (double)
 * c - cbor element (cbor*)
*/
int
cbor_vpack(cbor_allocator *a, cbor **rc, char *fmt, va_list *va)
{
	char *fmtp;
	int rv;
	u64int tag;
	cbor *c, *ce, *ck, *cv;

	fmtp = fmt;

	*rc = nil;

	switch(*fmtp++){
	default:
		goto err;

	case 'u':
		c = cbor_make_uint(a, va_arg(*va, u64int));
		if(c == nil)
			return -1;
		break;

	case 'i':
		c = cbor_make_sint(a, va_arg(*va, s64int));
		if(c == nil)
			return -1;
		break;

	case 'b':
		rv = va_arg(*va, int);
		c = cbor_make_byte(a, va_arg(*va, uchar*), rv);
		if(c == nil)
			return -1;
		break;

	case 's':
		rv = va_arg(*va, int);
		c = cbor_make_string(a, va_arg(*va, char*), rv);
		if(c == nil)
			return -1;
		break;

	case '[':
		c = cbor_make_array(a, 0);
		if(c == nil)
			return -1;

		for(;;){
			rv = cbor_vpack(a, &ce, fmtp++, va);
			if(rv == END_ARRAY)
				break;

			if(rv == -1 || cbor_array_append(a, c, ce) == nil){
				cbor_free(a, c);
				return -1;
			}
		}

		break;

	case ']':
		return END_ARRAY;

	case '{':
		c = cbor_make_map(a, 0);
		if(c == nil)
			return -1;

		for(;;){
			/* expect key or end */
			rv = cbor_vpack(a, &ck, fmtp++, va);
			if(rv == END_MAP)
				break;

			if(rv == -1)
				goto map_err;

			/* expect value */
			rv = cbor_vpack(a, &cv, fmtp++, va);
			if(rv != END_MAP && rv != 0){
				cbor_free(a, ck);
				goto map_err;
			}

			if(cbor_map_append(a, c, ck, cv) == nil){
				cbor_free(a, cv);
				cbor_free(a, ck);
				goto map_err;
			}
		}

		break;

map_err:
		cbor_free(a, c);
		return -1;

	case '}':
		return END_MAP;

	case 't':
		tag = va_arg(*va, u64int);

		rv = cbor_vpack(a, &ce, fmtp, va);
		if(rv == -1)
			return -1;

		c = cbor_make_tag(a, tag, ce);
		if(c == nil){
			cbor_free(a, ce);
			return -1;
		}

		break;

	case 'N':
		c = cbor_make_null(a);
		if(c == nil)
			return -1;
		break;		

	case 'f':
		c = cbor_make_float(a, (float)va_arg(*va, double));
		if(c == nil)
			return -1;
		break;

	case 'd':
		c = cbor_make_double(a, (float)va_arg(*va, double));
		if(c == nil)
			return -1;
		break;

	case 'c':
		c = va_arg(*va, cbor*);
		break;
	}

	*rc = c;
	return 0;

err:
	sysfatal("malformed format string: %s", fmt);
	return -1;
}

cbor*
cbor_pack(cbor_allocator *a, char *fmt, ...)
{
	int rv;
	va_list va;
	cbor *c;

	va_start(va, fmt);
	rv = cbor_vpack(a, &c, fmt, &va);
	va_end(va);

	if(rv != 0)
		return nil;

	return c;
}
