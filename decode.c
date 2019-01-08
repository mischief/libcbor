#include <u.h>
#include <libc.h>

#include "cbor.h"
#include "cborimpl.h"

#define OPSIZE(base, op) (1<<(-((base) - (op))))

static cbor *dec_tab(cbor_coder *d);

uchar*
cbor_take(cbor_coder *d, long want)
{
	uchar *p;

	if(d->e - d->p < want)
		return nil;

	p = d->p;
	d->p += want;
	return p;
}

static cbor*
dec_size(cbor_coder *d, int n, cbor *(*decf)(cbor_coder*, u64int))
{
	int i;
	u64int len;
	uchar *p;

	p = cbor_take(d, n);
	if(p == nil)
		return nil;

	len = 0;

	switch(n){
	case 1: case 2: case 4: case 8:
		for(i = 0; i < n; i++)
			len |= (u64int)p[i] << (n-1-i)*8;
		break;
	default:
		return nil;
	}

	return decf(d, len);
}

static cbor*
dec_u_common(cbor_coder *d, u64int len)
{
	return cbor_make_uint(d->alloc, len);
}

static cbor*
dec_ulit(cbor_coder *d)
{
	return dec_u_common(d, d->p[-1]);
}

static cbor*
dec_u(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0x18, d->p[-1]), dec_u_common);
}

static cbor*
dec_s_common(cbor_coder *d, u64int v)
{
	return cbor_make_sint(d->alloc, ~v);
}

static cbor*
dec_slit(cbor_coder *d)
{
	return dec_s_common(d, d->p[-1] - 0x20);
}

static cbor*
dec_s(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0x38, d->p[-1]), dec_s_common);
}

static cbor*
dec_b_common(cbor_coder *d, u64int len)
{
	uchar *p;

	p = cbor_take(d, len);
	if(p == nil)
		return nil;

	return cbor_make_byte(d->alloc, p, len);
}

static cbor*
dec_blit(cbor_coder *d)
{
	return dec_b_common(d, d->p[-1] - 0x40);
}

static cbor*
dec_b(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0x58, d->p[-1]), dec_b_common);
}

static cbor*
dec_string_common(cbor_coder *d, u64int len)
{
	uchar *p;

	p = cbor_take(d, len);
	if(p == nil)
		return nil;

	return cbor_make_string(d->alloc, (char*)p, len);
}

static cbor*
dec_stringlit(cbor_coder *d)
{
	return dec_string_common(d, d->p[-1] - 0x60);
}

static cbor*
dec_string(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0x78, d->p[-1]), dec_string_common);
}

static cbor*
dec_a_common(cbor_coder *d, u64int len)
{
	u64int i;

	cbor *c, *e;

	c = cbor_make_array(d->alloc, len);
	if(c == nil)
		return nil;

	for(i = 0; i < len; i++){
		e = dec_tab(d);
		if(e == nil)
			goto fail;

		c->array[i] = e;
	}

	return c;

fail:
	cbor_free(d->alloc, c);

	return nil;
}

static cbor*
dec_alit(cbor_coder *d)
{
	return dec_a_common(d, d->p[-1] - 0x80);
}

static cbor*
dec_a(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0x98, d->p[-1]), dec_a_common);
}

static cbor*
dec_m_common(cbor_coder *d, u64int len)
{
	u64int i;

	cbor *c, *e, *k, *v;

	c = cbor_make_map(d->alloc, len);
	if(c == nil)
		return nil;

	for(i = 0; i < len; i++){
		k = dec_tab(d);
		if(k == nil)
			goto fail;

		v = dec_tab(d);
		if(v == nil){
			cbor_free(d->alloc, k);
			goto fail;
		}

		e = cbor_make_map_element(d->alloc, k, v);
		if(e == nil){
			cbor_free(d->alloc, k);
			cbor_free(d->alloc, v);
			goto fail;
		}

		c->array[i] = e;
	}

	return c;

fail:
	cbor_free(d->alloc, c);

	return nil;
}

static cbor*
dec_mlit(cbor_coder *d)
{
	return dec_m_common(d, d->p[-1] - 0xa0);
}

static cbor*
dec_m(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0xb8, d->p[-1]), dec_m_common);
}

static cbor*
dec_t_common(cbor_coder *d, u64int tag)
{
	cbor *c, *e;

	e = dec_tab(d);
	if(e == nil)
		return nil;

	c = cbor_make_tag(d->alloc, tag, e);
	if(c == nil){
		cbor_free(d->alloc, e);
		return nil;
	}

	return c;
}

static cbor*
dec_tlit(cbor_coder *d)
{
	return dec_t_common(d, d->p[-1] - 0xc0);
}

