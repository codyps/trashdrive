#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <penny/penny.h>
#include <penny/print.h>

#include <ccan/array_size/array_size.h>
#include <ccan/pr_debug/pr_debug.h>
#include <ccan/darray/darray.h>
#include <ccan/err/err.h>
#include <ccan/compiler/compiler.h>

#include <tommyds/tommyhashlin.h>

#include "sync_path.h"
#include "block_list.h"


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
	long name_max = pathconf(path, _PC_NAME_MAX);
	if (name_max == -1)
		name_max = 255;
	return offsetof(struct dirent, d_name) + name_max + 1;
}

UNNEEDED
static size_t dir_full_path_length(struct dir *parent)
{
	size_t s = 0;
	while(parent) {
		s += parent->name_len;
		parent = parent->parent;
	}
	return s;
}

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

static char *full_path_of_entry(struct dir const *dir, struct dirent *d,
		darray_char *v)
{
	pr_debug(4, "FPOE: dir=%.*s dirent=%.*s",
			(int)dir->name_len, dir->name, (int)dirent_name_len(d), d->d_name);
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

UNNEEDED
static char *rel_path_of_dir(struct dir *dir, darray_char *v)
{
	darray_reset(*v);
	_rel_path_of_dir(dir, v);
	return darray_get_cstring(*v);
}

UNNEEDED
static char *rel_path_of_file(struct dir *dir, char const *name,
		size_t name_len, darray_char *v)
{
	darray_reset(*v);
	_rel_path_of_dir(dir, v);
	darray_append_items(*v, name, name_len);
	return darray_get_cstring(*v);
}

static void queue_dir_for_scan(struct sync_path *sp, struct dir *d)
{
	darray_char v = darray_new();
	printf("QUEUEING %s\n", full_path_of_dir(d, &v));
	darray_free(v);
	list_add_tail(&sp->to_scan, &d->to_scan_entry);
}

static void retry_dir_scan(struct sync_path *sp, struct dir *d)
{
	queue_dir_for_scan(sp, d);
}

static struct dir *get_next_dir_to_scan(struct sync_path *sp)
{
	return list_pop(&sp->to_scan, struct dir, to_scan_entry);
}

static struct dir *dir_create(struct sync_path *sp, char const *path, struct dir *parent, char const *name, size_t name_len)
{
	/* we don't need all this data for files: by watching directories, we
	 * get events on all files they contain, indicated by wd + a filename.
	 *
	 * We will probably need some data on files (dirty, when dirtied,
	 * closed since dirtied, when closed) to make choices about when to
	 * syncronize them.
	 */
	struct dir *d = malloc(offsetof(struct dir, name) + name_len + 1);
	if (!d)
		return NULL;

	/* XXX: We're using full paths instead of openat() to create
	 * directories. Will this create/solve any race issues? */
	d->dir = opendir(path);
	if (!d->dir) {
		free(d);
		return NULL;
	}

	d->parent = parent;
	d->name_len = name_len;
	memcpy(d->name, name, name_len);
	d->name[name_len] = '\0';

	/* add notifiers */
	pr_debug(3, "Adding watch on %s", path);
	int wd = inotify_add_watch(sp->inotify_fd,
			path, IN_ALL_EVENTS);
	if (wd == -1)
		fprintf(stderr, "inotify_add_watch failed on \"%s\": %s\n", path,
				strerror(errno));
	d->wd = wd;
	tommy_hashlin_insert(&sp->wd_to_dir, &d->wd_map, d, tommy_inthash_u32(d->wd));
	return d;
}

static struct dir *dir_create_under_dir(struct sync_path *sp, struct dir *parent, char const *name, size_t name_len)
{
	/* TODO: cache the darray_char inside sp */
	darray_char v = darray_new();
	char *path = full_path_of_file(parent, name, name_len, &v);
	struct dir *d = dir_create(sp, path, parent, name, name_len);
	darray_free(v);
	return d;
}

int sp_open(struct sync_path *sp, char const *path)
{
	*sp = (typeof(*sp)) {
		.to_scan = LIST_HEAD_INIT(sp->to_scan),
	};

	tommy_hashlin_init(&sp->wd_to_dir);

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
	sp->root = dir_create(sp, path, NULL, path, strlen(path));
	if (!sp->root)
		return -3;
	queue_dir_for_scan(sp, sp->root);
	return 0;
}

static int compare_wd_to_dir(const void *w_, const void *dir_)
{
	const struct dir *dir = dir_;
	int w = (uintptr_t)w_;
	/* XXX: tommy_hashlin_search() checks "== 0", we should be able to make
	 * this less bad. */
	return (w == dir->wd) ? 0 : 1;
}

static struct dir *wd_to_dir(struct sync_path *sp, int wd)
{
	struct dir *dir = tommy_hashlin_search(&sp->wd_to_dir,
					compare_wd_to_dir,
					(const void *)(uintptr_t)wd,
					tommy_inthash_u32(wd));
	return dir;
}

int sp_process(struct sync_path *sp)
{
	/* FIXME: this will break if the sync dir contains multiple
	 * filesystems. */
	size_t len = path_dirent_size(sp->root->name);

	struct dirent *d = malloc(len);
	darray_char v = darray_new();
	for (;;) {
		struct dir *dir = get_next_dir_to_scan(sp);
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
				struct dir *child = dir_create(sp,
						it,
						dir,
						d->d_name,
						name_len);
				if (!child)
					err(1, "failed to allocate child\n");
				fprintf(stderr, "queuing dir: %s\n", it);
				queue_dir_for_scan(sp, child);
			}

		}

	}

