# $NetBSD$

GITHUB_PROJECT=	fwupd
GITHUB_TAG=	1.5.7
DISTNAME=	1.5.7
PKGNAME=	${GITHUB_PROJECT}-${DISTNAME}
CATEGORIES=	sysutils
MASTER_SITES=	${MASTER_SITE_GITHUB:=fwupd/}
DIST_SUBDIR=	${GITHUB_PROJECT}

MAINTAINER=	pkgsrc-users@NetBSD.org
HOMEPAGE=	https://github.com/fwupd/fwupd/
COMMENT=	firmware update tool
LICENSE=	gnu-lgpl-v2.1

WRKSRC=			${WRKDIR}/fwupd-1.5.7
USE_LANGUAGES=		c c++

USE_LIBTOOL=	yes
USE_TOOLS+=	pkg-config msgfmt xgettext
MESON_ARGS+=	-Dplugin_uefi_capsule='false' \
		-Dplugin_dell='false' \
		-Dplugin_redfish='false' \
		-Dplugin_nvme='false' \
		-Dplugin_amt='false' \
		-Dplugin_emmc='false' \
		-Dplugin_altos='false' \
		-Dplugin_thunderbolt='false' \
		-Dplugin_synaptics_mst='false' \
		-Dpolkit='false' \
		-Dman='false' \
		-Dgudev='false' \
		-Dtests='false'\
		-Dplugin_tpm='false' \
		-Dsystemd='false' \
		-Dlibjcat:man='false' \
		-Dlibjcat:tests='false' \
		-Dplugin_synaptics_rmi='false' \
		-Dgcab:docs='false'

.include "../../mk/bsd.prefs.mk"

BUILDLINK_API_DEPENDS.glib2+= glib2>2.58.0
.include "../../devel/glib2/buildlink3.mk"
.include "../../devel/gobject-introspection/buildlink3.mk"
.include "../../lang/vala/buildlink3.mk"
.include "../../textproc/gtk-doc/buildlink3.mk"
.include "../../textproc/json-glib/buildlink3.mk"
.include "../../archivers/libarchive/buildlink3.mk"
.include "../../security/gpgme/buildlink3.mk"
.include "../../devel/libusb1/buildlink3.mk"
.include "../../security/gnutls/buildlink3.mk"
.include "../../databases/sqlite3/buildlink3.mk"
.include "../../www/curl/buildlink3.mk"
.include "../../devel/meson/build.mk"
.include "../../lang/python/tool.mk"
.include "../../mk/bsd.pkg.mk"
