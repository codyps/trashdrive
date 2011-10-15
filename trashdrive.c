#define _GNU_SOURCE /* for basename */
#include <string.h>

#include <stdio.h>
#include <uv.h>

#include <stdlib.h>

#include <time.h>

#define LOG(fmt, ...) do {				\
	fprintf(stderr, fmt, ## __VA_ARGS__);		\
	putc('\n', stderr);				\
} while(0)

#define LOG_FATAL LOG

#define p_debug(fmt, ...) do {				\
	fflush(stdout);					\
	fprintf(stderr, "%s:%d:", __FILE__, __LINE__);	\
	fprintf(stderr, fmt, ## __VA_ARGS__);		\
} while(0)


/* uv_fs_event { UV_HANDLE; char* filename }
 * UV_HANDLE   { uv_loop_t* loop; uv_handle_type type; uv_close_cb close_cb; void *data }
 */

/* uv_fs_t { UV_REQ; uv_loop_t *loop; uv_fs_type fs_type; uv_fs_cb cb; ssize_t
 * result; void *ptr; char *path; int errorno; }
 *
 * UV_REQ
 */


#include <assert.h>
#define ASSERT assert

static void mark_newer(const char *filename)
{
	LOG("marking \"%s\" as newer", filename);
}

static void file_in_dir_stat_cb(uv_fs_t *req);


static char *pathcatdup(const char *base, const char *rest)
{
	size_t base_len = strlen(base);
	size_t rest_len = strlen(rest);
	char *full = malloc(base_len + 1 + rest_len + 1);
	memcpy(full, base, base_len);
	full[base_len] = '/';
	memcpy(full + base_len + 1, rest, rest_len + 1);
	return full;
}

#define __unused __attribute__((__unused__))

static void dir_event_cb(uv_fs_event_t *handle, const char *filename, int events, __unused int status)
{
	/* if the dir the watch was on was removed, destroy the watch */
	if ((events & UV_CHANGE) && !strcmp(filename, basename(handle->filename))) {
		uv_close((uv_handle_t *)handle, /* uv_close_cb */NULL);
		free(handle);
		return;
	}

	/* mark as needing a new backup */
	mark_newer(filename);

	/* if a dir was added, do readdir_cb */
	uv_fs_t *req = malloc(sizeof(*req));
	char *full_path = pathcatdup(handle->filename, filename);
	uv_fs_lstat(handle->loop, req, full_path, file_in_dir_stat_cb);
	free(full_path);
}

struct file_ent {
	time_t mtime;
};

static struct file_ent fe;

static struct file_ent *file_lookup(__unused char *file)
{
	return &fe;
}

static void readdir_cb(uv_fs_t *req);
static void tree_l_dir(uv_loop_t *loop, char *path);

static void file_in_dir_stat_cb(uv_fs_t *req)
{
	ASSERT(req->type == UV_FS_LSTAT);
	/* is it newer than store? if so, note that a new backup is needed */
	struct stat *st = req->ptr;
	if (st->st_mtime > file_lookup(req->path)->mtime) {
		mark_newer(req->path);
	}

	/* if directory: */
	if (S_ISDIR(st->st_mode)) {
		tree_l_dir(req->loop, req->path);
	}

	uv_fs_req_cleanup(req);
	free(req);
}

/* On a directory that needs all lower files & directories mapped */
static void readdir_cb(uv_fs_t *req)
{
	LOG("req->type = %d", req->type);
	ASSERT(req->type == UV_FS_READDIR);

	size_t file_ct  = req->result;
	char *flist     = req->ptr;
	char *base      = req->path;
	size_t base_len = strlen(base);

	char *full_path = malloc(base_len + 2);
	memcpy(full_path, base, base_len);
	full_path[base_len] = '/';
	full_path[base_len + 1] = '\0';

	while(file_ct > 0) {
		char *file = flist;
		size_t file_len = strlen(file);
		flist += file_len + 1;

		{
			/* 1 char for '/', 1 char for '\0' */
			char *tmp = realloc(full_path, base_len + 1 + file_len + 1);
			if (!tmp) {
				/* NO mem. handle */
				LOG_FATAL("No memory");
				exit(EXIT_FAILURE);
			}
			full_path = tmp;
		}

		memcpy(full_path + base_len + 1, file, file_len + 1);

		/* don't follow symlinks (as normal stat would) */
		uv_fs_t *req_next = malloc(sizeof(*req_next));
		uv_fs_lstat(req->loop, req, full_path, file_in_dir_stat_cb);
	}

	free(full_path);
	uv_fs_req_cleanup(req);
	free(req);
}

static void tree_l_dir(uv_loop_t *loop, char *path)
{
	uv_fs_event_t *handle = malloc(sizeof(*handle));
	uv_fs_event_init(loop, handle, path, dir_event_cb);

	uv_fs_t *req = malloc(sizeof(*req));
	uv_fs_readdir(loop, req, path, 0, readdir_cb);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <trash dir> <cache dir>\n",
				argc ? argv[0] : "trashdrive");
		return -1;
	}

	char *trash_dir = argv[1];
	__unused char *cache_dir = argv[2];

	fe.mtime = time(NULL);

	uv_loop_t *loop = uv_default_loop();

	tree_l_dir(loop, trash_dir);

	uv_run(loop);
	return 0;
}
