#pragma lib	"libcbor.a"

enum {
	CBOR_UINT = 0,
	CBOR_NINT,
	CBOR_BYTE,
	CBOR_STRING,
	CBOR_ARRAY,
	CBOR_MAP,
	CBOR_MAP_ELEMENT,
	CBOR_TAG,
	CBOR_NULL,
	CBOR_FLOAT,
	CBOR_DOUBLE,

	CBOR_TYPE_MAX,

	CBOR_TAG_DATETIME	= 0,
	CBOR_TAG_UNIXTIME	= 1,
	CBOR_TAG_CBOR		= 55799ULL,
};

typedef struct cbor cbor;
struct cbor
{
	uchar	type;

	union {
		/* CBOR_UINT */
		/* CBOR_NINT */
		u64int	uint;

		struct {
			int len;

			union {
				/* CBOR_BYTE */
				uchar*	byte;

				/* CBOR_STRING */
				char*	string;

				/* CBOR_ARRAY / CBOR_MAP */
				cbor**	array;
			};
		};

		/* CBOR_MAP_ELEMENT */
		struct {
			cbor*	key;
			cbor*	value;
		};

		/* CBOR_TAG */
		struct {
			u64int	tag;
			cbor*	item;
		};

		/* CBOR_FLOAT */
		float	f;

		/* CBOR_DOUBLE */
		double	d;
	};
};

typedef struct cbor_allocator cbor_allocator;
struct cbor_allocator {
	void*	(*alloc)(void*, ulong);
	void*	(*realloc)(void*, void*, ulong, ulong);
	void	(*free)(void*, void*);
	void*	context;
};

extern cbor_allocator cbor_default_allocator;

cbor*	cbor_make_uint(cbor_allocator *a, u64int v);
cbor*	cbor_make_nint(cbor_allocator *a, s64int v);
cbor*	cbor_make_int(cbor_allocator *a, s64int v);
cbor*	cbor_make_byte(cbor_allocator *a, uchar *buf, int n);
cbor*	cbor_make_string(cbor_allocator *a, char *buf, int n);
cbor*	cbor_make_array(cbor_allocator *a, int len);
cbor*	cbor_array_append(cbor_allocator *a, cbor *array, cbor *item);
cbor*	cbor_make_map(cbor_allocator *a, int len);
cbor*	cbor_make_map_element(cbor_allocator *a, cbor *k, cbor *v);
cbor*	cbor_map_append_element(cbor_allocator *a, cbor *map, cbor *elem);
cbor*	cbor_map_append(cbor_allocator *a, cbor *map, cbor *key, cbor *value);
cbor*	cbor_make_tag(cbor_allocator *a, u64int tag, cbor *e);
cbor*	cbor_make_null(cbor_allocator *a);
cbor*	cbor_make_float(cbor_allocator *a, float f);
cbor*	cbor_make_double(cbor_allocator *a, double d);

int		cbor_int(cbor *c, s64int *v);

void	cbor_free(cbor_allocator *a, cbor *c);
cbor*	cbor_decode(cbor_allocator *alloc, uchar *buf, ulong n);
ulong	cbor_encode(cbor *c, uchar *buf, ulong n);
ulong	cbor_encode_size(cbor *c);

cbor*	cbor_pack(cbor_allocator *a, char *fmt, ...);
int		cbor_unpack(cbor_allocator *a, cbor *c, char *fmt, ...);
