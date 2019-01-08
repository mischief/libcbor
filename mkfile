</$objtype/mkfile

P=cbor

LIB=lib$P.$O.a
OFILES=decode.$O encode.$O alloc.$O pack.$O unpack.$O
HFILES=/sys/include/$P.h
CLEANFILES=$O.test $O.bench

</sys/src/cmd/mksyslib

install:V:	$LIB
	cp $LIB /$objtype/lib/lib$P.a
	cp $P.h /sys/include/$P.h

uninstall:V:
	rm -f /$objtype/lib/lib$P.a /sys/include/$P.h

$O.test: test.$O $LIB
	$LD $LDFLAGS -o $target $prereq

test:V: $O.test
	$O.test

$O.bench: bench.$O $LIB
	$LD $LDFLAGS -o $target $prereq

bench:V: $O.bench
	$O.bench

$O.convS2M: convS2M.$O $LIB
	$LD $LDFLAGS -o $target $prereq

sync:V:
	rcpu -h contrib.9front.org -u mischief -c dircp /mnt/term/`{pwd} /usr/mischief/contrib/sys/src/libcbor
