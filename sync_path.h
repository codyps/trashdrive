#ifndef SYNC_PATH_H_
#define SYNC_PATH_H_

#include <sys/types.h>
#include <dirent.h>

#include <ccan/list/list.h>
#include <tommyds/tommy.h>

struct dir {
	struct dir *parent;
	DIR *dir;

	struct list_node to_scan_entry;

	/* both are relative to the parent */
	size_t name_len;
	char name[];
};

struct sync_path;
struct sync_path_cb {
	void (*on_added_file)(struct sync_path *sp,
		struct dir const *parent,
		char const *name);
	void (*on_event)(struct sync_path *sp,
		struct dir const *parent,
		char const *name);
};

struct sync_path {
	struct dir *root;

	int inotify_fd;

	tommy_hashlin entries;
	tommy_hashlin wd_to_path;

	pthread_t io_th;

	/* elements are <something that refers to directories> that need to be
	 * scanned for new watches. */
	struct list_head to_scan;

	struct sync_path_cb cb;
};

/* Options for async api:
 * types:
 * a) spawn seperate thread
 *    d) use pthread_cond to pass "events"
 *    e) use a pipe or socketpair to pass "events"
 * b) don't spawn seperate thread
 *    c) use callbacks
 *
 * Possible combinations:
 * a+{d,e} - _should_not_ combine a with c because the api user would need to
 *           be aware of locking considerations.
 *
 *
 * b+c     - the question is (A) is it going to take too long to populate the
 *           sync_path in the same thread? Will this block user interactivity?
 *
 ****
 * - give a fd to a user who wants to recieve events, they poll on the fd and
 *   call another api func with that fd is "ready" (for read? for write?
 *   something else? any event?)
 * - provide a function that blocks until an event is ready, and returns it for
 *   processing.
 * - request the specification of callbacks (with coarse or fine granularity)
 *   when the object is opened.
 *   - provide a func which processes some amount of data and/or blocks with a
 *     timeout.
 *   - have callbacks execute in a seperate thread, spawn a thread per object.
 */

int sp_open(struct sync_path *sp, char const *path);

/* blocks forever */
int sp_process(struct sync_path *sp);
#endif
