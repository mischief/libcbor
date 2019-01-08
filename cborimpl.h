typedef struct cbor_coder cbor_coder;
struct cbor_coder {
	cbor_allocator *alloc;
	uchar *s, *e, *p;
};


uchar* cbor_take(cbor_coder *d, long want);
//#define cbor_take(d, want) ((d->e - d->p < want) ? nil : (d->p += want, d->p - want))
