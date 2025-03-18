#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(char* str1, char* str2);
int containsString(char* haystack, char* needle);
int hide(char* file_name);

char *splitString(char *str, const char *delim);
char *replaceString2(const char *orig, char *rep, char *with);
size_t trimString(char *out, size_t len, const char *str, bool first);
void removeParentheses(char *str_out, const char *str_in);
void serializeTime(char *dest_str, int nTime);
int countChar(const char *str, char ch);
char *removeExtension(const char *myStr);
const char *baseName(const char *filename);
void folderPath(const char *filePath, char *folder_path);
void cleanName(char *name_out, const char *file_name);
bool pathRelativeTo(char *path_out, const char *dir_from, const char *file_to);

void getDisplayName(const char* in_name, char* out_name);
void getEmuName(const char* in_name, char* out_name);
void getEmuPath(char* emu_name, char* pak_path);

void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);

int exists(char* path);
void touch(char* path);
void putFile(char* path, char* contents);
char* allocFile(char* path); // caller must free
void getFile(char* path, char* buffer, size_t buffer_size);
void putInt(char* path, int value);
int getInt(char* path);

uint64_t getMicroseconds(void);
int clamp(int x, int lower, int upper);

enum EntryType {
	ENTRY_DIR,
	ENTRY_PAK,
	ENTRY_ROM,
};

typedef struct Array {
	int count;
	int capacity;
	void** items;
} Array;

typedef struct Hash {
	Array* keys;
	Array* values;
} Hash; // not really a hash

typedef struct Entry {
	char* path;
	char* name;
	char* unique;
	int type;
	int alpha;
} Entry;

Array* Array_new(void);
void Array_push(Array* self, void* item);
void Array_unshift(Array* self, void* item);
void* Array_pop(Array* self);
void Array_reverse(Array* self);
void Array_free(Array* self);
int StringArray_indexOf(Array* self, char* str);
void StringArray_free(Array* self);
Hash* Hash_new(void);
void Hash_free(Hash* self);
void Hash_set(Hash* self, char* key, char* value);
char* Hash_get(Hash* self, char* key);
Entry* Entry_new(char* path, int type);
void Entry_free(Entry* self);
int EntryArray_indexOf(Array* self, char* path);
int EntryArray_sortEntry(const void* a, const void* b);
void EntryArray_sort(Array* self);
void EntryArray_free(Array* self);

#endif
