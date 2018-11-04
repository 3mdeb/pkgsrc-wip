$NetBSD$

--- lib/msan/msan_interceptors.cc.orig	2018-11-04 15:43:06.000000000 +0000
+++ lib/msan/msan_interceptors.cc
@@ -34,6 +34,7 @@
 #include "sanitizer_common/sanitizer_libc.h"
 #include "sanitizer_common/sanitizer_linux.h"
 #include "sanitizer_common/sanitizer_tls_get_addr.h"
+#include "sanitizer_common/sanitizer_vector.h"
 
 #if SANITIZER_NETBSD
 #define fstat __fstat50
@@ -1086,23 +1087,78 @@ struct MSanAtExitRecord {
   void *arg;
 };
 
-void MSanAtExitWrapper(void *arg) {
+struct InterceptorContext {
+  BlockingMutex atexit_mu;
+  Vector<struct MSanAtExitRecord *> AtExitStack;
+
+  InterceptorContext()
+      : AtExitStack() {
+  }
+};
+
+static ALIGNED(64) char interceptor_placeholder[sizeof(InterceptorContext)];
+InterceptorContext *interceptor_ctx() {
+  return reinterpret_cast<InterceptorContext*>(&interceptor_placeholder[0]);
+}
+
+void MSanAtExitWrapper() {
+  MSanAtExitRecord *r;
+  {
+    BlockingMutexLock l(&interceptor_ctx()->atexit_mu);
+
+    uptr element = interceptor_ctx()->AtExitStack.Size() - 1;
+    r = interceptor_ctx()->AtExitStack[element];
+    interceptor_ctx()->AtExitStack.PopBack();
+  }
+
+  UnpoisonParam(1);
+  ((void(*)())r->func)();
+  InternalFree(r);
+}
+
+void MSanCxaAtExitWrapper(void *arg) {
   UnpoisonParam(1);
   MSanAtExitRecord *r = (MSanAtExitRecord *)arg;
   r->func(r->arg);
   InternalFree(r);
 }
 
+static int setup_at_exit_wrapper(void(*f)(), void *arg, void *dso);
+
+// Unpoison argument shadow for C++ module destructors.
+INTERCEPTOR(int, atexit, void (*func)()) {
+  if (msan_init_is_running) return REAL(atexit)(func);
+  return setup_at_exit_wrapper((void(*)())func, 0, 0);
+}
+
 // Unpoison argument shadow for C++ module destructors.
 INTERCEPTOR(int, __cxa_atexit, void (*func)(void *), void *arg,
             void *dso_handle) {
   if (msan_init_is_running) return REAL(__cxa_atexit)(func, arg, dso_handle);
+  return setup_at_exit_wrapper((void(*)())func, arg, dso_handle);
+}
+
+static int setup_at_exit_wrapper(void(*f)(), void *arg, void *dso) {
   ENSURE_MSAN_INITED();
   MSanAtExitRecord *r =
       (MSanAtExitRecord *)InternalAlloc(sizeof(MSanAtExitRecord));
-  r->func = func;
+  r->func = (void(*)(void *a))f;
   r->arg = arg;
-  return REAL(__cxa_atexit)(MSanAtExitWrapper, r, dso_handle);
+  int res;
+  if (!dso) {
+    // NetBSD does not preserve the 2nd argument if dso is equal to 0
+    // Store ctx in a local stack-like structure
+
+    BlockingMutexLock l(&interceptor_ctx()->atexit_mu);
+
+    res = REAL(__cxa_atexit)((void (*)(void *a))MSanAtExitWrapper, 0, 0);
+    if (!res) {
+      interceptor_ctx()->AtExitStack.PushBack(r);
+    }
+  } else {
+    res = REAL(__cxa_atexit)(MSanCxaAtExitWrapper, r, dso);
+  }
+  return res;
 }
 
 static void BeforeFork() {
@@ -1522,6 +1578,9 @@ namespace __msan {
 void InitializeInterceptors() {
   static int inited = 0;
   CHECK_EQ(inited, 0);
+
+  new(interceptor_ctx()) InterceptorContext();
+
   InitializeCommonInterceptors();
   InitializeSignalInterceptors();
 
@@ -1631,6 +1690,7 @@ void InitializeInterceptors() {
 
   INTERCEPT_FUNCTION(pthread_join);
   INTERCEPT_FUNCTION(tzset);
+  INTERCEPT_FUNCTION(atexit);
   INTERCEPT_FUNCTION(__cxa_atexit);
   INTERCEPT_FUNCTION(shmat);
   INTERCEPT_FUNCTION(fork);
