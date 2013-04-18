#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "sync_path.h"

static void usage(const char *prog_name)
{
	fprintf(stderr, "usage: %s <sync path>\n", prog_name);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		usage(argc?argv[0]:"index");
	}

	struct sync_path sp;
	int r = sp_open(&sp, argv[1]);
	if (r) {
		fprintf(stderr, "could not open path \"%s\": %s\n", argv[1], strerror(errno));
		return 1;
	}

	struct sp_event e;
	while ((r = sp_wait_for_event(&sp, &e))) {

	}

	return 0;
}
