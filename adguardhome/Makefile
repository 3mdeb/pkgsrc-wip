# $NetBSD$

.include "go-modules.mk"

GITHUB_PROJECT=	AdGuardHome
GITHUB_TAG=	v0.104.3
DISTNAME=	v0.104.3
PKGNAME=	adguardhome-${DISTNAME:S,^v,,}
CATEGORIES=	net
MASTER_SITES=	${MASTER_SITE_GITHUB:=AdguardTeam/}
EXTRACT_SUFX=	.zip
DIST_SUBDIR=	${GITHUB_PROJECT}
PKGREVISION=	1

FRONTEND=	${PKGNAME_NOREV}-frontend.tar.gz
SITES.adguardhome-0.104.3-frontend.tar.gz=	https://cloud.krawczyk.it/
DISTFILES=	${DEFAULT_DISTFILES} ${FRONTEND}

MAINTAINER=	bbartlomiej@gmail.com
HOMEPAGE=	https://github.com/AdguardTeam/AdGuardHome/
COMMENT=	Free and open source, network-wide ads & trackers blocking DNS server
LICENSE=	gnu-gpl-v3

WRKSRC=		${WRKDIR}/AdGuardHome-${DISTNAME:S,^v,,}

USE_TOOLS+=	tar
#USE_LANGUAGES=	# none

# This software requires some frontend assets which are generated by npm and
# nodejs packages. To build those asses please uncomment below variables and
# run generate-frontend target. Next the resulting archive has to be uploaded
# to distfile location on pkgsrc distfile server. This has to be done once per
# package version by pkgsrc maintainer and only when those assets change. The
# reason is if npm commands are run during the build doznes of npm packages
# would get downloaded bypassing pkgsrc fetch model.
#BUILD_DEPENDS+=	npm>=6.14:../../lang/npm
#BUILD_DEPENDS+=	nodejs>=10.16:../../lang/nodejs

INSTALLATION_DIRS+=	sbin libdata/adguardhome
PKG_SYSCONFSUBDIR=	adguardhome
RCD_SCRIPTS+=		adguardhome

do-build:
	${RUN} cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${GO} generate ./...
	${RUN} cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${GO} build -ldflags="-s -w -X main.version=${DISTNAME:S,^v,,} -X main.channel=release -X main.goarm=${GOARM}"

#pre-install:
#	${RUN} ${RM} ${WRKDIR}/.destdir/usr/pkg/bin/packr

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/AdGuardHome ${DESTDIR}${PREFIX}/sbin/adguardhome

.PHONY: generate-frontend
generate-frontend:
	cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} npm --prefix client ci
	cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} npm --prefix client run build-prod
	cd ${WRKDIR} && ${TAR} -zcf ${WRKDIR}/${PKGNAME_NOREV}-frontend.tar.gz ${PKGNAME_NOREV}/build
	@${ECHO} "Now upload ${WRKDIR}/${PKGNAME_NOREV}-frontend.tar.gz to LOCAL_PORTS"

.include "../../mk/bsd.prefs.mk"

PREPEND_PATH+=	${WRKDIR}/.gopath/bin

.include "../../lang/go/go-module.mk"
.include "../../mk/bsd.pkg.mk"
