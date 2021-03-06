# $NetBSD: buildlink3.mk,v 1.7 2020/10/13 17:32:16 wiz Exp $

BUILDLINK_TREE+=	py-numpy

.if !defined(PY_NUMPY_BUILDLINK3_MK)
PY_NUMPY_BUILDLINK3_MK:=

.include "../../lang/python/pyversion.mk"

BUILDLINK_API_DEPENDS.py-numpy+=	${PYPKGPREFIX}-numpy>=1.0
.if ${_PYTHON_VERSION} == 27
BUILDLINK_ABI_DEPENDS.py-numpy?=	${PYPKGPREFIX}-numpy>=1.16.6nb1
BUILDLINK_PKGSRCDIR.py-numpy?=		../../wip/py-numpy16
.else
BUILDLINK_ABI_DEPENDS.py-numpy?=	${PYPKGPREFIX}-numpy>=1.19.2nb1
BUILDLINK_PKGSRCDIR.py-numpy?=		../../wip/py-numpy
.endif

.include "../../mk/bsd.fast.prefs.mk"

# Dependend python packages re-use the BLAS_LIBS logic and
# need this in the environment.
MAKE_ENV+=	BLAS_LIBS=${BLAS_LIBS:Q} LAPACK_LIBS=${LAPACK_LIBS:Q}

.if ${OPSYS} != "Darwin"
.include "../../mk/blas.buildlink3.mk"
.endif
.endif # PY_NUMPY_BUILDLINK3_MK

BUILDLINK_TREE+=	-py-numpy
