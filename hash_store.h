#ifndef HASH_STORE_H_
#define HASH_STORE_H_

/* i: I was writing this, but couldn't I just use a db like tokyocabinet?
 * c: single file dbs don't play nice with COW fs
 * i: Doesn't libgit2 have some type of lib implimenting a hash store?
 * c: It appears to have some complications wrt packing & object format
 */

#include <stdlib.h>

typedef struct hs_hash {
	unsigned char bytes[HS_HASH_BYTES];
} hs_hash;

typedef struct hs_hash_string {
	unsigned char bytes[HS_HASH_STRING_BYTES];
} hs_hash;

typedef struct hash_store_entry {
	/* metadata */
	size_t length;
	hs_hash h;
	hs_hash_string hs;

	/* don't touch */
	int fd;
} hash_store_entry;

typedef struct hash_store {
	int root_fd;
	char *path;

	int fd;
	DIR *dir;

	/* hashops */
} hash_store;

int hash_store_open_at(hash_store *hs, int base_fd, char *path);

int hash_store_store(hash_store *hs, void *data, size_t bytes);
int hash_store_store_with_hash(hash_store *hs, void *data, size_t data_bytes, void *hash, size_t hash_len);

int hash_store_lookup(hash_store *hs, hash_store_entry *e);
int hash_store_entry_retrieve(hash_store_entry *e, void *data);

#endif
