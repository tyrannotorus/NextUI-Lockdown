#define _GNU_SOURCE // for strcasestr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <sys/time.h>
#include "defines.h"
#include "utils.h"

///////////////////////////////////////

int prefixMatch(char* pre, char* str) {
	return (strncasecmp(pre,str,strlen(pre))==0);
}
int suffixMatch(char* suf, char* str) {
	int len = strlen(suf);
	int offset = strlen(str)-len;
	return (offset>=0 && strncasecmp(suf, str+offset, len)==0);
}
int exactMatch(char* str1, char* str2) {
	if (!str1 || !str2) return 0; // NULL isn't safe here
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}
int containsString(char* haystack, char* needle) {
	return strcasestr(haystack, needle) != NULL;
}
int hide(char* file_name) {
	return file_name[0]=='.' || suffixMatch(".disabled", file_name) || exactMatch("map.txt", file_name);
}
char *splitString(char *str, const char *delim)
{
    char *p = strstr(str, delim);
    if (p == NULL)
        return NULL;          // delimiter not found
    *p = '\0';                // terminate string after head
    return p + strlen(delim); // return tail substring
}
// TODO: verify this yields the same result as the one in minui.c, remove one
char *replaceString2(char *orig, char *rep, char *with)
{
    char *ins;     // the next insert point
    char *tmp;     // varies
    int len_rep;   // length of rep (the string to remove)
    int len_with;  // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;     // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count)
        ins = tmp + len_rep;

    char *result =
        (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    tmp = result;

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}
// Stores the trimmed input string into the given output buffer, which must be
// large enough to store the result.  If it is too small, the output is
// truncated.
size_t trimString(char *out, size_t len, const char *str, bool first)
{
    if (len == 0)
        return 0;

    const char *end;
    size_t out_size;
    bool is_string = false;

    // Trim leading space
    while (strchr("\r\n\t {},", (unsigned char)*str) != NULL)
        str++;

    end = str + 1;

    if ((unsigned char)*str == '"') {
        is_string = true;
        str++;
        while (strchr("\r\n\"", (unsigned char)*end) == NULL)
            end++;
    }

    if (*str == 0) // All spaces?
    {
        *out = 0;
        return 1;
    }

    // Trim trailing space
    if (first)
        while (strchr("\r\n\t {},", (unsigned char)*end) == NULL)
            end++;
    else {
        end = str + strlen(str) - 1;
        while (end > str && strchr("\r\n\t {},", (unsigned char)*end) != NULL)
            end--;
        end++;
    }

    if (is_string && (unsigned char)*(end - 1) == '"')
        end--;

    // Set output size to minimum of trimmed string length and buffer size minus
    // 1
    out_size = (end - str) < len - 1 ? (end - str) : len - 1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

void removeParentheses(char *str_out, const char *str_in)
{
    char temp[STR_MAX];
    int len = strlen(str_in);
    int c = 0;
    bool inside = false;
    char end_char;

    for (int i = 0; i < len && i < STR_MAX; i++) {
        if (!inside && (str_in[i] == '(' || str_in[i] == '[')) {
            end_char = str_in[i] == '(' ? ')' : ']';
            inside = true;
            continue;
        }
        else if (inside) {
            if (str_in[i] == end_char)
                inside = false;
            continue;
        }
        temp[c++] = str_in[i];
    }

    temp[c] = '\0';

    trimString(str_out, STR_MAX - 1, temp, false);
}
void serializeTime(char *dest_str, int nTime)
{
    if (nTime >= 60) {
        int h = nTime / 3600;
        int m = (nTime - 3600 * h) / 60;
        if (h > 0) {
            sprintf(dest_str, "%dh %dm", h, m);
        }
        else {
            sprintf(dest_str, "%dm %ds", m, nTime - 60 * m);
        }
    }
    else {
        sprintf(dest_str, "%ds", nTime);
    }
}
int countChar(const char *str, char ch)
{
    int i, count = 0;
    for (i = 0; i <= strlen(str); i++) {
        if (str[i] == ch) {
            count++;
        }
    }
    return count;
}
char *removeExtension(const char *myStr)
{
    if (myStr == NULL)
        return NULL;
    char *retStr = (char *)malloc(strlen(myStr) + 1);
    char *lastExt;
    if (retStr == NULL)
        return NULL;
    strcpy(retStr, myStr);
    if ((lastExt = strrchr(retStr, '.')) != NULL && *(lastExt + 1) != ' ' && *(lastExt + 2) != '\0')
        *lastExt = '\0';
    return retStr;
}
const char *baseName(const char *filename)
{
    char *p = strrchr(filename, '/');
    return p ? p + 1 : (char *)filename;
}
void folderPath(const char *path, char *result) {
    char pathCopy[256];  
    strcpy(pathCopy, path);

    char *lastSlash = strrchr(pathCopy, '/');  // Find the last slash
    if (lastSlash != NULL) {
        *lastSlash = '\0';  // Cut off the filename
        strcpy(result, pathCopy);  // Copy the remaining path
    } else {
        strcpy(result, "");  // No folder found
    }
}
void cleanName(char *name_out, const char *file_name)
{
    char *name_without_ext = removeExtension(file_name);
    char *no_underscores = replaceString2(name_without_ext, "_", " ");
    char *dot_ptr = strstr(no_underscores, ".");
    if (dot_ptr != NULL) {
        char *s = no_underscores;
        while (isdigit(*s) && s < dot_ptr)
            s++;
        if (s != dot_ptr)
            dot_ptr = no_underscores;
        else {
            dot_ptr++;
            if (dot_ptr[0] == ' ')
                dot_ptr++;
        }
    }
    else {
        dot_ptr = no_underscores;
    }
    removeParentheses(name_out, dot_ptr);
    free(name_without_ext);
    free(no_underscores);
}
bool pathRelativeTo(char *path_out, const char *dir_from, const char *file_to)
{
    path_out[0] = '\0';

    char abs_from[MAX_PATH];
    char abs_to[MAX_PATH];
    if (realpath(dir_from, abs_from) == NULL || realpath(file_to, abs_to) == NULL) {
        return false;
    }

    char *p1 = abs_from;
    char *p2 = abs_to;
    while (*p1 && (*p1 == *p2)) {
        ++p1, ++p2;
    }

    if (*p2 == '/') {
        ++p2;
    }

    if (strlen(p1) > 0) {
        int num_parens = countChar(p1, '/') + 1;
        for (int i = 0; i < num_parens; i++) {
            strcat(path_out, "../");
        }
    }
    strcat(path_out, p2);

    return true;
}

void getDisplayName(const char* in_name, char* out_name) {
	char* tmp;
	char work_name[256];
	strcpy(work_name, in_name);
	strcpy(out_name, in_name);
	
	if (suffixMatch("/" PLATFORM, work_name)) { // hide platform from Tools path...
		tmp = strrchr(work_name, '/');
		tmp[0] = '\0';
	}
	
	// extract just the filename if necessary
	tmp = strrchr(work_name, '/');
	if (tmp) strcpy(out_name, tmp+1);
	
	// remove extension(s), eg. .p8.png
	while ((tmp = strrchr(out_name, '.'))!=NULL) {
		int len = strlen(tmp);
		if (len>2 && len<=5) tmp[0] = '\0'; // 1-4 letter extension plus dot (was 1-3, extended for .doom files)
		else break;
	}
	
	// remove trailing parens (round and square)
	strcpy(work_name, out_name);
	while ((tmp=strrchr(out_name, '('))!=NULL || (tmp=strrchr(out_name, '['))!=NULL) {
		if (tmp==out_name) break;
		tmp[0] = '\0';
		tmp = out_name;
	}
	
	// make sure we haven't nuked the entire name
	if (out_name[0]=='\0') strcpy(out_name, work_name);
	
	// remove trailing whitespace
	tmp = out_name + strlen(out_name) - 1;
    while(tmp>out_name && isspace((unsigned char)*tmp)) tmp--;
    tmp[1] = '\0';
}
void getEmuName(const char* in_name, char* out_name) { // NOTE: both char arrays need to be MAX_PATH length!
	char* tmp;
	strcpy(out_name, in_name);
	tmp = out_name;
	
	// printf("--------\n  in_name: %s\n",in_name); fflush(stdout);
	
	// extract just the Roms folder name if necessary
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2) tmp2[0] = '\0';
		// printf("    tmp1: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = out_name;
	}

	// finally extract pak name from parenths if present
	tmp = strrchr(tmp, '(');
	if (tmp) {
		tmp += 1;
		// printf("    tmp2: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = strchr(out_name,')');
		tmp[0] = '\0';
	}
	
	// printf(" out_name: %s\n", out_name); fflush(stdout);
}
void getEmuPath(char* emu_name, char* pak_path) {
	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	if (exists(pak_path)) return;
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
}

