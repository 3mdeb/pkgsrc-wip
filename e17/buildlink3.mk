# $NetBSD: buildlink3.mk,v 1.8 2009/06/08 15:32:07 jsonn Exp $

BUILDLINK_TREE+=	e17

.if !defined(E17_BUILDLINK3_MK)
E17_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.e17+=	e17>=0.16.999.060
BUILDLINK_PKGSRCDIR.e17?=	../../wip/e17

.include "../../wip/ecore/buildlink3.mk"
.include "../../wip/edje/buildlink3.mk"
.include "../../devel/eet/buildlink3.mk"
.include "../../wip/efreet/buildlink3.mk"
.include "../../graphics/evas/buildlink3.mk"
.include "../../graphics/evas-buffer/buildlink3.mk"
.include "../../graphics/imlib2/buildlink3.mk"
.endif # E17_BUILDLINK3_MK

BUILDLINK_TREE+=	-e17
