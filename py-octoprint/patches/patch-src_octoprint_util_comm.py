$NetBSD$

Also look for the default USB serial ports on NetBSD

--- src/octoprint/util/comm.py.orig	2020-12-10 11:34:21.000000000 +0000
+++ src/octoprint/util/comm.py
@@ -211,6 +211,7 @@ def serialList():
             + glob.glob("/dev/cu.*")
             + glob.glob("/dev/cuaU*")
             + glob.glob("/dev/ttyS*")
+            + glob.glob("/dev/ttyU*")
             + glob.glob("/dev/rfcomm*")
         )
 