void normalizeNewline(char* line) {
	int len = strlen(line);
	if (len>1 && line[len-1]=='\n' && line[len-2]=='\r') { // windows!
		line[len-2] = '\n';
		line[len-1] = '\0';
	}
}
void trimTrailingNewlines(char* line) {
	int len = strlen(line);
	while (len>0 && line[len-1]=='\n') {
		line[len-1] = '\0'; // trim newline
		len -= 1;
	}
}
void trimSortingMeta(char** str) { // eg. `001) `
	// TODO: this code is suss
	char* safe = *str;
	while(isdigit(**str)) *str += 1; // ignore leading numbers

	if (*str[0]==')') { // then match a closing parenthesis
		*str += 1;
	}
	else { //  or bail, restoring the string to its original value
		*str = safe;
		return;
	}
	
	while(isblank(**str)) *str += 1; // ignore leading space
}

///////////////////////////////////////

int exists(char* path) {
	return access(path, F_OK)==0;
}
void touch(char* path) {
	close(open(path, O_RDWR|O_CREAT, 0777));
}
void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}
void getFile(char* path, char* buffer, size_t buffer_size) {
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size>buffer_size-1) size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}
char* allocFile(char* path) { // caller must free!
	char* contents = NULL;
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		contents = calloc(size+1, sizeof(char));
		fseek(file, 0L, SEEK_SET);
		fread(contents, sizeof(char), size, file);
		fclose(file);
		contents[size] = '\0';
	}
	return contents;
}
int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

