.include "../Makefile.inc"

KMOD=		luahw
SRCS=		luahw.c

NOGCCERROR=	1
CFLAGS+=	-Werror-implicit-function-declaration
CPPFLAGS+=	-I${S}/../external/mit/lua/dist/src \
		-I${S}/modules/lua

.include <bsd.kmodule.mk>
