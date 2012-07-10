/*	cvsnt xdiff external libraty
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <config.h>
#include <system.h>
#include "xdiff.h"
#include <cvstools.h>
#include "../version.h"

static int (*output_fn_p)(const char *,size_t);

static int xdiff_output_fn(const char *buf,size_t len, void *)
{
	return output_fn_p(buf,len);
}

static int xdiff_function(const char *name, const char *file1, const char *file2, const char *label1, const char *label2, int argc, const char *const*argv, int (*output_fn)(const char *,size_t))
{
	cvs::string cmd_line;
	CRunFile run;

	for(size_t n=0; n<argc; n++)
	{
		const char *arg = argv[n];
		if(!strcasecmp(argv[n],"%file1%"))
			arg=file1;
		else if(!strcasecmp(argv[n],"%file2%"))
			arg=file2;
		else if(!strcasecmp(argv[n],"%label1%"))
			arg=label1;
		else if(!strcasecmp(argv[n],"%label2%"))
			arg=label1;
		else if(!strcasecmp(argv[n],"%name%"))
			arg=label1;
		run.addArg(arg);
	}

	output_fn_p = output_fn;
	run.setOutput(xdiff_output_fn, NULL);
	run.run(argv[0]);
	int ret;
	run.wait(ret);
	return ret;
}

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static xdiff_interface ext_xdiff =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"External XDiff handler",CVSNT_PRODUCTVERSION_STRING,"ExternalXdiff",
		init,
		destroy,
		get_interface,
		NULL /* Configure */
	},
	xdiff_function
};

static int init(const struct plugin_interface *plugin)
{
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitXdiff)
		return NULL;

	return (void*)&ext_xdiff;
}

plugin_interface *get_plugin_interface()
{
	return (plugin_interface*)&ext_xdiff;
}
