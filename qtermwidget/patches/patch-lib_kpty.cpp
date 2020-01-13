$NetBSD$

Allow kpty to find libutil

--- lib/kpty.cpp.orig	2019-02-25 22:13:12.000000000 +0000
+++ lib/kpty.cpp
@@ -27,7 +27,7 @@
 #include <QtDebug>
 
 
-#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
+#if defined(__FreeBSD__) || defined(__DragonFly__)
 #define HAVE_LOGIN
 #define HAVE_LIBUTIL_H
 #endif
@@ -37,6 +37,11 @@
 #define HAVE_UTIL_H
 #endif
 
+#if defined(__NetBSD__)
+#define HAVE_LOGIN
+#define HAVE_UTIL_H
+#endif
+
 #if defined(__APPLE__)
 #define HAVE_OPENPTY
 #define HAVE_UTIL_H