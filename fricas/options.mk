# $NetBSD: options.mk,v 1.3 2008/03/04 13:29:43 asau Exp $

PKG_OPTIONS_VAR=		PKG_OPTIONS.fricas
PKG_OPTIONS_REQUIRED_GROUPS=	lisp
PKG_OPTIONS_GROUP.lisp=		clisp sbcl

PKG_SUPPORTED_OPTIONS+=		x11

PKG_SUGGESTED_OPTIONS+=		clisp # x11

.include "../../mk/bsd.options.mk"

# Select Lisp backend
.if !empty(PKG_OPTIONS:Mclisp)
FASL=			fas
BUILD_DEPENDS+=		clisp>=2.41:../../lang/clisp
CONFIGURE_ARGS+=	--with-lisp=clisp
.endif
.if !empty(PKG_OPTIONS:Msbcl)
FASL=			fasl
BUILD_DEPENDS+=		sbcl-[0-9]*:../../lang/sbcl
CONFIGURE_ARGS+=	--with-lisp=sbcl
.endif

# Fix suffix for "fast load" files:
PLIST_SUBST+=	FASL=${FASL:Q}

# X11
.if !empty(PKG_OPTIONS:Mx11)
CONFIGURE_ARGS+=	--with-x=yes
.include "../../x11/libX11/buildlink3.mk"
.include "../../x11/libXpm/buildlink3.mk"
.else
CONFIGURE_ARGS+=	--with-x=no
.endif

.for opt in clisp sbcl
.if !empty(PKG_OPTIONS:M${opt})
PLIST_SUBST+=	${opt}=""
.else
PLIST_SUBST+=	${opt}="@comment "
.endif
.endfor
