#ifndef XDIFF__H
#define XDIFF__H

#include <cvstools.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xdiff_interface
{
	plugin_interface plugin;
	int (*xdiff_function)(const char *name, const char *file1, const char *file2, const char *label1, const char *label2, int argc, const char *const*argv, int (*output_fn)(const char *,size_t));
};

#ifdef __cplusplus
}
#endif

#endif
