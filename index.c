#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <dirent.h>

#include "block_list.h"
#include "tommyds/tommy.h"
#include "penny/penny.h"
#include "ccan/list/list.h"

#if 0
#include <linux/fanotify.h>
/* Allows "watching" on a given path. doesn't get creates, moves, & deletes, so
 * is essentially useless to
 * us. */
#endif

/* spawn watchers on every directory */
#include <sys/inotify.h>

struct dir {
	struct dir *parent;
	DIR *dir;

	struct list_node to_scan_entry;

	/* both are relative to the parent */
	size_t name_len;
	char name[];
};

struct sync_path {
	char const *dir_path;
	size_t dir_path_len;

	struct dir *root;

	int inotify_fd;

	tommy_hashlin entries;
	tommy_hashlin wd_to_path;

	pthread_t io_th;

	/* elements are <something that refers to directories> that need to be
	 * scanned for new watches. */
	struct list_head to_scan;
	pthread_mutex_t  to_scan_lock;
	pthread_cond_t   to_scan_cond;
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
	pthread_mutex_lock(&sp->to_scan_lock);
	list_add_tail(&sp->to_scan, &d->to_scan_entry);
	pthread_cond_signal(&sp->to_scan_cond);
	pthread_mutex_unlock(&sp->to_scan_lock);
}

static void retry_dir_scan(struct sync_path *sp, struct dir *d)
{
	scan_this_dir(sp, d);
}

static struct dir *wait_for_dir_to_scan(struct sync_path *sp)
{
	int rc = 0;
	struct dir *d;
	pthread_mutex_lock(&sp->to_scan_lock);
	while (list_empty(&sp->to_scan) && rc == 0)
		rc = pthread_cond_wait(&sp->to_scan_cond, &sp->to_scan_lock);
	d = list_pop(&sp->to_scan, typeof(*d), to_scan_entry);
	pthread_mutex_unlock(&sp->to_scan_lock);
	return d;
}

static struct dir *dir_create_exact(char const *path, size_t path_len, struct dir *parent)
{
	struct dir *d = malloc(offsetof(struct dir, name) + path_len + 1);

	d->dir = opendir(path);
	if (!d) {
		free(d);
		return NULL;
	}

	d->parent = parent;
	d->name_len = path_len;
	memcpy(d->name, path, path_len + 1);
	return d;
}

struct vec {
	size_t bytes;
	void *data;
};

#define VEC_INIT() { .bytes = 0, .data = NULL }

static int vec_reinit_grow(struct vec *v, size_t min_size)
{
	if (min_size > v->bytes) {
		free(v->data);
		v->data = malloc(min_size);
		if (!v->data)
			return -1;
	}

	return 0;
}

static size_t dirent_name_len(struct dirent *d)
{
	/* FIXME: not quite correct: while this will be valid, the name field
	 * could be termintated early with a '\0', causing this to
	 * overestimate.
	 * TODO: Determine whether this is a problem.
	 */
	return d->d_reclen - offsetof(struct dirent, d_name);
}

static char *full_path_of_entry(struct dir *dir, struct dirent *d, struct vec *v)
{
	vec_reinit_grow(v, dir_full_path_length(dir) + dirent_name_len(d));
	/* TODO: */
	return v->data;
}

static void *sp_index_daemon(void *varg)
{
	struct sync_path *sp = varg;

	/* FIXME: this will break if the sync dir contains multiple filesystems. */
	size_t len = path_dirent_size(sp->dir_path);

	struct dirent *d = malloc(len);
	struct vec v = VEC_INIT();
	for (;;) {
		struct dir *dir = wait_for_dir_to_scan(sp);
		struct dirent *result;
		int r = readdir_r(dir->dir, d, &result);
		if (r) {
			fprintf(stderr, "readdir_r() failed: %s\n", strerror(errno));
			retry_dir_scan(sp, dir);
			continue;
		}

		if (!result)
			/* done with this dir */
			break;

		/* TODO: add option to avoid leaving the current filesystem
		 * (ie: "only one fs") */

		if (d->d_type == DT_DIR) {
			struct dir *child = dir_create_exact(d->d_name, dirent_name_len(d), dir);
			scan_this_dir(sp, child);
		}

		/* add notifiers */
		int wd = inotify_add_watch(sp->inotify_fd, full_path_of_entry(dir, d, &v),
				IN_ATTRIB | IN_CREATE | IN_DELETE |
				IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF |
				IN_MOVED_FROM | IN_MOVED_TO | IN_DONT_FOLLOW |
				IN_EXCL_UNLINK);
		if (wd == -1) {
			fprintf(stderr, "inotify_add_watch failed: %s\n", strerror(errno));
			continue;
		}


	}

	return NULL;
}

static int sp_open(struct sync_path *sp, char const *path)
{
	*sp = (typeof(*sp)) {
		.dir_path = path,
		.dir_path_len = strlen(path),

		.to_scan = LIST_HEAD_INIT(sp->to_scan),
		.to_scan_cond = PTHREAD_COND_INITIALIZER,
		.to_scan_lock = PTHREAD_MUTEX_INITIALIZER,
	};

	tommy_hashlin_init(&sp->entries);

	if (!access(path, R_OK)) {
		fprintf(stderr, "could not access the sync_path\n");
		return -1;
	}

	sp->inotify_fd = inotify_init();
	if (sp->inotify_fd < 0)
		return -2;

	/* enqueue base dir in to_scan */
	sp->root = dir_create_exact(sp->dir_path, sp->dir_path_len, NULL);
	if (!sp->root)
		return -3;
	scan_this_dir(sp, sp->root);

	/* should we spawn a thread to handle the scanning of the directory? I
	 * think so */
	int r = pthread_create(&sp->io_th, NULL, sp_index_daemon, sp);
	if (r)
		return r;

	return 0;
}

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

	return 0;
}
