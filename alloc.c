#include <u.h>
#include <libc.h>

#include "cbor.h"

void
cbor_free(cbor_allocator *a, cbor *c)
{
	u64int i;

	if(c == nil)
		return;

	switch(c->type){
	default:
		abort();

	case CBOR_UINT:
	case CBOR_SINT:
	case CBOR_NULL:
	case CBOR_FLOAT:
	case CBOR_DOUBLE:
		break;

	case CBOR_BYTE:
		a->free(a->context, c->byte);
		break;

	case CBOR_STRING:
		a->free(a->context, c->string);
		break;

	case CBOR_ARRAY:
	case CBOR_MAP:
		for(i = 0; i < c->len; i++)
			cbor_free(a, c->array[i]);
		a->free(a->context, c->array);
		break;

	case CBOR_MAP_ELEMENT:
		cbor_free(a, c->key);
		cbor_free(a, c->value);
		break;

	case CBOR_TAG:
		cbor_free(a, c->item);
		break;
	}

	a->free(a->context, c);
}

static void*
cbor_default_alloc(void *context, ulong size)
{
	USED(context);

	return malloc(size);
}

static void*
cbor_default_realloc(void *context, void *optr, ulong osize, ulong size)
{
	USED(context, osize);

	return realloc(optr, size);
}


static void
cbor_default_free(void *context, void *ptr)
{
	USED(context);

	free(ptr);
}

cbor_allocator cbor_default_allocator = {
	.alloc		= cbor_default_alloc,
	.realloc	= cbor_default_realloc,
	.free		= cbor_default_free,
};

cbor*
cbor_make_uint(cbor_allocator *a, u64int v)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_UINT;
	c->uint = v;

	return c;
}

cbor*
cbor_make_sint(cbor_allocator *a, s64int v)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_SINT;
	c->sint = v;

	return c;
}

static cbor*
cbor_make_bytestring(cbor_allocator *a, uchar *buf, int n, int typ)
{
	uchar *p;
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	p = a->alloc(a->context, n);
	if(p == nil){
		a->free(a->context, c);
		return nil;
	}

	memmove(p, buf, n);
	c->type = typ;
	c->len = n;
	c->byte = p;

	return c;
}

cbor*
cbor_make_byte(cbor_allocator *a, uchar *buf, int n)
{
	return cbor_make_bytestring(a, buf, n, CBOR_BYTE);
}

cbor*
cbor_make_string(cbor_allocator *a, char *buf, int n)
{
	return cbor_make_bytestring(a, (uchar*)buf, n, CBOR_STRING);
}

static cbor*
cbor_make_arraymap(cbor_allocator *a, int len, int typ)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->array = a->alloc(a->context, len * sizeof(cbor*));
	if(c->array == nil){
		a->free(a->context, c);
		return nil;
	}

	c->type = typ;
	c->len = len;

	return c;
}

cbor*
cbor_make_array(cbor_allocator *a, int len)
{
	return cbor_make_arraymap(a, len, CBOR_ARRAY);
}

cbor*
cbor_array_append(cbor_allocator *a, cbor *array, cbor *item)
{
	ulong osz, nsz;
	cbor **na;

	assert(array->type == CBOR_ARRAY);

	osz = array->len * sizeof(cbor*);
	nsz = (array->len + 1) * sizeof(cbor*);

	na = a->realloc(a->context, array->array, osz, nsz);
	if(na == nil)
		return nil;

	na[array->len] = item;

	array->len += 1;
	array->array = na;

	return array;
}

cbor*
cbor_make_map(cbor_allocator *a, int len)
{
	return cbor_make_arraymap(a, len, CBOR_MAP);
}

cbor*
cbor_make_map_element(cbor_allocator *a, cbor *k, cbor *v)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_MAP_ELEMENT;
	c->key = k;
	c->value = v;

	return c;
}

cbor*
cbor_map_append_element(cbor_allocator *a, cbor *map, cbor *elem)
{
	int i;
	cbor **na;

	assert(map->type == CBOR_MAP);
	assert(elem->type == CBOR_MAP_ELEMENT);

	na = a->alloc(a->context, (map->len+1) * sizeof(cbor*));
	if(na == nil)
		return nil;

	for(i = 0; i < map->len; i++)
		na[i] = map->array[i];

	na[i] = elem;

	a->free(a->context, map->array);

	map->len += 1;
	map->array = na;

	return map;
}

cbor*
cbor_map_append(cbor_allocator *a, cbor *map, cbor *key, cbor *value)
{
	cbor *e;

	assert(map->type == CBOR_MAP);

	e = cbor_make_map_element(a, key, value);
	if(e == nil)
		return e;

	if(cbor_map_append_element(a, map, e) == nil){
		cbor_free(a, e);
		return nil;
	}

	return map;
}

cbor*
cbor_make_tag(cbor_allocator *a, u64int tag, cbor *e)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_TAG;
	c->tag = tag;
	c->item = e;

	return c;
}

cbor*
cbor_make_null(cbor_allocator *a)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_NULL;

	return c;
}

cbor*
cbor_make_float(cbor_allocator *a, float f)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_FLOAT;
	c->f = f;

	return c;
}

cbor*
cbor_make_double(cbor_allocator *a, double d)
{
	cbor *c;

	c = a->alloc(a->context, sizeof(*c));
	if(c == nil)
		return nil;

	c->type = CBOR_DOUBLE;
	c->d = d;

	return c;
}
