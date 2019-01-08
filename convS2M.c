#include <u.h>
#include <libc.h>
#include <fcall.h>

#include "cbor.h"

static char*
cbor_print(cbor *c, char *bp, char *be)
{
	int i;
	char *p, *e, *bracket;

	switch(c->type){
	case CBOR_UINT:
		return seprint(bp, be, "%llud", c->uint);

	case CBOR_SINT:
		return seprint(bp, be, "%lld", c->sint);

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

static void
cbor_fprint(int fd, cbor *c)
{
	char buf[2048];

	cbor_print(c, buf, buf+sizeof(buf));

	fprint(fd, "%s\n", buf);
}

ulong
convS2Mcbor(Fcall *f, uchar *ap, uint nap)
{
	int i, n, nn;
	ulong sz;
	cbor *o, *c, *a;

	switch(f->type){
	default:
		abort();

	case Tversion:
		n = strlen(f->version);
		o = cbor_pack(&cbor_default_allocator, "[us]", (u64int)f->msize, n, f->version);
		break;

	case Tflush:
		o = cbor_pack(&cbor_default_allocator, "u", (u64int)f->oldtag);
		break;

	case Tauth:
		n = strlen(f->uname);
		nn = strlen(f->aname);
		o = cbor_pack(&cbor_default_allocator, "[uss]", (u64int)f->afid, n, f->uname, nn, f->aname);
		break;

	case Tattach:
		n = strlen(f->uname);
		nn = strlen(f->aname);
		o = cbor_pack(&cbor_default_allocator, "[uuss]", (u64int)f->fid, (u64int)f->afid, n, f->uname, nn, f->aname);
		break;

	case Twalk:
		a = cbor_make_array(&cbor_default_allocator, 0);
		assert(a != nil);
		for(i = 0; i < f->nwname; i++){
			o = cbor_make_string(&cbor_default_allocator, f->wname[i], strlen(f->wname[i]));
			assert(o != nil);
			assert(cbor_array_append(&cbor_default_allocator, a, o) != nil);
		}

		o = cbor_pack(&cbor_default_allocator, "[uuc]", (u64int)f->fid, (u64int)f->newfid, a);
		break;

	case Topen:
		o = cbor_pack(&cbor_default_allocator, "[uu]", (u64int)f->fid, f->mode);
		break;

	case Tcreate:
		n = strlen(f->name);
		o = cbor_pack(&cbor_default_allocator, "[usuu]", (u64int)f->fid, n, f->name, f->perm, f->mode);
		break;

	case Tread:
		o = cbor_pack(&cbor_default_allocator, "[uiu]", (u64int)f->fid, (s64int)f->offset, f->count);
		break;

	case Twrite:
		o = cbor_pack(&cbor_default_allocator, "[uib]", (u64int)f->fid, (s64int)f->offset, f->count, f->data);
		break;

	case Tclunk:
	case Tremove:
	case Tstat:
		o = cbor_pack(&cbor_default_allocator, "u", (u64int)f->fid);
		break;

	case Twstat:
		o = cbor_pack(&cbor_default_allocator, "[ub]", (u64int)f->fid, f->nstat, f->stat);
		break;

	case Rversion:
		n = strlen(f->version);
		o = cbor_pack(&cbor_default_allocator, "[us]", (u64int)f->msize, n, f->version);
		break;

	case Rerror:
		n = strlen(f->ename);
		o = cbor_pack(&cbor_default_allocator, "s", n, f->ename);
		break;

	case Rflush:
		o = cbor_pack(&cbor_default_allocator, "N");
		break;

	case Rauth:
		o = cbor_pack(&cbor_default_allocator, "[uuu]",
			(u64int)f->aqid.type, (u64int)f->aqid.vers, (u64int)f->aqid.path);
		break;

	case Rattach:
		o = cbor_pack(&cbor_default_allocator, "[uuu]",
			(u64int)f->qid.type, (u64int)f->qid.vers, (u64int)f->qid.path);
		break;

	case Rwalk:
		a = cbor_make_array(&cbor_default_allocator, 0);
		assert(a != nil);
		for(i = 0; i < f->nwqid; i++){
			o = cbor_pack(&cbor_default_allocator, "[uuu]",
				(u64int)f->wqid[i].type, (u64int)f->wqid[i].vers, (u64int)f->wqid[i].path);
			assert(o != nil);
			assert(cbor_array_append(&cbor_default_allocator, a, o) != nil);
		}

		o = a;

		break;

	case Ropen:
	case Rcreate:
		o = cbor_pack(&cbor_default_allocator, "[uuuu]",
			(u64int)f->qid.type, (u64int)f->qid.vers, (u64int)f->qid.path, (u64int)f->iounit);
		break;

	case Rread:
		o = cbor_pack(&cbor_default_allocator, "b", f->count, f->data);
		break;

	case Rwrite:
		o = cbor_pack(&cbor_default_allocator, "u", (u64int)f->count);
		break;

	case Rclunk:
	case Rremove:
	case Rwstat:
		o = cbor_pack(&cbor_default_allocator, "N");
		break;

	case Rstat:
		o = cbor_pack(&cbor_default_allocator, "b", f->nstat, f->stat);
		break;
	}

	assert(o != nil);

	c = cbor_pack(&cbor_default_allocator, "t[uc]", (u64int)f->type, (u64int)f->tag, o);

	assert(c != nil);

	sz = cbor_encode_size(c);

	if(nap >= sz){
		assert(cbor_encode(c, ap, nap) == sz);
	}

	cbor_free(&cbor_default_allocator, c);

	return sz;
}

uint
convM2Scbor(uchar *ap, uint nap, Fcall *f)
{
	int len, len2;
	u64int type, tag, msize, afid;
	char *s, *s2;
	cbor *c, *v;

	c = cbor_decode(&cbor_default_allocator, ap, nap);
	if(c == nil){
		fprint(2, "decode M2S failed\n");
		return 0;
	}

	if(cbor_unpack(&cbor_default_allocator, c, "t[uc]", &type, &tag, &v) < 0){
		fprint(2, "unpack type/tag failed\n");
		goto err;
	}

	f->type = type;
	f->tag = tag;

	switch(f->type){
	default:
		goto err;

	case Rversion:
	case Tversion:
		if(cbor_unpack(&cbor_default_allocator, v, "[us]", &msize, &len, &s) < 0){
			fprint(2, "unpack Tversion failed\n");
			goto err;
		}

		f->msize = msize;
		f->version = s;
		break;

	case Tauth:
		if(cbor_unpack(&cbor_default_allocator, v, "[uss]", &afid, &len, &s, &len2, &s2) < 0)
			goto err;

		f->afid = afid;
		f->uname = s;
		f->aname = s2;
		break;

	}

	return 1;

err:
	cbor_free(&cbor_default_allocator, c);
	return 0;
}

static Fcall fcalls[17];

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, sz;
	uchar buf[1024];
	Fcall *f, f2;;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	fmtinstall('H', encodefmt);
	fmtinstall('F', fcallfmt);

	f = &fcalls[0];
	f->type = Tversion;
	f->tag = 100;
	f->msize = 8192+IOHDRSZ;
	f->version = "9P2000";

	f = &fcalls[1];
	f->type = Rversion;
	f->tag = 100;
	f->msize = 8192+IOHDRSZ;
	f->version = "9P2000";

	f = &fcalls[2];
	f->type = Tauth;
	f->tag = 100;
	f->afid = 1;
	f->uname = "glenda";
	f->aname = "";

	f = &fcalls[3];
	f->type = Rauth;
	f->tag = 100;
	f->aqid = (Qid){0, 0, QTAUTH};

	f = &fcalls[4];
	f->type = Tattach;
	f->tag = 100;
	f->fid = 200;
	f->afid = 1;
	f->uname = "glenda";
	f->aname = "";

	f = &fcalls[5];
	f->type = Rattach;
	f->tag = 100;
	f->qid = (Qid){1, 1, QTDIR};

	f = &fcalls[6];
	f->type = Rerror;
	f->tag = 100;
	f->ename = "some kind of horrible error";

	f = &fcalls[7];
	f->type = Tflush;
	f->tag = 100;
	f->oldtag = 99;

	f = &fcalls[8];
	f->type = Rflush;
	f->tag = 100;

	f = &fcalls[9];
	f->type = Twalk;
	f->tag = 100;
	f->fid = 200;
	f->newfid = 201;
	f->nwname = 4;
	f->wname[0] = "foo";
	f->wname[1] = "bar";
	f->wname[2] = "baz";
	f->wname[3] = "quux";

	f = &fcalls[10];
	f->type = Rwalk;
	f->tag = 100;
	f->nwqid = 4;
	f->wqid[0] = (Qid){2, 1, QTDIR};
	f->wqid[1] = (Qid){3, 1, QTDIR};
	f->wqid[2] = (Qid){4, 1, QTDIR};
	f->wqid[3] = (Qid){5, 1, QTFILE};

	f = &fcalls[11];
	f->type = Topen;
	f->tag = 100;
	f->fid = 200;
	f->mode = ORDWR;

	f = &fcalls[12];
	f->type = Ropen;
	f->tag = 100;
	f->qid = (Qid){1, 1, QTFILE};
	f->iounit = 8192;

	f = &fcalls[13];
	f->type = Tcreate;
	f->tag = 100;
	f->fid = 200;
	f->name = "foo";
	f->perm = 0755;
	f->mode = ORDWR;

	f = &fcalls[14];
	f->type = Rcreate;
	f->tag = 100;
	f->qid = (Qid){1, 1, QTFILE};
	f->iounit = 8192;

	f = &fcalls[15];
	f->type = Tread;
	f->tag = 100;
	f->fid = 200;
	f->offset = 1234;
	f->count = 8192;

	f = &fcalls[16];
	f->type = Rread;
	f->tag = 100;
	f->count = 5;
	f->data = "hello";

	for(i = 0; i < nelem(fcalls); i++){
		f = &fcalls[i];

		print("<- %F\n", f);

		sz = convS2M(f, buf, sizeof(buf));
		print("9p   %4d 0x%.*H\n", sz, sz, buf);

		sz = convS2Mcbor(f, buf, sizeof(buf));
		print("cbor %4d 0x%.*H\n", sz, sz, buf);

		if(convM2Scbor(buf, sz, &f2) == 0)
			fprint(2, "convM2Scbor failed\n");
		else
			print("-> %F\n", &f2);

		// free shit in f2
	}

	exits(nil);
}
