# $NetBSD: buildlink3.mk,v 1.2 2009/03/20 19:43:43 jsonn Exp $

BUILDLINK_TREE+=	kdebase4

.if !defined(KDEBASE4_BUILDLINK3_MK)
KDEBASE4_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.kdebase4+=	kdebase4>=4.1.85
BUILDLINK_PKGSRCDIR.kdebase4?=	../../wip/kdebase4
.endif # KDEBASE4_BUILDLINK3_MK

BUILDLINK_TREE+=	-kdebase4
