.include "../Makefile.inc"

.PATH: ${S}/modules/lua

KMOD=		xgifb
IOCONF=		xgifb.ioconf
SRCS=		xgifb.c lautoc_helpers.c

DPADD+=		${DESTDIR}/usr/lib/liblautoc.a

NOGCCERROR=	1
CFLAGS+=	-Werror-implicit-function-declaration
CPPFLAGS+=	-I${S}/../external/mit/lua/dist/src \
		-I${S}/modules/lua \
		-I${S}/modules/luahw \
		-I${S}/../include -I${S}/../external/bsd/lautoc/dist/include

.include <bsd.kmodule.mk>
OBJS+=		${DESTDIR}/usr/lib/liblautoc.a
