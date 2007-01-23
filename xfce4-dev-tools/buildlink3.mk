# $NetBSD: buildlink3.mk,v 1.4 2007/01/23 08:00:35 marttikuparinen Exp $

BUILDLINK_DEPTH:=		${BUILDLINK_DEPTH}+
XFCE4_DEV_TOOLS_BUILDLINK3_MK:=	${XFCE4_DEV_TOOLS_BUILDLINK3_MK}+

.if ${BUILDLINK_DEPTH} == "+"
BUILDLINK_DEPENDS+=	xfce4-dev-tools
.endif

BUILDLINK_PACKAGES:=	${BUILDLINK_PACKAGES:Nxfce4-dev-tools}
BUILDLINK_PACKAGES+=	xfce4-dev-tools
BUILDLINK_ORDER:=	${BUILDLINK_ORDER} ${BUILDLINK_DEPTH}xfce4-dev-tools

.if ${XFCE4_DEV_TOOLS_BUILDLINK3_MK} == "+"
BUILDLINK_API_DEPENDS.xfce4-dev-tools+=	xfce4-dev-tools>=4.4.0
BUILDLINK_PKGSRCDIR.xfce4-dev-tools?=	../../wip/xfce4-dev-tools
.endif	# XFCE4_DEV_TOOLS_BUILDLINK3_MK

BUILDLINK_DEPTH:=		${BUILDLINK_DEPTH:S/+$//}
