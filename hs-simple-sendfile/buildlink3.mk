# $NetBSD: buildlink3.mk,v 1.2 2014/05/15 21:49:10 szptvlfn Exp $

BUILDLINK_TREE+=	hs-simple-sendfile

.if !defined(HS_SIMPLE_SENDFILE_BUILDLINK3_MK)
HS_SIMPLE_SENDFILE_BUILDLINK3_MK:=

BUILDLINK_API_DEPENDS.hs-simple-sendfile+=	hs-simple-sendfile>=0.2.13
BUILDLINK_PKGSRCDIR.hs-simple-sendfile?=	../../wip/hs-simple-sendfile

.include "../../wip/hs-conduit/buildlink3.mk"
.include "../../wip/hs-network/buildlink3.mk"
.include "../../devel/hs-transformers/buildlink3.mk"
.endif	# HS_SIMPLE_SENDFILE_BUILDLINK3_MK

BUILDLINK_TREE+=	-hs-simple-sendfile
