#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <errno.h>

#include <ccan/array_size/array_size.h>
#include <ccan/err/err.h>

#include "sync_path.h"

static void usage(const char *prog_name)
{
	fprintf(stderr, "usage: %s <sync path>\n", prog_name);
	exit(1);
}

static void on_added_file(struct sync_path *sp,
		struct dir const *parent,
		char const *name)
{

}

static void on_event(struct sync_path *sp,
		struct dir const *parent,
		char const *name)
{

}

int main(int argc, char **argv)
{
	if (argc != 2) {
		usage(argc?argv[0]:"index");
	}

	struct sync_path sp = {
		.cb = {
			.on_added_file = on_added_file,
			.on_event = on_event,
		},
	};
	int r = sp_open(&sp, argv[1]);
	if (r) {
		fprintf(stderr, "could not open path \"%s\": %s\n",
				argv[1], strerror(errno));
		return 1;
	}

	sp_process(&sp);

	struct pollfd pfd[1] = {
		{
			.fd = sp.inotify_fd,
			.events = POLLIN,
		}
	};

	for (;;) {
		r = poll(pfd, ARRAY_SIZE(pfd), -1);
		if (r != 1) {
			warn("poll returned with no fd.");
			continue;
		}

		printf("POLL\n");

		sp_process_inotify_fd(&sp);
	}

	return 0;
}
