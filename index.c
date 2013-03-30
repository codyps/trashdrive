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

	/* both are relative to the parent */
	size_t name_len;
	char name[];
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
		s += parent->name_len;
		parent = parent->parent;
	}

	return s;
}

static void scan_this_dir(struct sync_path *sp, struct dir *d)
{
	char const *path;
	size_t path_len = dir_full_path_length(d->parent) + d->name_len;
}

static struct dir *dir_create_exact(char const *path, size_t path_len, struct dir *parent)
{
	struct dir *d = malloc(offsetof(struct dir, name) + path_len + 1);

	d->parent = parent;
	d->dir = diropen(path);

	d->name_len = path_len;
	memcpy(d->name, path, path_len + 1);

	return d;
}

static void *sp_index_daemon(void *varg)
{
	struct sync_path *sp = varg;

	/* enqueue base dir in to_scan */
	sp->dir_path_len = strlen(sp->dir_path);
	sp->root = dir_create_exact(sp->dir_path, sp->dir_path_len, NULL);
	scan_this_dir(sp, sp->root);

	size_t len = path_dirent_size(sp->dir_path);
	struct dirent *d = malloc(len);

	for (;;) {
		struct dir *dir = wait_for_dir_to_scan();
		struct dirent *result;
		int r = readdir_r(dir, d, &result);
		if (r) {
			fprintf(stderr, "readdir_r() failed: %s\n", strerror(errno));
			exit(1);
		}

		if (!result)
			/* done with this dir */
			break;

		if (d->d_type == DT_DIR) {
			struct dir *child = dir_create_exact(d->d_name, dirent_name_len(d), dir)
			scan_this_dir(sp, child);
		}

		/* add notifiers */
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