out:
	darray_free(v);
	free(d);
	return 0;
}

struct flag {
	uintmax_t mask;
	const char *name;
};

#define FLAG(m) { .mask = m, .name = #m }

/* TODO: currently only works with set/unset flags. Make it so we can define
 * mask fields with values like perf. */
static struct flag inotify_flags[] = {
	/* Individual flags */
	FLAG(IN_ACCESS),
	FLAG(IN_MODIFY),
	FLAG(IN_ATTRIB),
	FLAG(IN_CLOSE_WRITE),
	FLAG(IN_CLOSE_NOWRITE),
	FLAG(IN_OPEN),
	FLAG(IN_MOVED_FROM),
	FLAG(IN_MOVED_TO),
	FLAG(IN_CREATE),
	FLAG(IN_DELETE),
	FLAG(IN_DELETE_SELF),
	FLAG(IN_MOVE_SELF),
	FLAG(IN_UNMOUNT),
	FLAG(IN_Q_OVERFLOW),
	FLAG(IN_IGNORED),
	FLAG(IN_ONLYDIR),
	FLAG(IN_DONT_FOLLOW),
	FLAG(IN_EXCL_UNLINK),
	FLAG(IN_MASK_ADD),
	FLAG(IN_ISDIR),
	FLAG(IN_ONESHOT),
};

static void print_flags(struct flag flags[], size_t flag_ct, uintmax_t v, FILE *o)
{
	size_t i;
	bool started = false;
	uintmax_t used = 0;
	for (i = 0; i < flag_ct; i++) {
		if ((v & flags[i].mask & ~used) == flags[i].mask) {
			if (started) {
				fprintf(o, "|%s", flags[i].name);
			} else {
				started = true;
				fprintf(o, "%s", flags[i].name);
			}
			used |= flags[i].mask;
		}
	}

	uintmax_t rem = (~used) & v;
	if (rem) {
		if (started)
			fprintf(o, "|%#" PRIxMAX, rem);
		else
			fprintf(o, "%#" PRIxMAX, rem);
	}
}

static void print_inotify_event(struct inotify_event *i, FILE *f)
{
	fprintf(f, "(struct inotify_event){ .wd = %d, .mask = ", i->wd);
	print_flags(inotify_flags, ARRAY_SIZE(inotify_flags), i->mask, f);
	fprintf(f, "/* %#"PRIx32" */, .cookie = %#"PRIx32", .len = %"PRIu32", .name = \"", i->mask, i->cookie, i->len);
	print_string_as_cstring_(i->name, i->len, f);
	fputs("\" }", f);
}

int sp_process_inotify_fd(struct sync_path *sp)
{
	/* sp->inotify_fd is read to read, grab events from it an process them */
	char buf[4096];
	ssize_t r = read(sp->inotify_fd, buf, sizeof(buf));

	printf("got %zd bytes.\n", r);

	struct inotify_event *e = (struct inotify_event *)buf;
	print_inotify_event(e, stdout);
	putchar('\n');

	/* Action on a directory, we need to update our watches/queue someone for scanning */
	if (e->mask & IN_ISDIR) {
		switch (e->mask & ~IN_ISDIR) {
		case IN_CREATE:
		case IN_MOVED_TO: {
			/* inside of: */
			struct dir *parent = wd_to_dir(sp, e->wd);
			if (!parent) {
				printf("unknown wd=%d\n", e->wd);
				goto bad_event;
			}
			struct dir *dir = dir_create_under_dir(sp, parent, e->name, strlen(e->name));
			if (!dir) {
				printf("dir creation failed\n");
				goto bad_event;
			}

			queue_dir_for_scan(sp, dir);
		}
		}
	}
bad_event:
	;

	return 0;
}
