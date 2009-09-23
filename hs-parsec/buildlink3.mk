# $NetBSD: buildlink3.mk,v 1.2 2009/09/23 08:59:55 phonohawk Exp $

BUILDLINK_TREE+=	hs-parsec

.if !defined(HS_PARSEC_BUILDLINK3_MK)
HS_PARSEC_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.hs-parsec+=	hs-parsec>=3.0.1
BUILDLINK_PKGSRCDIR.hs-parsec?=	../../wip/hs-parsec

.include "../../wip/hs-mtl/buildlink3.mk"
.endif	# HS_PARSEC_BUILDLINK3_MK

BUILDLINK_TREE+=	-hs-parsec
