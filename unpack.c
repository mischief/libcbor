#include <u.h>
#include <libc.h>

#include "cbor.h"

#define MIN(x, y)	((x) < (y) ? (x): (y))

static int cbor_vunpack(cbor_allocator *a, cbor *c, char **fmt, va_list *va);

static int
cbor_vunpack_array(cbor_allocator *a, cbor *array, char **fmt, va_list *va)
{
	char c;
	int i;
	cbor *e;

	assert(array->type == CBOR_ARRAY);

	for(i = 0; i < array->len; i++){
		e = array->array[i];
		c = **fmt;

		switch(c){
		case '\0':
			/* expected ']' */
			abort();
			goto err;
		case ']':
			return 0;
		}

		if(cbor_vunpack(a, e, fmt, va) < 0)
			goto err;
	}

	if(**fmt == ']')
		return 0;

err:
	return -1;
}

static cbor*
map_find(cbor *map, char *key)
{
	int i, klen, min;
	cbor *e;

	assert(map->type == CBOR_MAP);

	klen = strlen(key);

	for(i = 0; i < map->len; i++){
		e = map->array[i];
		assert(e->type == CBOR_MAP_ELEMENT);
		if(e->key->type != CBOR_STRING)
			continue;

		min = MIN(klen, e->key->len);

		if(strncmp(e->key->string, key, min) != 0)
			continue;

		return e->value;
	}

	return nil;
}

static int
cbor_vunpack_map(cbor_allocator *a, cbor *map, char **fmt, va_list *va)
{
	char *key;
	cbor *v;

	assert(map->type == CBOR_MAP);

	for(;;){
		switch(*(*fmt)++){
		default:
		case '\0':
			/* expected '}' */
			abort();
			goto err;
		case '}':
			return 0;
		case 'S':
			key = va_arg(*va, char*);

			v = map_find(map, key);
			break;
		}

		if(cbor_vunpack(a, v, fmt, va) < 0)
			goto err;
	}

err:
	return -1;
}

static int
cbor_vunpack(cbor_allocator *a, cbor *c, char **fmt, va_list *va)
{
	u64int *up;
	s64int *sp;
	int *lenp;
	uchar *uch, **uchp;
	char *sch, **schp;
	cbor **cp;

	assert(c != nil);

	switch(*(*fmt)++){
	default:
		break;

	case 'u':
		if(c->type != CBOR_UINT)
			break;

		up = va_arg(*va, u64int*);
		*up = c->uint;
		return 0;
	case 'i':
		sp = va_arg(*va, s64int*);

		if(cbor_int(c, sp) < 0)
			break;

		return 0;

	case 'b':
		if(c->type != CBOR_BYTE)
			break;

		lenp = va_arg(*va, int*);
		uchp = va_arg(*va, uchar**);

		uch = a->alloc(a->context, c->len);
		if(uch == nil)
			break;

		memcpy(uch, c->byte, c->len);

		*lenp = c->len;
		*uchp = uch;
		return 0;

	case 's':
		if(c->type != CBOR_STRING)
			break;

		lenp = va_arg(*va, int*);
		schp = va_arg(*va, char**);

		sch = a->alloc(a->context, c->len+1);
		if(sch == nil)
			break;

		memcpy(sch, c->string, c->len);

		sch[c->len] = '\0';

		*lenp = c->len;
		*schp = sch;
		return 0;

	case '{':
		if(c->type != CBOR_MAP)
			break;

		return cbor_vunpack_map(a, c, fmt, va);

	case '[':
		if(c->type != CBOR_ARRAY)
			break;

		return cbor_vunpack_array(a, c, fmt, va);

	case 't':
		if(c->type != CBOR_TAG)
			break;

		up = va_arg(*va, u64int*);
		*up = c->tag;

		return cbor_vunpack(a, c->item, fmt, va);

	case 'c':
		cp = va_arg(*va, cbor**);

		/* TODO: copy value? */
		*cp = c;
		return 0;
	}

	return -1;
}

int
cbor_unpack(cbor_allocator *a, cbor *c, char *fmt, ...)
{
	int rv;
	va_list va;

	va_start(va, fmt);
	rv = cbor_vunpack(a, c, &fmt, &va);
	va_end(va);

	return rv;
}
