#include <u.h>
#include <libc.h>
#include <bin.h>

#include "cbor.h"

static void*
cbor_alloc_bin(void *context, ulong size)
{
	return binalloc((Bin**)context, size, 0);
}

static void*
cbor_realloc_bin(void *context, void *optr, ulong osize, ulong size)
{
	return bingrow((Bin**)context, optr, osize, size, 0);
}

static void
cbor_free_bin(void *context, void *ptr)
{
	USED(context);
	USED(ptr);

	return;
}

static char*
cbor_print(cbor *c, char *bp, char *be)
{
	int i;
	char *p, *e, *bracket;

	switch(c->type){
	case CBOR_UINT:
		return seprint(bp, be, "%llud", c->uint);

	case CBOR_NINT:
		return seprint(bp, be, "-%llud", c->uint+1);

	case CBOR_BYTE:
		return seprint(bp, be, "%.*H", c->len, c->byte);

	case CBOR_STRING:
		return seprint(bp, be, "\"%.*s\"", c->len, c->string);

	case CBOR_ARRAY:
		bracket = "[]";
		goto arr;

	case CBOR_MAP:
		bracket = "{}";

arr:
		p = bp;
		e = be;

		p = seprint(p, e, "%c", bracket[0]);

		for(i = 0; i < c->len; i++){
			p = cbor_print(c->array[i], p, e);
			if(c->len > 1 && i < c->len - 1)
				p = seprint(p, e, ", ");
		}

		p = seprint(p, e, "%c", bracket[1]);
		return p;

	case CBOR_MAP_ELEMENT:
		p = bp;
		e = be;

		p = cbor_print(c->key, p, e);
		p = seprint(p, e, ": ");
		p = cbor_print(c->value, p, e);
		return p;

	case CBOR_TAG:
		p = bp;
		e = be;

		p = seprint(p, e, "%llud(", c->tag);
		p = cbor_print(c->item, p, e);
		p = seprint(p, e, ")");
		return p;

	case CBOR_NULL:
		return seprint(bp, be, "null");

	case CBOR_FLOAT:
		return seprint(bp, be, "%#g", c->f);

	case CBOR_DOUBLE:
		return seprint(bp, be, "%#g", c->d);

	default:
		abort();
	}

	return nil;
}

void
cbor_fprint(int fd, cbor *c)
{
	char buf[2048];

	cbor_print(c, buf, buf+sizeof(buf));

	fprint(fd, "%s\n", buf);
}

static char *tests[] = {
	"0x00",
	"0x01",
	"0x0a",
	"0x17",
	"0x1818",
	"0x1819",
	"0x1864",
	"0x1903e8",
	"0x1a000f4240",
	"0x1b000000e8d4a51000",
	"0x1bffffffffffffffff",
	//"0xc249010000000000000000",
	"0x3b0633275e3af7fffd",
	"0x3bffffffffffffffff",
	//"0xc349010000000000000000",
	"0x20",
	"0x29",
	"0x3863",
	"0x3903e7",

/* single precision float */
	"0xfa47c35000",
	"0xfa7f7fffff",
	"0xfa7f800000",
	"0xfa7fc00000",
	"0xfaff800000",

/* double precision float */
	"0xfb3ff199999999999a",
	"0xfb7e37e43c8800759c",
	"0xfbc010666666666666",
	"0xfb7ff0000000000000",
	"0xfb7ff8000000000000",
	"0xfbfff0000000000000",

/* byte literal */
	"0x4401020304",

/* byte 1 */
	"5818736c696768746c79206c6f6e676572207468616e20323421",

/* byte 2 */
	//"590018736c696768746c79206c6f6e676572207468616e20323421",

/* byte 4 */
	//"0x5a00000018736c696768746c79206c6f6e676572207468616e20323421",

/* byte 8 */
	//"0x5b000000000000000401020304",

/* string literal */
	"0x6449455446",

/* string 1 */
	"7818736c696768746c79206c6f6e676572207468616e20323421",

/* string 2 */
	//"0x79000449455446",

/* string 4 */
	//"0x7a0000000449455446",

/* string 8 */
	//"0x7b000000000000000449455446",

/* array literal */
	"0x83010203",

/* nested array literal */
	"0x8301820203820405",

/* array 1 */
	"0x9818010203040506070801020304050607080102030405060708",

/* array 2 */
	//"0x990003010203",

/* array 4 */
	//"0x9a00000003010203",

/* array 8 */
	//"0x9b0000000000000003010203",

/* map literal */
	"0xa201020304",

/* map 1 */
	//"0xb81801020304",

/* map 2 */
	//"0xb9000201020304",

/* map 4 */
	//"0xba0000000201020304",

/* map 8 */
	//"0xbb000000000000000201020304",

/* map literal with nested array literal */
	"0xa26161016162820203",

/* various tags */
	"0xc074323031332d30332d32315432303a30343a30305a",
	"0xc11a514b67b0",
	"0xc1fb41d452d9ec200000",
	"0xd74401020304",
	"0xd818456449455446",
	"0xd82076687474703a2f2f7777772e6578616d706c652e636f6d",
};

