--- frame.c	Sun Mar 15 21:55:09 1998
+++ ../../amiwm-mod/frame.c	Sun Dec  2 14:32:00 2007
@@ -1,9 +1,9 @@
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 #include <X11/Xresource.h>
-#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
+//#ifdef HAVE_X11_EXTENSIONS_SHAPE_H
 #include <X11/extensions/shape.h>
-#endif
+//#endif
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
