.include "../Makefile.inc"

KMOD=		xgifb
IOCONF=	xgifb.ioconf
SRCS=		xgifb.c

NOGCCERROR=	1
CFLAGS+=	-Werror-implicit-function-declaration
CPPFLAGS+=	-I${S}/../external/mit/lua/dist/src \
		-I${S}/modules/lua \
		-I${S}/modules/luahw

.include <bsd.kmodule.mk>