static cbor*
dec_t(cbor_coder *d)
{
	return dec_size(d, OPSIZE(0xd8, d->p[-1]), dec_t_common);
}

static cbor*
dec_null(cbor_coder *d)
{
	return cbor_make_null(d->alloc);
}

static cbor*
dec_half_v(cbor_coder *d, u64int v)
{
	int exp, mant;
	double dub;

	exp = (v>>10) & 0x1f;
	mant = v & 0x3ff;

	if(exp == 0)
		dub = ldexp(mant, -24);
	else if(exp != 31)
		dub = ldexp(mant + 1024, exp - 25);
	else
		dub = mant == 0 ? Inf(0) : NaN();

	return cbor_make_double(d->alloc, (v & 0x8000) ? -dub : dub);
}

static cbor*
dec_half(cbor_coder *d)
{
	return dec_size(d, 2, dec_half_v);
}

static cbor*
dec_f_v(cbor_coder *d, u64int v)
{
	float f;
	u32int fv;

	fv = (u32int)v;
	memcpy(&f, &fv, 4);

	return cbor_make_float(d->alloc, f);
}

static cbor*
dec_f(cbor_coder *d)
{
	return dec_size(d, 4, dec_f_v);
}

static cbor*
dec_d_v(cbor_coder *d, u64int v)
{
	double dub;

	memcpy(&dub, &v, 8);

	return cbor_make_double(d->alloc, dub);
}

static cbor*
dec_d(cbor_coder *d)
{
	return dec_size(d, 8, dec_d_v);
}

