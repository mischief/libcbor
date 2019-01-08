#include <u.h>
#include <libc.h>

#include "cbor.h"
#include "cborimpl.h"

typedef ulong (*encfun)(cbor_coder *d, cbor *c, int justsize);

static ulong cbor_enc(cbor_coder *d, cbor *c, int justsize);

static ulong
enc_size(cbor_coder *d, u64int v, uchar major, int justsize)
{
	ulong n;
	uchar *p;

	if(v < 24){
		n = 1;
		major |= v;
	} else if(v < 0x100ULL){
		n = 2;
		major |= 24;
	} else if(v < 0x10000ULL){
		n = 3;
		major |= 25;
	} else if(v < 0x100000000ULL){
		n = 5;
		major |= 26;
	} else {
		n = 9;
		major |= 27;
	}

	if(justsize)
		return n;

	p = cbor_take(d, n);
	if(p == nil)
		return 0;

	*p++ = major;

	switch(n){
	default:
		abort();
	case 9:
		*p++ = v >> 56;
		*p++ = v >> 48;
		*p++ = v >> 40;
		*p++ = v >> 32;
	case 5:
		*p++ = v >> 24;
		*p++ = v >> 16;
	case 3:
		*p++ = v >> 8;
	case 2:
		*p = v;
	case 1:
		;
	}

	return n;
}

static ulong
enc_u(cbor_coder *d, cbor *c, int justsize)
{
	return enc_size(d, c->uint, 0<<5, justsize);
}

static ulong
enc_s(cbor_coder *d, cbor *c, int justsize)
{
	return enc_size(d, ~c->sint, 1<<5, justsize);
}

static ulong
enc_data_common(cbor_coder *d, cbor *c, uchar major, int justsize)
{
	ulong rv;
	uchar *p;

	rv = enc_size(d, c->len, major, justsize);
	if(rv == 0)
		return 0;

	if(justsize)
		return rv + c->len;

	p = cbor_take(d, c->len);
	memmove(p, c->byte, c->len);

	return rv + c->len;
}

static ulong
enc_byte(cbor_coder *d, cbor *c, int justsize)
{
	return enc_data_common(d, c, 2<<5, justsize);
}

static ulong
enc_string(cbor_coder *d, cbor *c, int justsize)
{
	return enc_data_common(d, c, 3<<5, justsize);
}

static ulong
enc_array_common(cbor_coder *d, cbor *c, uchar major, int justsize)
{

	int i;
	ulong rv, r;

	rv = enc_size(d, c->len, major, justsize);
	if(rv == 0)
		return 0;

	for(i = 0; i < c->len; i++){
		r = cbor_enc(d, c->array[i], justsize);
		if(r == 0)
			return 0;
		rv += r;
	}

	return rv;
}

static ulong
enc_a(cbor_coder *d, cbor *c, int justsize)
{
	return enc_array_common(d, c, 4<<5, justsize);
}

static ulong
enc_m(cbor_coder *d, cbor *c, int justsize)
{
	return enc_array_common(d, c, 5<<5, justsize);
}

static ulong
enc_m_e(cbor_coder *d, cbor *c, int justsize)
{
	ulong k, v;

	k = cbor_enc(d, c->key, justsize);
	if(k == 0)
		return 0;

	v = cbor_enc(d, c->value, justsize);
	if(v == 0)
		return 0;

	return k + v;
}

static ulong
enc_t(cbor_coder *d, cbor *c, int justsize)
{
	ulong t, e;

	t = enc_size(d, c->tag, 6<<5, justsize);
	if(t == 0)
		return 0;

	e = cbor_enc(d, c->item, justsize);
	if(t == 0)
		return 0;

	return t + e;
}

static ulong
enc_null(cbor_coder *d, cbor *c, int justsize)
{
	uchar *p;

	if(justsize)
		return 1;

	p = cbor_take(d, 1);
	if(p == nil)
		return 0;

	*p = 0xf6;

	return 1;
}

static ulong
enc_f(cbor_coder *d, cbor *c, int justsize)
{
	u32int v;
	uchar *p;

	if(justsize)
		return 5;

	p = cbor_take(d, 5);
	if(p == nil)
		return 0;

	*p++ = 0xfa;

	memcpy(&v, &c->f, 4);
	*p++ = v >> 24;
	*p++ = v >> 16;
	*p++ = v >> 8;
	*p = v;

	return 5;
}

static ulong
enc_d(cbor_coder *d, cbor *c, int justsize)
{
	u64int v;
	uchar *p;

	if(justsize)
		return 9;

	p = cbor_take(d, 9);
	if(p == nil)
		return 0;

	*p++ = 0xfb;

	memcpy(&v, &c->f, 8);
	*p++ = v >> 56;
	*p++ = v >> 48;
	*p++ = v >> 40;
	*p++ = v >> 32;
	*p++ = v >> 24;
	*p++ = v >> 16;
	*p++ = v >> 8;
	*p = v;

	return 9;
}

static encfun encfuns[] = {
[CBOR_UINT]			enc_u,
[CBOR_SINT]			enc_s,
[CBOR_BYTE]			enc_byte,
[CBOR_STRING]		enc_string,
[CBOR_ARRAY]		enc_a,
[CBOR_MAP]			enc_m,
[CBOR_MAP_ELEMENT]	enc_m_e,
[CBOR_TAG]			enc_t,
[CBOR_NULL]			enc_null,
[CBOR_FLOAT]		enc_f,
[CBOR_DOUBLE]		enc_d,
};

static ulong
cbor_enc(cbor_coder *d, cbor *c, int justsize)
{
	encfun f;

	assert(c->type < CBOR_TYPE_MAX);

	f = encfuns[c->type];
	assert(f != nil);

	return f(d, c, justsize);
}

ulong
cbor_encode(cbor *c, unsigned char *buf, ulong n)
{
	cbor_coder d = {
		.alloc = nil,
		.s = buf,
		.p = buf,
		.e = buf + n,
	};

	return cbor_enc(&d, c, 0);
}

ulong
cbor_encode_size(cbor *c)
{
	return cbor_enc(nil, c, 1);
}