uint64_t getMicroseconds(void) {
    uint64_t ret;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    ret = (uint64_t)tv.tv_sec * 1000000;
    ret += (uint64_t)tv.tv_usec;

    return ret;
}

inline Array* Array_new(void) {
    Array* self = malloc(sizeof(Array));
    if (!self) return NULL;  // Allocation failure
    
    self->count = 0;
    self->capacity = 8;
    self->items = malloc(sizeof(void*) * self->capacity);
    
    if (!self->items) {  // Allocation failure
        free(self);
        return NULL;
    }
    
    return self;
}

inline void Array_push(Array* self, void* item) {
    if (!self) return;  // Null pointer check
    
    if (self->count >= self->capacity) {
        int new_capacity = self->capacity * 2;
        void** new_items = realloc(self->items, sizeof(void*) * new_capacity);
        if (!new_items) return;  // Allocation failure
        self->items = new_items;
        self->capacity = new_capacity;
    }
    self->items[self->count++] = item;
}

inline void Array_unshift(Array* self, void* item) {
    if (!self) return;  // Null pointer check
    
    if (self->count == 0) {
        Array_push(self, item);
        return;
    }
    
    // Ensure capacity before shifting
    if (self->count >= self->capacity) {
        int new_capacity = self->capacity * 2;
        void** new_items = realloc(self->items, sizeof(void*) * new_capacity);
        if (!new_items) return;  // Allocation failure
        self->items = new_items;
        self->capacity = new_capacity;
    }
    
    // Shift elements in one operation using memmove
    memmove(&self->items[1], self->items, sizeof(void*) * self->count);
    self->items[0] = item;
    self->count++;
}

inline void* Array_pop(Array* self) {
    if (!self || self->count == 0) return NULL;
    return self->items[--self->count];
}

