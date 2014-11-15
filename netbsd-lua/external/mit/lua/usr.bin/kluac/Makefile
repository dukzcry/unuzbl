PROG=		kluac
SRCS=		luac.c print.c
BINDIR=		/usr/bin

#
# statically link ${LIBLUA} as "hidden" symbols need to be accessed
#
DPADD+=		${DESTDIR}/usr/lib/libklua.a ${LIBM}
LDADD+=		${DESTDIR}/usr/lib/libklua.a -lm

.include <bsd.own.mk>

.PATH:		${NETBSDSRCDIR}/external/mit/lua/dist/src \
		${NETBSDSRCDIR}/external/mit/lua/dist/doc

.include <bsd.prog.mk>
