$NetBSD$

--- chrome/browser/prefs/pref_service_incognito_allowlist.cc.orig	2020-07-08 21:40:34.000000000 +0000
+++ chrome/browser/prefs/pref_service_incognito_allowlist.cc
@@ -135,7 +135,7 @@ const char* const kPersistentPrefNames[]
     prefs::kShowFullscreenToolbar,
 #endif
 
-#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
+#if (defined(OS_LINUX) || defined(OS_BSD)) && !defined(OS_CHROMEOS)
     // Toggleing custom frames affects all open windows in the profile, hence
     // should be written to the regular profile when changed in incognito mode.
     prefs::kUseCustomChromeFrame,