static cbor *(*decfuns[256])(cbor_coder *d) = {
/* major type 0 */
[0x00]	dec_ulit,
[0x01]	dec_ulit,
[0x02]	dec_ulit,
[0x03]	dec_ulit,
[0x04]	dec_ulit,
[0x05]	dec_ulit,
[0x06]	dec_ulit,
[0x07]	dec_ulit,
[0x08]	dec_ulit,
[0x09]	dec_ulit,
[0x0a]	dec_ulit,
[0x0b]	dec_ulit,
[0x0c]	dec_ulit,
[0x0d]	dec_ulit,
[0x0e]	dec_ulit,
[0x0f]	dec_ulit,
[0x10]	dec_ulit,
[0x11]	dec_ulit,
[0x12]	dec_ulit,
[0x13]	dec_ulit,
[0x14]	dec_ulit,
[0x15]	dec_ulit,
[0x16]	dec_ulit,
[0x17]	dec_ulit,
[0x18]	dec_u,
[0x19]	dec_u,
[0x1a]	dec_u,
[0x1b]	dec_u,

/* major type 1 */
[0x20]	dec_slit,
[0x21]	dec_slit,
[0x22]	dec_slit,
[0x23]	dec_slit,
[0x24]	dec_slit,
[0x25]	dec_slit,
[0x26]	dec_slit,
[0x27]	dec_slit,
[0x28]	dec_slit,
[0x29]	dec_slit,
[0x2a]	dec_slit,
[0x2b]	dec_slit,
[0x2c]	dec_slit,
[0x2d]	dec_slit,
[0x2e]	dec_slit,
[0x2f]	dec_slit,
[0x30]	dec_slit,
[0x31]	dec_slit,
[0x32]	dec_slit,
[0x33]	dec_slit,
[0x34]	dec_slit,
[0x35]	dec_slit,
[0x36]	dec_slit,
[0x37]	dec_slit,
[0x38]	dec_s,
[0x39]	dec_s,
[0x3a]	dec_s,
[0x3b]	dec_s,

/* major type 2 */
[0x40]	dec_blit,
[0x41]	dec_blit,
[0x42]	dec_blit,
[0x43]	dec_blit,
[0x44]	dec_blit,
[0x45]	dec_blit,
[0x46]	dec_blit,
[0x47]	dec_blit,
[0x48]	dec_blit,
[0x49]	dec_blit,
[0x4a]	dec_blit,
[0x4b]	dec_blit,
[0x4c]	dec_blit,
[0x4d]	dec_blit,
[0x4e]	dec_blit,
[0x4f]	dec_blit,
[0x50]	dec_blit,
[0x51]	dec_blit,
[0x52]	dec_blit,
[0x53]	dec_blit,
[0x54]	dec_blit,
[0x55]	dec_blit,
[0x56]	dec_blit,
[0x57]	dec_blit,
[0x58]	dec_b,
[0x59]	dec_b,
[0x5a]	dec_b,
[0x5b]	dec_b,

/* major type 3 */
[0x60]	dec_stringlit,
[0x61]	dec_stringlit,
[0x62]	dec_stringlit,
[0x63]	dec_stringlit,
[0x64]	dec_stringlit,
[0x65]	dec_stringlit,
[0x66]	dec_stringlit,
[0x67]	dec_stringlit,
[0x68]	dec_stringlit,
[0x69]	dec_stringlit,
[0x6a]	dec_stringlit,
[0x6b]	dec_stringlit,
[0x6c]	dec_stringlit,
[0x6d]	dec_stringlit,
[0x6e]	dec_stringlit,
[0x6f]	dec_stringlit,
[0x70]	dec_stringlit,
[0x71]	dec_stringlit,
[0x72]	dec_stringlit,
[0x73]	dec_stringlit,
[0x74]	dec_stringlit,
[0x75]	dec_stringlit,
[0x76]	dec_stringlit,
[0x77]	dec_stringlit,
[0x78]	dec_string,
[0x79]	dec_string,
[0x7a]	dec_string,
[0x7b]	dec_string,

/* major type 4 */
[0x80]	dec_alit,
[0x81]	dec_alit,
[0x82]	dec_alit,
[0x83]	dec_alit,
[0x84]	dec_alit,
[0x85]	dec_alit,
[0x86]	dec_alit,
[0x87]	dec_alit,
[0x88]	dec_alit,
[0x89]	dec_alit,
[0x8a]	dec_alit,
[0x8b]	dec_alit,
[0x8c]	dec_alit,
[0x8d]	dec_alit,
[0x8e]	dec_alit,
[0x8f]	dec_alit,
[0x90]	dec_alit,
[0x91]	dec_alit,
[0x92]	dec_alit,
[0x93]	dec_alit,
[0x94]	dec_alit,
[0x95]	dec_alit,
[0x96]	dec_alit,
[0x97]	dec_alit,
[0x98]	dec_a,
[0x99]	dec_a,
[0x9a]	dec_a,
[0x9b]	dec_a,

/* major type 5 */
[0xa0]	dec_mlit,
[0xa1]	dec_mlit,
[0xa2]	dec_mlit,
[0xa3]	dec_mlit,
[0xa4]	dec_mlit,
[0xa5]	dec_mlit,
[0xa6]	dec_mlit,
[0xa7]	dec_mlit,
[0xa8]	dec_mlit,
[0xa9]	dec_mlit,
[0xaa]	dec_mlit,
[0xab]	dec_mlit,
[0xac]	dec_mlit,
[0xad]	dec_mlit,
[0xae]	dec_mlit,
[0xaf]	dec_mlit,
[0xb0]	dec_mlit,
[0xb1]	dec_mlit,
[0xb2]	dec_mlit,
[0xb3]	dec_mlit,
[0xb4]	dec_mlit,
[0xb5]	dec_mlit,
[0xb6]	dec_mlit,
[0xb7]	dec_mlit,
[0xb8]	dec_m,
[0xb9]	dec_m,
[0xba]	dec_m,
[0xbb]	dec_m,

/* major type 6 */
[0xc0]  dec_tlit,
[0xc1]  dec_tlit,
[0xc2]  dec_tlit,
[0xc3]  dec_tlit,
[0xc4]  dec_tlit,
[0xc5]  dec_tlit,
[0xc6]  dec_tlit,
[0xc7]  dec_tlit,
[0xc8]  dec_tlit,
[0xc9]  dec_tlit,
[0xca]  dec_tlit,
[0xcb]  dec_tlit,
[0xcc]  dec_tlit,
[0xcd]  dec_tlit,
[0xce]  dec_tlit,
[0xcf]  dec_tlit,
[0xd0]  dec_tlit,
[0xd1]  dec_tlit,
[0xd2]  dec_tlit,
[0xd3]  dec_tlit,
[0xd4]  dec_tlit,
[0xd5]  dec_tlit,
[0xd6]  dec_tlit,
[0xd7]  dec_tlit,
[0xd8]  dec_t,
[0xd9]  dec_t,
[0xda]  dec_t,
[0xdb]  dec_t,

/* major type 7 */
[0xf6]	dec_null,
[0xf9]	dec_half,
[0xfa]	dec_f,
[0xfb]	dec_d,
};

static cbor*
dec_tab(cbor_coder *d)
{
	uchar *p, op;
	cbor *(*f)(cbor_coder *d);

	p = cbor_take(d, 1);
	if(p == nil)
		return nil;

	op = *p;

	f = decfuns[op];

	if(f == nil)
		sysfatal("%hhud not implemented", op);

	assert(f != nil);

	return f(d);
}

cbor*
cbor_decode(cbor_allocator *alloc, uchar *buf, ulong n)
{
	cbor_coder d = {
		.alloc = alloc,
		.s = buf,
		.p = buf,
		.e = buf + n,
	};

	return dec_tab(&d);
}
