# $NetBSD: buildlink3.mk,v 1.3 2014/02/21 10:39:01 szptvlfn Exp $

BUILDLINK_TREE+=	hs-MonadCatchIO-mtl

.if !defined(HS_MONADCATCHIO_MTL_BUILDLINK3_MK)
HS_MONADCATCHIO_MTL_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.hs-MonadCatchIO-mtl+=	hs-MonadCatchIO-mtl>=0.3.0
BUILDLINK_PKGSRCDIR.hs-MonadCatchIO-mtl?=	../../wip/hs-MonadCatchIO-mtl

.include "../../wip/hs-extensible-exceptions/buildlink3.mk"
.include "../../wip/hs-mtl/buildlink3.mk"
.endif	# HS_MONADCATCHIO_MTL_BUILDLINK3_MK

BUILDLINK_TREE+=	-hs-MonadCatchIO-mtl
