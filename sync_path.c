#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "block_list.h"
#include "penny/penny.h"

#include "sync_path.h"

#include <ccan/darray/darray.h>

#if 0
#include <linux/fanotify.h>
/* Allows "watching" on a given path. doesn't get creates, moves, & deletes, so
 * is essentially useless to
 * us. */
#endif
/* spawn watchers on every directory */
#include <sys/inotify.h>

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

static void queue_dir_for_scan(struct sync_path *sp, struct dir *d)
{
	list_add_tail(&sp->to_scan, &d->to_scan_entry);
}

static void retry_dir_scan(struct sync_path *sp, struct dir *d)
{
	queue_dir_for_scan(sp, d);
}

static struct dir *wait_for_dir_to_scan(struct sync_path *sp)
{
	return list_pop(&sp->to_scan, struct dir, to_scan_entry);
}

static struct dir *dir_create(char const *path, struct dir *parent, char const *name, size_t name_len)
{
	struct dir *d = malloc(offsetof(struct dir, name) + name_len + 1);

	d->dir = opendir(path);
	if (!d) {
		free(d);
		return NULL;
	}

	d->parent = parent;
	d->name_len = name_len;
	memcpy(d->name, name, name_len);
	d->name[name_len] = '\0';
	return d;
}

static size_t dirent_name_len(struct dirent *d)
{
	/* FIXME: not quite correct: while this will be valid, the name field
	 * could be termintated early with a '\0', causing this to
	 * overestimate.
	 * TODO: Determine whether this is a problem.
	 */
	// return d->d_reclen - offsetof(struct dirent, d_name);
	return strlen(d->d_name);
}

#define darray_reset(arr) do { (arr).size = 0; } while(0)
#define darray_nullterminate(arr) darray_append(arr, '\0')
#define darray_get(arr) ((arr).item)
#define darray_get_cstring(arr) ({ darray_append(arr, '\0'); (arr).item; })

/* appends to v */
static void _full_path_of_dir(struct dir const *dir, darray_char *v)
{
	if (dir->parent)
		_full_path_of_dir(dir->parent, v);
	darray_append_items(*v, dir->name, dir->name_len);
	darray_append(*v, '/');
}

static char *full_path_of_dir(struct dir const *dir, darray_char *v)
{
	darray_reset(*v);
	_full_path_of_dir(dir, v);
	return darray_get_cstring(*v);
}

static char *full_path_of_file(struct dir const *dir,
		char const *name, size_t name_len, darray_char *v)
{
	darray_reset(*v);
	_full_path_of_dir(dir, v);
	darray_append_items(*v, name, name_len);
	return darray_get_cstring(*v);
}

static char *full_path_of_entry(struct dir const *dir, struct dirent *d,
		darray_char *v)
{
	printf("FPOE: dir=%.*s dirent=%.*s\n",
			dir->name_len, dir->name, dirent_name_len(d), d->d_name);
	return full_path_of_file(dir, d->d_name, dirent_name_len(d), v);
}

static void _rel_path_of_dir(struct dir const *dir, darray_char *v)
{
	if (dir->parent) {
		_rel_path_of_dir(dir->parent, v);
		darray_append_items(*v, dir->name, dir->name_len);
		darray_append(*v, '/');
	}
}

static char *rel_path_of_dir(struct dir *dir, darray_char *v)
{
	darray_reset(*v);
	_rel_path_of_dir(dir, v);
	return darray_get_cstring(*v);
}

static char *rel_path_of_file(struct dir *dir, char const *name,
		size_t name_len, darray_char *v)
{
	darray_reset(*v);
	_rel_path_of_dir(dir, v);
	darray_append_items(*v, name, name_len);
	return darray_get_cstring(*v);
}

int sp_open(struct sync_path *sp, char const *path)
{
	*sp = (typeof(*sp)) {
		.to_scan = LIST_HEAD_INIT(sp->to_scan),
	};

	tommy_hashlin_init(&sp->entries);

#if 0
	if (!access(path, R_OK)) {
		fprintf(stderr, "could not access the sync_path: \"%s\"\n", path);
		return -1;
	}
#endif

	sp->inotify_fd = inotify_init();
	if (sp->inotify_fd < 0)
		return -2;

	/* enqueue base dir in to_scan */
	sp->root = dir_create(path, NULL, path, strlen(path));
	if (!sp->root)
		return -3;
	queue_dir_for_scan(sp, sp->root);
	return 0;
}

int sp_process(struct sync_path *sp)
{

	/* FIXME: this will break if the sync dir contains multiple
	 * filesystems. */
	size_t len = path_dirent_size(sp->root->name);

	struct dirent *d = malloc(len);
	darray_char v = darray_new();
	for (;;) {
		struct dir *dir = wait_for_dir_to_scan(sp);
		if (!dir)
			goto out;
		for (;;) {
			fprintf(stderr, "scanning: %s %p\n",
					full_path_of_dir(dir, &v),
					dir);
			struct dirent *result;
			int r = readdir_r(dir->dir, d, &result);
			if (r) {
				fprintf(stderr, "readdir_r() failed: %s\n",
						strerror(errno));
				retry_dir_scan(sp, dir);
				continue;
			}

			if (!result)
				/* done with this dir */
				break;

			/* TODO: add option to avoid leaving the current filesystem
			 * (ie: "only one fs") */
			char *it = full_path_of_entry(dir, d, &v);

			printf("fp = %s\n", it);
			if (d->d_type == DT_DIR) {
				size_t name_len = dirent_name_len(d);
				if ((name_len == 1 && d->d_name[0] == '.') ||
						(name_len == 2 && (d->d_name [0] == '.'
							&& d->d_name[1] == '.'))) {
					fprintf(stderr, "skipping: %s\n", it);
					continue;
				}
				struct dir *child = dir_create(it,
						dir,
						d->d_name,
						name_len);
				fprintf(stderr, "queuing dir: %s\n", it);
				queue_dir_for_scan(sp, child);
			}

			/* add notifiers */
			int wd = inotify_add_watch(sp->inotify_fd,
					it,
					IN_ATTRIB | IN_CREATE | IN_DELETE |
					IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF |
					IN_MOVED_FROM | IN_MOVED_TO | IN_DONT_FOLLOW |
					IN_EXCL_UNLINK);
			if (wd == -1) {
				fprintf(stderr, "inotify_add_watch failed on \"%s\": %s\n", it,
						strerror(errno));
				continue;
			}
		}

	}

out:
	darray_free(v);
	free(d);
	return 0;
}
