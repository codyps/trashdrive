#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include "block_list.h"
#include "tommyds/tommy.h"

#if 0
#include <linux/fanotify.h>
/* Allows "watching" on a given path. doesn't get creates, moves, & deletes, so
 * is essentially useless to
 * us. */
#endif

/* spawn watchers on every directory */
#include <linux/inotify.h>

struct sync_path {
	char const *dir_path;
	int dir_fd;

	tommy_hash_lin entries;
	pthread_t io_th;

	/* elements are <something that refers to directories> that need to be
	 * scanned for new watches. */
	struct block_list to_scan;
};

static void *sp_index_daemon(void *varg)
{
	struct sync_path *sp = varg;
	LIST_HEAD(to_scan);

	/* dirent sizing */
	size_t name_max = pathconf(sp->dir_path, _PC_NAME_MAX);
	if (name_max == -1)
		name_max = 255;
	size_t len = offsetof(struct dirent, d_name) + name_max + 1;
	struct dirent *entry = malloc(len);
	if (!entry) {
		fprintf(stderr, "failed to allocate entry\n");
		exit(1);
	}

	DIR *dir = fdopendir(sp->dir_fd);
	if (!dir) {
		fprintf(stderr, "something went wrong.\n");
		exit(1);
	}

	for (;;) {
		struct dirent entry, *result;
		int r = readdir_r(dir, &entry, &result);
		if (r) {
			fprintf(stderr, "readdir_r() failed: %s\n", strerror(errno));
			exit(1);
		}

		if (entry->

		if (!entry)
			break;
	}
}

int sp_open(struct sync_path *sp, char const *path)
{
	*sp = typeof(*sp) {
		.dir_path = path,
	};


	int r = open(sp->dir_path, O_RDONLY);
	if (r == -1)
		return -1;

	sp->dir_fd = r;

	tommy_hashlin_init(&sp->entries);

	/* should we spawn a thread to handle the scanning of the directory? I think so */
	r = pthread_create(&sp->io_th, NULL, sp_index_daemon, sp);
	if (r)
		return r;

	return 0;
}

void usage(const char *prog_name)
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

	return 0;
}
