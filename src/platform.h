#ifndef PLATFORM_H
#define PLATFORM_H

#include <gtk/gtk.h>

// Use Command key on macOS, Control on other platforms
#ifdef __APPLE__
#define REVEL_MOD_MASK GDK_META_MASK
#else
#define REVEL_MOD_MASK GDK_CONTROL_MASK
#endif

#endif // PLATFORM_H