inline void Array_reverse(Array* self) {
    if (!self || self->count <= 1) return;  // Early return for empty or single-item arrays
    
    void** start = self->items;
    void** end = self->items + self->count - 1;
    
    // Optimization: Use pointer arithmetic instead of array indexing
    while (start < end) {
        void* temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

inline void Array_free(Array* self) {
    if (!self) return;  // Null pointer check
    
    free(self->items);
    free(self);
}

// Assumes exactMatch is an existing function that compares strings
inline int StringArray_indexOf(Array* self, char* str) {
    if (!self || !str) return -1;  // Null pointer check
    
    for (int i = 0; i < self->count; i++) {
        if (exactMatch(self->items[i], str)) return i;
    }
    return -1;
}

inline void StringArray_free(Array* self) {
    if (!self) return;  // Null pointer check
    
    for (int i = 0; i < self->count; i++) {
        free(self->items[i]);
    }
    Array_free(self);
}

inline Hash* Hash_new(void) {
    Hash* self = malloc(sizeof(Hash));
    if (!self) return NULL;  // Allocation failure
    
    self->keys = Array_new();
    self->values = Array_new();
    
    // Add hash codes array for O(1) lookups
    if (!self->keys || !self->values) {
        if (self->keys) Array_free(self->keys);
        if (self->values) Array_free(self->values);
        free(self);
        return NULL;
    }
    
    return self;
}

inline void Hash_free(Hash* self) {
    if (!self) return;  // Null pointer check
    
    StringArray_free(self->keys);
    StringArray_free(self->values);
    free(self);
}

inline void Hash_set(Hash* self, char* key, char* value) {
    if (!self || !key || !value) return;  // Null pointer check
    
    // Check if key already exists to avoid duplicates
    int index = StringArray_indexOf(self->keys, key);
    if (index != -1) {
        // Replace existing value
        free(self->values->items[index]);
        self->values->items[index] = strdup(value);
        return;
    }
    
    Array_push(self->keys, strdup(key));
    Array_push(self->values, strdup(value));
}

inline char* Hash_get(Hash* self, char* key) {
    if (!self || !key) return NULL;  // Null pointer check
    
    int i = StringArray_indexOf(self->keys, key);
    if (i == -1) return NULL;
    return self->values->items[i];
}

inline Entry* Entry_new(char* path, int type) {
    if (!path) return NULL;  // Null pointer check
    
    char display_name[256];
    getDisplayName(path, display_name);
    
    Entry* self = malloc(sizeof(Entry));
    if (!self) return NULL;  // Allocation failure
    
    self->path = strdup(path);
    self->name = strdup(display_name);
    
    // Check for allocation failures
    if (!self->path || !self->name) {
        Entry_free(self);
        return NULL;
    }
    
    self->unique = NULL;
    self->type = type;
    self->alpha = 0;
    
    return self;
}

inline void Entry_free(Entry* self) {
    if (!self) return;  // Null pointer check
    
    free(self->path);
    free(self->name);
    if (self->unique) free(self->unique);
    free(self);
}

inline int EntryArray_indexOf(Array* self, char* path) {
    if (!self || !path) return -1;  // Null pointer check
    
    for (int i = 0; i < self->count; i++) {
        Entry* entry = self->items[i];
        if (exactMatch(entry->path, path)) return i;
    }
    return -1;
}

inline int EntryArray_sortEntry(const void* a, const void* b) {
    if (!a || !b) return 0;  // Null pointer check
    
    Entry* item1 = *(Entry**)a;
    Entry* item2 = *(Entry**)b;
    
    // First sort by type, then by name
    if (item1->type != item2->type)
        return item1->type - item2->type;
    
    return strcasecmp(item1->name, item2->name);
}

inline void EntryArray_sort(Array* self) {
    if (!self || self->count <= 1) return;  // Early return for empty or single-item arrays
    
    qsort(self->items, self->count, sizeof(void*), EntryArray_sortEntry);
}

inline void EntryArray_free(Array* self) {
    if (!self) return;  // Null pointer check
    
    for (int i = 0; i < self->count; i++) {
        Entry_free(self->items[i]);
    }
    Array_free(self);
}