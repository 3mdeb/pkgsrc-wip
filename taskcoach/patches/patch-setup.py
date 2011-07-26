$NetBSD: patch-setup.py,v 1.3 2011/07/26 09:22:07 ryo-on Exp $

* Support NetBSD

--- setup.py.orig	2011-04-30 15:01:33.000000000 +0000
+++ setup.py
@@ -76,7 +76,7 @@ for language in languages:
     setupOptions['classifiers'].append('Natural Language :: %s'%language)
 
 system = platform.system()
-if system == 'Linux':
+if (system == 'Linux') or (system == 'NetBSD'):
     setupOptions['package_data'] = {'taskcoachlib': ['bin.in/linux/_pysyncml.so']}
     # Add data files for Debian-based systems:
     current_dist = [dist.lower() for dist in platform.dist()]
