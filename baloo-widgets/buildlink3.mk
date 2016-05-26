# $NetBSD$

BUILDLINK_TREE+=	baloo-widgets

.if !defined(BALOO_WIDGETS_BUILDLINK3_MK)
BALOO_WIDGETS_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.baloo-widgets+=	baloo-widgets>=16.04.0
BUILDLINK_PKGSRCDIR.baloo-widgets?=	../../wip/baloo-widgets

.include "../../sysutils/baloo5/buildlink3.mk"
.include "../../x11/qt5-qtbase/buildlink3.mk"
.endif	# BALOO_WIDGETS_BUILDLINK3_MK

BUILDLINK_TREE+=	-baloo-widgets
