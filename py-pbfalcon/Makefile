# $NetBSD$

###########################################################
#               Generated by fbsd2pkg                     #
###########################################################

###########################################################
# Unconverted and partially converted FreeBSD port syntax:

# Must use the exact same version of dazz_db, daligner, pypeflow, and falcon!
# Distfiles are rolled from a FALCON-integrate snapshot to ensure that
# this is possible.
# FIXME: Get to a newer version of rdflib and eliminate rdfextras
# Unknown tool: USE_TOOLS=	python
#post-stage:
#	${STRIP_CMD} ${DESTDIR}${PREFIX}/lib/python2.7/site-packages/falcon_kit/*.so

DISTNAME=	pbfalcon-${PORTVERSION}
PKGNAME=	${PYPKGPREFIX}-pbfalcon-${PORTVERSION}
CATEGORIES=	biology
MASTER_SITES=	http://acadix.biz/Ports/distfiles/
EXTRACT_SUFX=	.tar.xz

MAINTAINER=	bacon@NetBSD.org

COMMENT=	Experimental PacBio diploid assembler
# Double-check this
LICENSE=	modified-bsd

# Pessimistic assumption.  Test and change if possible.
# MAKE_JOBS_SAFE=	no

# Just assuming C and C++: Adjust this!
USE_LANGUAGES=	c c++
# PYDISTUTILSPKG=	yes

DEPENDS+=	${PYPKGPREFIX}-networkx>=0:../../math/py-networkx
DEPENDS+=	${PYPKGPREFIX}-pypeflow>=0:../../wip/py-pypeflow
DEPENDS+=	${PYPKGPREFIX}-rdflib-3.4.0:../../wip/py-rdflib340
DEPENDS+=	${PYPKGPREFIX}-rdfextras>=0:../../wip/py-rdfextras

PORTVERSION=	2015.10.28
EGG_NAME=	falcon_kit-0.4.0

post-patch:
	for f in ${WRKSRC}/src/py_scripts/*.py; do \
		${PRINTF} "#!${PYTHONBIN}\\n\\n" > $$f.temp; \
		${CAT} $$f >> $$f.temp; \
		${MV} -f $$f.temp $$f; \
	done

.include "../../lang/python/egg.mk"
.include "../../wip/dazz_db/buildlink3.mk"
.include "../../wip/daligner/buildlink3.mk"
.include "../../mk/bsd.pkg.mk"
