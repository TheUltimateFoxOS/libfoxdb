#include <foxdb.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void* foxdb_new() {
	foxdb_file_header_t* header = (foxdb_file_header_t*) malloc(sizeof(foxdb_file_header_t));

	header->magic = FOXDB_MAGIC;
	header->size = sizeof(foxdb_file_header_t);

	return (void*) header;
}

void foxdb_del(void* foxdb) {
	free(foxdb);
}

void* foxdb_insert(void* foxdb, foxdb_entry_t* entry) {
	assert(foxdb_get(foxdb, entry->key) == NULL);

	foxdb_file_header_t* header = (foxdb_file_header_t*) foxdb;

	header = (foxdb_file_header_t*) realloc(header, header->size + entry->size);
	
	memcpy(&((uint8_t*) header)[header->size], entry, entry->size);

	header->size += entry->size;

	return (void*) header;
}

foxdb_entry_t* foxdb_copy_entry(foxdb_entry_t* entry) {
	foxdb_entry_t* new_entry = (foxdb_entry_t*) malloc(entry->size);
	memcpy(new_entry, entry, entry->size);
	return new_entry;
}

void* foxdb_remove(void* foxdb, const char* name) {
	foxdb_file_header_t* header = (foxdb_file_header_t*) foxdb;

	uint64_t curr_offset = sizeof(foxdb_file_header_t);
	
	while (curr_offset < header->size) {
		foxdb_entry_t* curr = (foxdb_entry_t*) &((uint8_t*) header)[curr_offset];
		if (strncmp(curr->key, name, FOXDB_KEY_MAX) == 0) {
			header->size -= curr->size;
			memmove(curr, &((uint8_t*) curr)[curr->size], header->size - curr_offset);
			return realloc(header, header->size);
		}

		curr_offset += curr->size;
	}

	return NULL;
}

foxdb_entry_t* foxdb_get(void* foxdb, const char* name) {
	// returns copy of struct. NEEDS TO BE FREED WITH A CALL TO foxdb_del_entry

	foxdb_file_header_t* header = (foxdb_file_header_t*) foxdb;

	uint64_t curr_offset = sizeof(foxdb_file_header_t);
	
	while (curr_offset < header->size) {
		foxdb_entry_t* curr = (foxdb_entry_t*) &((uint8_t*) header)[curr_offset];
		if (strncmp(curr->key, name, FOXDB_KEY_MAX) == 0) {
			return foxdb_copy_entry(curr);
		}

		curr_offset += curr->size;
	}

	return NULL;
}

void foxdb_iterate(void* foxdb, foxdb_key_iterator it)  {
	foxdb_file_header_t* header = (foxdb_file_header_t*) foxdb;

	uint64_t curr_offset = sizeof(foxdb_file_header_t);
	
	while (curr_offset < header->size) {
		foxdb_entry_t* curr = (foxdb_entry_t*) &((uint8_t*) header)[curr_offset];
		it(curr, curr_offset);
		curr_offset += curr->size;
	}
}

#define FOXDB_GET_TYPED(type_n, fname, type_id) type_n* fname(void* foxdb, const char* name) { \
	foxdb_entry_t* e = foxdb_get(foxdb, name); \
	assert(e && e->type == type_id); \
	return (type_n*) e; \
}

FOXDB_GET_TYPED(foxdb_int_t, foxdb_get_int, FOXDB_INT);
FOXDB_GET_TYPED(foxdb_bool_t, foxdb_get_bool, FOXDB_BOOL);
FOXDB_GET_TYPED(foxdb_str_t, foxdb_get_str, FOXDB_STR);
FOXDB_GET_TYPED(foxdb_float_t, foxdb_get_float, FOXDB_FLOAT);
FOXDB_GET_TYPED(foxdb_bin_t, foxdb_get_bin, FOXDB_BIN);

void foxdb_del_entry(foxdb_entry_t* entry) {
	free(entry);
}

foxdb_int_t* foxdb_int(const char* name, int value) {
	foxdb_int_t* e = (foxdb_int_t*) malloc(sizeof(foxdb_int_t));

	strncpy(e->header.key, name, FOXDB_KEY_MAX);
	e->header.size = sizeof(foxdb_int_t);
	e->header.type = FOXDB_INT;
	e->val = value;

	return e;
}

foxdb_bool_t* foxdb_bool(const char* name, bool value) {
	foxdb_bool_t* e = (foxdb_bool_t*) malloc(sizeof(foxdb_bool_t));

	strncpy(e->header.key, name, FOXDB_KEY_MAX);
	e->header.size = sizeof(foxdb_bool_t);
	e->header.type = FOXDB_BOOL;
	e->val = value;

	return e;
}

foxdb_str_t* foxdb_str(const char* name, const char* value) {
	int val_len = strlen(value);
	foxdb_str_t* e = (foxdb_str_t*) malloc(sizeof(foxdb_str_t) + val_len + 1);

	strncpy(e->header.key, name, FOXDB_KEY_MAX);
	e->header.size = sizeof(foxdb_str_t) + val_len + 1;
	e->header.type = FOXDB_STR;
	
	memset(e->val, 0, val_len + 1);
	memcpy(e->val, value, val_len);

	return e;
}

foxdb_float_t* foxdb_float(const char* name, float value) {
	foxdb_float_t* e = (foxdb_float_t*) malloc(sizeof(foxdb_float_t));

	strncpy(e->header.key, name, FOXDB_KEY_MAX);
	e->header.size = sizeof(foxdb_float_t);
	e->header.type = FOXDB_FLOAT;
	e->val = value;

	return e;
}

foxdb_bin_t* foxdb_bin(const char* name, uint64_t cid, uint64_t size, uint8_t* value) {
	foxdb_bin_t* e = (foxdb_bin_t*) malloc(sizeof(foxdb_bin_t) + size);

	strncpy(e->header.key, name, FOXDB_KEY_MAX);
	e->header.size = sizeof(foxdb_bin_t) + size;
	e->header.type = FOXDB_BIN;
	
	e->cid = cid;
	e->size = size;

	memcpy(e->val, value, size);

	return e;
}

void* foxdb_update(void* foxdb, foxdb_entry_t* new_entry) {
	foxdb_entry_t* old = foxdb_get(foxdb, new_entry->key);
	assert(old != NULL);
	foxdb_del_entry(old);

	foxdb = foxdb_remove(foxdb, new_entry->key);
	foxdb = foxdb_insert(foxdb, new_entry);

	return foxdb;
}

void* foxdb_from_file(FILE* db) {
	fseek(db, 0, SEEK_END);
	int s = ftell(db);
	fseek(db, 0, SEEK_SET);

	void* buf = malloc(s);
	fread(buf, 1, s, db);

	foxdb_file_header_t* header = (foxdb_file_header_t*) buf;
	assert(header->magic == FOXDB_MAGIC);

	return buf;
}

void foxdb_to_file(void* foxdb, FILE* db) {
	foxdb_file_header_t* header = (foxdb_file_header_t*) foxdb;

	fseek(db, 0, SEEK_SET);
	fwrite(foxdb, 1, header->size, db);
	fflush(db);
}