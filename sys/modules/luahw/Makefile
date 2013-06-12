.include "../Makefile.inc"

.PATH:          ${S}/modules/luahw

KMOD=		luahw
SRCS=		luahw.c

NOGCCERROR=	1
CPPFLAGS+=	-I${S}/../external/mit/lua/dist/src \
		-I${S}/modules/lua

.include <bsd.kmodule.mk>