static void
test_decenc(void)
{
	int j, i, n, np, rv;
	ulong sz, ne;
	char *p, pr[512];
	uchar buf[512];
	Bin *bin;
	cbor *c;

	cbor_allocator cbor_bin_allocator = {
		.alloc		= cbor_alloc_bin,
		.realloc	= cbor_realloc_bin,
		.free		= cbor_free_bin,
		.context	= &bin,
	};

	for(j = 0; j < 1; j++)
	for(i = 0; i < nelem(tests); i++){
		p = tests[i];
		if(strncmp(p, "0x", 2) == 0)
			p += 2;
		n = strlen(p);
		fprint(2, "test %d: %d %s\n", i, n, p);
		rv = dec16(buf, sizeof(buf), p, n);
		assert(rv != -1);
		c = cbor_decode(&cbor_bin_allocator, buf, rv);
		if(c == nil)
			sysfatal("cbor_decode: %r");

		cbor_fprint(2, c);
		//fprint(1, "%-032s	%-032s\n", pr, tests[i]);

		sz = cbor_encode_size(c);

		ne = cbor_encode(c, buf, sz);
		if(ne != sz)
			fprint(2, "expected size: %lud: got: %lud\n", sz, ne);
		assert(ne == sz);
		np = snprint(pr, sizeof(pr), "%.*lH", (int)ne, buf);

		if(strncmp(pr, p, n) != 0){
			fprint(2, "error:   %02d %s\n", np, pr);
			sysfatal("test %d unexpected encoding", i);
		}
	}

	binfree(&bin);

	//sleep(100000);
}

static void
test_encoder(void)
{
	ulong n;
	uchar buf[512];
	cbor c;

	c.type = CBOR_UINT;
	c.uint = 1000000000000ULL;

	n = cbor_encode(&c, buf, sizeof(buf));
	fprint(2, "%ld 0x%.*lH\n", n, (int)n, buf);
}

static void
test_array(void)
{
	char pr[512];
	cbor *a, *u;

	a = cbor_make_array(&cbor_default_allocator, 0);
	u = cbor_make_uint(&cbor_default_allocator, 42);

	a = cbor_array_append(&cbor_default_allocator, a, u);

	cbor_print(a, pr, pr+sizeof(pr));
	fprint(2, "append: %s\n", pr);

	cbor_free(&cbor_default_allocator, a);
}

static void
test_pack(void)
{
	int rv, glen;
	char pr[512], *greet;
	cbor *c;

	u64int uv = (1ULL<<63ULL) + 42;
	s64int svneg = -(1LL<<62LL) + 42;
	s64int svpos = (1LL<<62LL) + 42;

	c = cbor_pack(&cbor_default_allocator, "{sfsusssusisi}",
		2, "pi",3.14,
		6,"answer",42,
		8,"greeting",5,"hello",
		8,"unsigned",uv,
		3,"neg",svneg,
		3,"pos",svpos
	);
	assert(c != nil);

	cbor_print(c, pr, pr+sizeof(pr));
	fprint(2, "pack: %s\n", pr);

	u64int ruv;
	s64int rsvneg, rsvpos;

	rv = cbor_unpack(&cbor_default_allocator, c, "{SsSuSiSi}",
		"greeting", &glen, &greet,
		"unsigned", &ruv,
		"neg", &rsvneg,
		"pos", &rsvpos
	);

	assert(rv == 0);
	rv = strcmp(greet, "hello");
	assert(rv == 0);
	cbor_default_allocator.free(cbor_default_allocator.context, greet);

	assert(uv == ruv);
	assert(svneg == rsvneg);
	assert(svpos == rsvpos);

	cbor_free(&cbor_default_allocator, c);
}

static void
test_ints(void)
{
	s64int i, v;
	int rv;
	uchar buf[128];
	cbor *cbo;

	for(i = (2LL<<62LL)*-1; i < 2LL<<62LL-1; i += (2LL<<56LL)+3){
		cbo = cbor_make_int(&cbor_default_allocator, i);
		rv = cbor_encode(cbo, buf, sizeof(buf));
		assert(rv >= 0);
		cbor_free(&cbor_default_allocator, cbo);

		cbo = cbor_decode(&cbor_default_allocator, buf, rv);
		assert(cbo != nil);

		assert(cbor_int(cbo, &v) >= 0);
		assert(v == i);

		cbor_free(&cbor_default_allocator, cbo);
	}
}

static void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	fmtinstall('H', encodefmt);

	test_decenc();
	test_array();
	test_pack();
	test_ints();

	exits(nil);
}
