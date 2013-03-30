#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include "block_list.h"
#include "tommyds/tommy.h"
#include "penny/list.h"

#if 0
#include <linux/fanotify.h>
/* Allows "watching" on a given path. doesn't get creates, moves, & deletes, so
 * is essentially useless to
 * us. */
#endif

/* spawn watchers on every directory */
#include <linux/inotify.h>

struct dir {
	struct dir *parent;
	DIR *dir;
	struct dirent d;
	/* Note: dirent is sized as is appropriate via _PC_NAME_MAX */
};

struct sync_path {
	char const *dir_path;
	struct dir *root;

	tommy_hash_lin entries;
	pthread_t io_th;

	/* elements are <something that refers to directories> that need to be
	 * scanned for new watches. */
	struct list_head to_scan;
};

static size_t path_dirent_size(char const *path)
{
	size_t name_max = pathconf(path, _PC_NAME_MAX);
	if (name_max == -1)
		name_max = 255;
	return offsetof(struct dirent, d_name) + name_max + 1;
}

static size_t dir_full_path_length(struct dir *parent)
{
	size_t s = 0;
	while(parent) {
		s += parent->d.d_reclen;
		parent = parent->parent;
	}

	return s;
}

static void scan_this_dir(struct sync_path *sp, char const *relative_path,
		size_t rel_path_len, struct dir *parent)
{
	char const *path;
	size_t path_len;
	if (!parent) {
		path = relative_path;
		path_len = rel_path_len;
	} else {
		path_len = dir_full_path_length(parent) + rel_path_len;
		path = malloc(path_len);
	}
}

static void *sp_index_daemon(void *varg)
{
	struct sync_path *sp = varg;

	/* enqueue base dir in to_scan */
	sp->root = dir_create_exact(sp->dir_path);
	scan_this_dir(sp, sp->root, NULL);

	/* dirent sizing */
	struct dirent *entry = malloc(path_dir);
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
		struct dirent *result;
		int r = readdir_r(dir, entry, &result);
		if (r) {
			fprintf(stderr, "readdir_r() failed: %s\n", strerror(errno));
			exit(1);
		}

		if (!result)
			/* done with this dir */
			break;

		if (entry->d_type == DT_DIR) {
			scan_this_dir(
			sp_queue_dir(sp, );
			entry = malloc(len);
			if (!entry) {
				fprintf(stderr, "OOM\n");
				exit(1);
			}
		}

	}
}

int sp_open(struct sync_path *sp, char const *path)
{
	*sp = typeof(*sp) {
		.dir_path = path,
	};

	if (!access(path, R_OK)) {
		fprintf(stderr, "could not access the sync_path\n");
		return -1;
	}

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
