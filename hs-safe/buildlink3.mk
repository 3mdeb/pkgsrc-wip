# $NetBSD: buildlink3.mk,v 1.4 2014/07/26 21:49:27 szptvlfn Exp $

BUILDLINK_TREE+=	hs-safe

.if !defined(HS_SAFE_BUILDLINK3_MK)
HS_SAFE_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.hs-safe+=	hs-safe>=0.3.7
BUILDLINK_PKGSRCDIR.hs-safe?=	../../wip/hs-safe
.endif	# HS_SAFE_BUILDLINK3_MK

BUILDLINK_TREE+=	-hs-safe
