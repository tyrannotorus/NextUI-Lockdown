#include <stdio.h>
#include <stdlib.h>
#include <msettings.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>  // For dirname()
#include "defines.h"
#include "api.h"
#include "utils.h"
#include "konamicode.h"

#define INT_ARRAY_MAX 27
typedef struct IntArray {
	int count;
	int items[INT_ARRAY_MAX];
} IntArray;
static IntArray* IntArray_new(void) {
	IntArray* self = malloc(sizeof(IntArray));
	self->count = 0;
	memset(self->items, 0, sizeof(int) * INT_ARRAY_MAX);
	return self;
}
static void IntArray_push(IntArray* self, int i) {
	self->items[self->count++] = i;
}
static void IntArray_free(IntArray* self) {
	free(self);
}

///////////////////////////////////////

typedef struct Directory {
	char* path;
	char* name;
	Array* entries;
	IntArray* alphas;
	// rendering
	int selected;
	int start;
	int end;
} Directory;

static int getIndexChar(char* str) {
	char i = 0;
	char c = tolower(str[0]);
	if (c>='a' && c<='z') i = (c-'a')+1;
	return i;
}

static void getUniqueName(Entry* entry, char* out_name) {
	char* filename = strrchr(entry->path, '/')+1;
	char emu_tag[256];
	getEmuName(entry->path, emu_tag);
	
	char *tmp;
	strcpy(out_name, entry->name);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, " (");
	tmp = out_name + strlen(out_name);
	strcpy(tmp, emu_tag);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, ")");
}

static void Directory_index(Directory* self) {
    int is_collection = prefixMatch(COLLECTIONS_PATH, self->path);
    int skip_index = exactMatch(FAUX_RECENT_PATH, self->path) || is_collection; // not alphabetized
    
    Hash* map = NULL;
    char map_path[256];
    sprintf(map_path, "%s/map.txt", is_collection ? COLLECTIONS_PATH : self->path);

    if (exists(map_path)) {
        FILE* file = fopen(map_path, "r");
        if (file) {
            map = Hash_new();
            char line[256];
            while (fgets(line, 256, file) != NULL) {
                normalizeNewline(line);
                trimTrailingNewlines(line);
                if (strlen(line) == 0) continue; // skip empty lines

                char* tmp = strchr(line, '\t');
                if (tmp) {
                    tmp[0] = '\0';
                    char* key = line;
                    char* value = tmp + 1;
                    Hash_set(map, key, strdup(value)); // Ensure strdup to store value properly
                }
            }
            fclose(file);
            
            int resort = 0;
            int filter = 0;
            for (int i = 0; i < self->entries->count; i++) {
                Entry* entry = self->entries->items[i];
                char* filename = strrchr(entry->path, '/') + 1;
                char* alias = Hash_get(map, filename);
                if (alias) {
                    free(entry->name);  // Free before overwriting
                    entry->name = strdup(alias);
                    resort = 1;
                    if (!filter && hide(entry->name)) filter = 1;
                }
            }
            
            if (filter) {
                Array* entries = Array_new();
                for (int i = 0; i < self->entries->count; i++) {
                    Entry* entry = self->entries->items[i];
                    if (hide(entry->name)) {
                        Entry_free(entry); // Ensure Entry_free handles all memory cleanup
                    } else {
                        Array_push(entries, entry);
                    }
                }
                Array_free(self->entries);
                self->entries = entries;
            }
            if (resort) EntryArray_sort(self->entries);
        }
    }
    
    Entry* prior = NULL;
    int alpha = -1;
    int index = 0;
    for (int i = 0; i < self->entries->count; i++) {
        Entry* entry = self->entries->items[i];
        if (map) {
            char* filename = strrchr(entry->path, '/') + 1;
            char* alias = Hash_get(map, filename);
            if (alias) {
                free(entry->name);  // Free before overwriting
                entry->name = strdup(alias);
            }
        }
        
        if (prior != NULL && exactMatch(prior->name, entry->name)) {
            free(prior->unique);
            free(entry->unique);
            prior->unique = NULL;
            entry->unique = NULL;

            char* prior_filename = strrchr(prior->path, '/') + 1;
            char* entry_filename = strrchr(entry->path, '/') + 1;
            if (exactMatch(prior_filename, entry_filename)) {
                char prior_unique[256];
                char entry_unique[256];
                getUniqueName(prior, prior_unique);
                getUniqueName(entry, entry_unique);

                prior->unique = strdup(prior_unique);
                entry->unique = strdup(entry_unique);
            } else {
                prior->unique = strdup(prior_filename);
                entry->unique = strdup(entry_filename);
            }
        }

        if (!skip_index) {
            int a = getIndexChar(entry->name);
            if (a != alpha) {
                index = self->alphas->count;
                IntArray_push(self->alphas, i);
                alpha = a;
            }
            entry->alpha = index;
        }
        
        prior = entry;
    }

    if (map) Hash_free(map);  // Free the map at the end
}


static Array* getRoot(void);
static Array* getRecents(void);
static Array* getCollection(char* path);
static Array* getDiscs(char* path);
static Array* getEntries(char* path);

static Directory* Directory_new(char* path, int selected) {
	char display_name[256];
	getDisplayName(path, display_name);
	
	Directory* self = malloc(sizeof(Directory));
	self->path = strdup(path);
	self->name = strdup(display_name);
	if (exactMatch(path, SDCARD_PATH)) {
		self->entries = getRoot();
	}
	else if (exactMatch(path, FAUX_RECENT_PATH)) {
		self->entries = getRecents();
	}
	else if (!exactMatch(path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, path) && suffixMatch(".txt", path)) {
		self->entries = getCollection(path);
	}
	else if (suffixMatch(".m3u", path)) {
		self->entries = getDiscs(path);
	}
	else {
		self->entries = getEntries(path);
	}
	self->alphas = IntArray_new();
	self->selected = selected;
	Directory_index(self);
	return self;
}

static void Directory_free(Directory* self) {
	free(self->path);
	free(self->name);
	EntryArray_free(self->entries);
	IntArray_free(self->alphas);
	free(self);
}

static void DirectoryArray_pop(Array* self) {
	Directory_free(Array_pop(self));
}
static void DirectoryArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Directory_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////////////

typedef struct Recent {
	char* path; // NOTE: this is without the SDCARD_PATH prefix!
	char* alias;
	int available;
} Recent;
 // yiiikes
static char* recent_alias = NULL;

static int hasEmu(char* emu_name);
static Recent* Recent_new(char* path, char* alias) {
	Recent* self = malloc(sizeof(Recent));

	char sd_path[256]; // only need to get emu name
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);

	char emu_name[256];
	getEmuName(sd_path, emu_name);
	
	self->path = strdup(path);
	self->alias = alias ? strdup(alias) : NULL;
	self->available = hasEmu(emu_name);
	return self;
}
static void Recent_free(Recent* self) {
	free(self->path);
	if (self->alias) free(self->alias);
	free(self);
}

static int RecentArray_indexOf(Array* self, char* str) {
	for (int i=0; i<self->count; i++) {
		Recent* item = self->items[i];
		if (exactMatch(item->path, str)) return i;
	}
	return -1;
}
static void RecentArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Recent_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////////////

static Directory* top;
static Array* stack; // DirectoryArray
static Array* recents; // RecentArray

static int quit = 0;
static int can_resume = 0;
static int should_resume = 0; // set to 1 on BTN_RESUME but only if can_resume==1
static int has_preview = 0;
static int show_switcher_screen = 0;
static int show_version_screen = 0;
static int simple_mode = 0;
static int switcher_selected = 0;
static char slot_path[256];
static char preview_path[256];

static int restore_depth = -1;
static int restore_relative = -1;
static int restore_selected = 0;
static int restore_start = 0;
static int restore_end = 0;

///////////////////////////////////////

#define MAX_RECENTS 24 // a multiple of all menu rows
static void saveRecents(void) {
	FILE* file = fopen(RECENT_PATH, "w");
	if (file) {
		for (int i=0; i<recents->count; i++) {
			Recent* recent = recents->items[i];
			fputs(recent->path, file);
			if (recent->alias) {
				fputs("\t", file);
				fputs(recent->alias, file);
			}
			putc('\n', file);
		}
		fclose(file);
	}
}
static void addRecent(char* path, char* alias) {
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	int id = RecentArray_indexOf(recents, path);
	if (id==-1) { // add
		while (recents->count>=MAX_RECENTS) {
			Recent_free(Array_pop(recents));
		}
		Array_unshift(recents, Recent_new(path, alias));
	}
	else if (id>0) { // bump to top
		for (int i=id; i>0; i--) {
			void* tmp = recents->items[i-1];
			recents->items[i-1] = recents->items[i];
			recents->items[i] = tmp;
		}
	}
	saveRecents();
}

static int hasEmu(char* emu_name) {
	char pak_path[256];
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
	if (exists(pak_path)) return 1;

	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	return exists(pak_path);
}
static int hasCue(char* dir_path, char* cue_path) { // NOTE: dir_path not rom_path
	char* tmp = strrchr(dir_path, '/') + 1; // folder name
	sprintf(cue_path, "%s/%s.cue", dir_path, tmp);
	return exists(cue_path);
}
static int hasM3u(char* rom_path, char* m3u_path) { // NOTE: rom_path not dir_path
	char* tmp;
	
	strcpy(m3u_path, rom_path);
	tmp = strrchr(m3u_path, '/') + 1;
	tmp[0] = '\0';
	
	// path to parent directory
	char base_path[256];
	strcpy(base_path, m3u_path);
	
	tmp = strrchr(m3u_path, '/');
	tmp[0] = '\0';
	
	// get parent directory name
	char dir_name[256];
	tmp = strrchr(m3u_path, '/');
	strcpy(dir_name, tmp);
	
	// dir_name is also our m3u file name
	tmp = m3u_path + strlen(m3u_path); 
	strcpy(tmp, dir_name);

	// add extension
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");
	
	return exists(m3u_path);
}

static int hasRecents(void) {
	LOG_info("hasRecents %s\n", RECENT_PATH);
	int has = 0;
	
	Array* parent_paths = Array_new();
	if (exists(CHANGE_DISC_PATH)) {
		char sd_path[256];
		getFile(CHANGE_DISC_PATH, sd_path, 256);
		if (exists(sd_path)) {
			char* disc_path = sd_path + strlen(SDCARD_PATH); // makes path platform agnostic
			Recent* recent = Recent_new(disc_path, NULL);
			if (recent->available) has += 1;
			Array_push(recents, recent);
		
			char parent_path[256];
			strcpy(parent_path, disc_path);
			char* tmp = strrchr(parent_path, '/') + 1;
			tmp[0] = '\0';
			Array_push(parent_paths, strdup(parent_path));
		}
		unlink(CHANGE_DISC_PATH);
	}

	FILE *file = fopen(RECENT_PATH, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			// LOG_info("line: %s\n", line);
			
			char* path = line;
			char* alias = NULL;
			char* tmp = strchr(line,'\t');
			if (tmp) {
				tmp[0] = '\0';
				alias = tmp+1;
			}
			
			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, path);
			if (exists(sd_path)) {
				if (recents->count<MAX_RECENTS) {
					// this logic replaces an existing disc from a multi-disc game with the last used
					char m3u_path[256];
					if (hasM3u(sd_path, m3u_path)) { // TODO: this might tank launch speed
						char parent_path[256];
						strcpy(parent_path, path);
						char* tmp = strrchr(parent_path, '/') + 1;
						tmp[0] = '\0';
						
						int found = 0;
						for (int i=0; i<parent_paths->count; i++) {
							char* path = parent_paths->items[i];
							if (prefixMatch(path, parent_path)) {
								found = 1;
								break;
							}
						}
						if (found) continue;
						
						Array_push(parent_paths, strdup(parent_path));
					}
					
					// LOG_info("path:%s alias:%s\n", path, alias);
					
					Recent* recent = Recent_new(path, alias);
					if (recent->available) has += 1;
					Array_push(recents, recent);
				}
			}
		}
		fclose(file);
	}
	
	saveRecents();
	
	StringArray_free(parent_paths);
	return has>0;
}
static int hasCollections(void) {
	int has = 0;
	if (!exists(COLLECTIONS_PATH)) return has;
	
	DIR *dh = opendir(COLLECTIONS_PATH);
	struct dirent *dp;
	while((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name)) continue;
		has = 1;
		break;
	}
	closedir(dh);
	return has;
}
static int hasRoms(char* dir_name) {
	int has = 0;
	char emu_name[256];
	char rom_path[256];

	getEmuName(dir_name, emu_name);
	
	// check for emu pak
	if (!hasEmu(emu_name)) return has;
	
	// check for at least one non-hidden file (we're going to assume it's a rom)
	sprintf(rom_path, "%s/%s/", ROMS_PATH, dir_name);
	DIR *dh = opendir(rom_path);
	if (dh!=NULL) {
		struct dirent *dp;
		while((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name)) continue;
			has = 1;
			break;
		}
		closedir(dh);
	}
	// if (!has) printf("No roms for %s!\n", dir_name);
	return has;
}
static Array* getRoot(void) {
    Array* root = Array_new();

	// Hide recents in Simple Mode.
    if (!simple_mode && hasRecents()) {
		Array_push(root, Entry_new(FAUX_RECENT_PATH, ENTRY_DIR));
	}

    Array* entries = Array_new();
    DIR* dh = opendir(ROMS_PATH);
    if (dh) {
        struct dirent* dp;
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/", ROMS_PATH);
        char* tmp = full_path + strlen(full_path);

        Array* emus = Array_new();
        while ((dp = readdir(dh)) != NULL) {
            if (hide(dp->d_name)) continue;
            if (hasRoms(dp->d_name)) {
                strcpy(tmp, dp->d_name);
                Array_push(emus, Entry_new(full_path, ENTRY_DIR));
            }
        }
        closedir(dh); // Ensure directory is closed immediately after use

        EntryArray_sort(emus);
        Entry* prev_entry = NULL;
        for (int i = 0; i < emus->count; i++) {
            Entry* entry = emus->items[i];
            if (prev_entry && exactMatch(prev_entry->name, entry->name)) {
                Entry_free(entry);
                continue;
            }
            Array_push(entries, entry);
            prev_entry = entry;
        }
        Array_free(emus); // Only frees container, entries now owns the items
    }

    // Handle mapping logic
    char map_path[256];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", ROMS_PATH);
    if (entries->count > 0 && exists(map_path)) {
        FILE* file = fopen(map_path, "r");
        if (file) {
            Hash* map = Hash_new();
            char line[256];

            while (fgets(line, sizeof(line), file)) {
                normalizeNewline(line);
                trimTrailingNewlines(line);
                if (strlen(line) == 0) continue; // Skip empty lines

                char* tmp = strchr(line, '\t');
                if (tmp) {
                    *tmp = '\0';
                    char* key = line;
                    char* value = tmp + 1;
                    Hash_set(map, key, strdup(value)); // Ensure strdup
                }
            }
            fclose(file);

            int resort = 0;
            for (int i = 0; i < entries->count; i++) {
                Entry* entry = entries->items[i];
                char* filename = strrchr(entry->path, '/') + 1;
                char* alias = Hash_get(map, filename);
                if (alias) {
                    free(entry->name);  // Free before overwriting
                    entry->name = strdup(alias);
                    resort = 1;
                }
            }
            if (resort) EntryArray_sort(entries);
            Hash_free(map);
        }
    }

    // Hide collections in Simple Mode.
    if (!simple_mode && hasCollections()) {
        if (entries->count) {
            Array_push(root, Entry_new(COLLECTIONS_PATH, ENTRY_DIR));
        } else { // No visible systems, promote collections to root
            dh = opendir(COLLECTIONS_PATH);
            if (dh) {
                struct dirent* dp;
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s/", COLLECTIONS_PATH);
                char* tmp = full_path + strlen(full_path);

                Array* collections = Array_new();
                while ((dp = readdir(dh)) != NULL) {
                    if (hide(dp->d_name)) continue;
                    strcpy(tmp, dp->d_name);
                    Array_push(collections, Entry_new(full_path, ENTRY_DIR)); // Collections are fake directories
                }
                closedir(dh); // Close immediately after use
                EntryArray_sort(collections);

                for (int i = 0; i < collections->count; i++) {
                    Array_push(entries, collections->items[i]);
                }
                Array_free(collections); // Only free the container, `entries` owns the items now
            }
        }
    }

    // Move entries to root
    for (int i = 0; i < entries->count; i++) {
        Array_push(root, entries->items[i]);
    }
    Array_free(entries); // `root` now owns the entries

    // Add tools to the menu if:
	// - Simple Mode Mode is off
	// - Tools exist in the folder.
    if(!simple_mode) {
		char tools_path[256];
		snprintf(tools_path, sizeof(tools_path), "%s/Tools/%s", SDCARD_PATH, PLATFORM);
		if (exists(tools_path)) {
			Array_push(root, Entry_new(tools_path, ENTRY_DIR));
		}
	}

    return root;
}

static Entry* entryFromRecent(Recent* recent)
{
	if(!recent || !recent->available)
		return NULL;
	
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, recent->path);
	int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
	Entry* entry = Entry_new(sd_path, type);
	if (recent->alias) {
		free(entry->name);
		entry->name = strdup(recent->alias);
	}
	return entry;
}

static Array* getRecents(void) {
	Array* entries = Array_new();
	for (int i=0; i<recents->count; i++) {
		Recent *recent = recents->items[i];
		Entry *entry = entryFromRecent(recent);
		if(entry)
			Array_push(entries, entry);
	}
	return entries;
}

static Array* getCollection(char* path) {
	Array* entries = Array_new();
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, line);
			if (exists(sd_path)) {
				int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
				Array_push(entries, Entry_new(sd_path, type));
				
				// char emu_name[256];
				// getEmuName(sd_path, emu_name);
				// if (hasEmu(emu_name)) {
					// Array_push(entries, Entry_new(sd_path, ENTRY_ROM));
				// }
			}
		}
		fclose(file);
	}
	return entries;
}
static Array* getDiscs(char* path){
	
	// TODO: does path have SDCARD_PATH prefix?
	
	Array* entries = Array_new();
	
	char base_path[256];
	strcpy(base_path, path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';
	
	// TODO: limit number of discs supported (to 9?)
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		int disc = 0;
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			char disc_path[256];
			sprintf(disc_path, "%s%s", base_path, line);
						
			if (exists(disc_path)) {
				disc += 1;
				Entry* entry = Entry_new(disc_path, ENTRY_ROM);
				free(entry->name);
				char name[16];
				sprintf(name, "Disc %i", disc);
				entry->name = strdup(name);
				Array_push(entries, entry);
			}
		}
		fclose(file);
	}
	return entries;
}
static int getFirstDisc(char* m3u_path, char* disc_path) { // based on getDiscs() natch
	int found = 0;

	char base_path[256];
	strcpy(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';
	
	FILE* file = fopen(m3u_path, "r");
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			sprintf(disc_path, "%s%s", base_path, line);
						
			if (exists(disc_path)) found = 1;
			break;
		}
		fclose(file);
	}
	return found;
}

static void addEntries(Array* entries, char* path) {
	DIR *dh = opendir(path);
	if (dh!=NULL) {
		struct dirent *dp;
		char* tmp;
		char full_path[256];
		sprintf(full_path, "%s/", path);
		tmp = full_path + strlen(full_path);
		while((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name)) continue;
			strcpy(tmp, dp->d_name);
			int is_dir = dp->d_type==DT_DIR;
			int type;
			if (is_dir) {
				// TODO: this should make sure launch.sh exists
				if (suffixMatch(".pak", dp->d_name)) {
					type = ENTRY_PAK;
				}
				else {
					type = ENTRY_DIR;
				}
			}
			else {
				if (prefixMatch(COLLECTIONS_PATH, full_path)) {
					type = ENTRY_DIR; // :shrug:
				}
				else {
					type = ENTRY_ROM;
				}
			}
			Array_push(entries, Entry_new(full_path, type));
		}
		closedir(dh);
	}
}

static int isConsoleDir(char* path) {
	char* tmp;
	char parent_dir[256];
	strcpy(parent_dir, path);
	tmp = strrchr(parent_dir, '/');
	tmp[0] = '\0';
	
	return exactMatch(parent_dir, ROMS_PATH);
}

static Array* getEntries(char* path){
	Array* entries = Array_new();

	// Top-level consoles folder.
	if (isConsoleDir(path)) { 
		char collated_path[256];
		strcpy(collated_path, path);
		char* tmp = strrchr(collated_path, '(');
		// 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
		// but conditional so we can continue to support a bare tag name as a folder name
		if (tmp) tmp[1] = '\0'; 
		
		DIR *dh = opendir(ROMS_PATH);
		if (dh!=NULL) {
			struct dirent *dp;
			char full_path[256];
			sprintf(full_path, "%s/", ROMS_PATH);
			tmp = full_path + strlen(full_path);
			// while loop so we can collate paths, see above
			while((dp = readdir(dh)) != NULL) {
				if (hide(dp->d_name)) continue;
				if (dp->d_type!=DT_DIR) continue;
				strcpy(tmp, dp->d_name);
			
				if (!prefixMatch(collated_path, full_path)) continue;
				addEntries(entries, full_path);
			}
			closedir(dh);
		}
	}
	else addEntries(entries, path); // just a subfolder
	
	EntryArray_sort(entries);
	return entries;
}

///////////////////////////////////////

static void queueNext(char* cmd) {
	LOG_info("cmd: %s\n", cmd);
	putFile("/tmp/next", cmd);
	quit = 1;
}

// based on https://stackoverflow.com/a/31775567/145965
static int replaceString(char *line, const char *search, const char *replace) {
   char *sp; // start of pattern
   if ((sp = strstr(line, search)) == NULL) {
      return 0;
   }
   int count = 1;
   int sLen = strlen(search);
   int rLen = strlen(replace);
   if (sLen > rLen) {
      // move from right to left
      char *src = sp + sLen;
      char *dst = sp + rLen;
      while((*dst = *src) != '\0') { dst++; src++; }
   } else if (sLen < rLen) {
      // move from left to right
      int tLen = strlen(sp) - sLen;
      char *stop = sp + rLen;
      char *src = sp + sLen + tLen;
      char *dst = sp + rLen + tLen;
      while(dst >= stop) { *dst = *src; dst--; src--; }
   }
   memcpy(sp, replace, rLen);
   count += replaceString(sp + rLen, search, replace);
   return count;
}
static char* escapeSingleQuotes(char* str) {
	// why not call replaceString directly?
	// call points require the modified string be returned
	// but replaceString is recursive and depends on its
	// own return value (but does it need to?)
	replaceString(str, "'", "'\\''");
	return str;
}

///////////////////////////////////////

static void readyResumePath(char* rom_path, int type) {
	char* tmp;
	can_resume = 0;
	has_preview = 0;
	char path[256];
	strcpy(path, rom_path);
	
	if (!prefixMatch(ROMS_PATH, path)) return;
	
	char auto_path[256];
	if (type==ENTRY_DIR) {
		if (!hasCue(path, auto_path)) { // no cue?
			tmp = strrchr(auto_path, '.') + 1; // extension
			strcpy(tmp, "m3u"); // replace with m3u
			if (!exists(auto_path)) return; // no m3u
		}
		strcpy(path, auto_path); // cue or m3u if one exists
	}
	
	if (!suffixMatch(".m3u", path)) {
		char m3u_path[256];
		if (hasM3u(path, m3u_path)) {
			// change path to m3u path
			strcpy(path, m3u_path);
		}
	}
	
	char emu_name[256];
	getEmuName(path, emu_name);
	
	char rom_file[256];
	tmp = strrchr(path, '/') + 1;
	strcpy(rom_file, tmp);
	
	sprintf(slot_path, "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file); // /.userdata/.minui/<EMU>/<romname>.ext.txt
	sprintf(preview_path, "%s/.minui/%s/%s.0.bmp", SHARED_USERDATA_PATH, emu_name, rom_file); // /.userdata/.minui/<EMU>/<romname>.ext.0.bmp
	
	can_resume = exists(slot_path);
	has_preview = exists(preview_path);

}
static void readyResume(Entry* entry) {
	readyResumePath(entry->path, entry->type);
}

static void saveLast(char* path);
static void loadLast(void);

static int autoResume(void) {
	// NOTE: bypasses recents

	if (!exists(AUTO_RESUME_PATH)) return 0;
	
	char path[256];
	getFile(AUTO_RESUME_PATH, path, 256);
	unlink(AUTO_RESUME_PATH);
	sync();
	
	// make sure rom still exists
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);
	if (!exists(sd_path)) return 0;
	
	// make sure emu still exists
	char emu_name[256];
	getEmuName(sd_path, emu_name);
	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	if (!exists(emu_path)) return 0;
	
	// putFile(LAST_PATH, FAUX_RECENT_PATH); // saveLast() will crash here because top is NULL

	char act[256];
	sprintf(act, "gametimectl.elf start '%s'", escapeSingleQuotes(sd_path));
	system(act);
	
	char cmd[256];
	// dont escape sd_path again because it was already escaped for gametimectl and function modifies input str aswell
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), sd_path);
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	queueNext(cmd);
	return 1;
}

static void openPak(char* path) {
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	// if (prefixMatch(ROMS_PATH, path)) {
	// 	addRecent(path);
	// }
	saveLast(path);
	
	char cmd[256];
	sprintf(cmd, "'%s/launch.sh'", escapeSingleQuotes(path));
	queueNext(cmd);
}
static void openRom(char* path, char* last) {
	LOG_info("openRom(%s,%s)\n", path, last);
	
	char sd_path[256];
	strcpy(sd_path, path);
	
	char m3u_path[256];
	int has_m3u = hasM3u(sd_path, m3u_path);
	
	char recent_path[256];
	strcpy(recent_path, has_m3u ? m3u_path : sd_path);
	
	if (has_m3u && suffixMatch(".m3u", sd_path)) {
		getFirstDisc(m3u_path, sd_path);
	}

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	if (should_resume) {
		char slot[16];
		getFile(slot_path, slot, 16);
		putFile(RESUME_SLOT_PATH, slot);
		should_resume = 0;

		if (has_m3u) {
			char rom_file[256];
			strcpy(rom_file, strrchr(m3u_path, '/') + 1);
			
			// get disc for state
			char disc_path_path[256];
			sprintf(disc_path_path, "%s/.minui/%s/%s.%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file, slot); // /.userdata/arm-480/.minui/<EMU>/<romname>.ext.0.txt

			if (exists(disc_path_path)) {
				// switch to disc path
				char disc_path[256];
				getFile(disc_path_path, disc_path, 256);
				if (disc_path[0]=='/') strcpy(sd_path, disc_path); // absolute
				else { // relative
					strcpy(sd_path, m3u_path);
					char* tmp = strrchr(sd_path, '/') + 1;
					strcpy(tmp, disc_path);
				}
			}
		}
	}
	else putInt(RESUME_SLOT_PATH,8); // resume hidden default state
	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	addRecent(recent_path, recent_alias); // yiiikes
	saveLast(last==NULL ? sd_path : last);
	char act[256];
	sprintf(act, "gametimectl.elf start '%s'", escapeSingleQuotes(sd_path));
	system(act);
	char cmd[256];
	// dont escape sd_path again because it was already escaped for gametimectl and function modifies input str aswell
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), sd_path);
	queueNext(cmd);
}
static void openDirectory(char* path, int auto_launch) {
	char auto_path[256];
	if (hasCue(path, auto_path) && auto_launch) {
		openRom(auto_path, path);
		return;
	}

	char m3u_path[256];
	strcpy(m3u_path, auto_path);
	char* tmp = strrchr(m3u_path, '.') + 1; // extension
	strcpy(tmp, "m3u"); // replace with m3u
	if (exists(m3u_path) && auto_launch) {
		auto_path[0] = '\0';
		if (getFirstDisc(m3u_path, auto_path)) {
			openRom(auto_path, path);
			return;
		}
		// TODO: doesn't handle empty m3u files
	}
	
	int selected = 0;
	int start = selected;
	int end = 0;
	if (top && top->entries->count>0) {
		if (restore_depth==stack->count && top->selected==restore_relative) {
			selected = restore_selected;
			start = restore_start;
			end = restore_end;
		}
	}
	
	top = Directory_new(path, selected);
	top->start = start;
	top->end = end ? end : ((top->entries->count<MAIN_ROW_COUNT) ? top->entries->count : MAIN_ROW_COUNT);

	Array_push(stack, top);
}
static void closeDirectory(void) {
	restore_selected = top->selected;
	restore_start = top->start;
	restore_end = top->end;
	DirectoryArray_pop(stack);
	restore_depth = stack->count;
	top = stack->items[stack->count-1];
	restore_relative = top->selected;
}

static void Entry_open(Entry* self) {
	recent_alias = self->name;  // yiiikes
	if (self->type==ENTRY_ROM) {
		char *last = NULL;
		if (prefixMatch(COLLECTIONS_PATH, top->path)) {
			char* tmp;
			char filename[256];
			
			tmp = strrchr(self->path, '/');
			if (tmp) strcpy(filename, tmp+1);
			
			char last_path[256];
			sprintf(last_path, "%s/%s", top->path, filename);
			last = last_path;
		}
		openRom(self->path, last);
	}
	else if (self->type==ENTRY_PAK) {
		openPak(self->path);
	}
	else if (self->type==ENTRY_DIR) {
		openDirectory(self->path, 1);
	}
}

///////////////////////////////////////

static void saveLast(char* path) {
	// special case for recently played
	if (exactMatch(top->path, FAUX_RECENT_PATH)) {
		// NOTE: that we don't have to save the file because
		// your most recently played game will always be at
		// the top which is also the default selection
		path = FAUX_RECENT_PATH;
	}
	putFile(LAST_PATH, path);
}
static void loadLast(void) { // call after loading root directory
	if (!exists(LAST_PATH)) return;

	char last_path[256];
	getFile(LAST_PATH, last_path, 256);
	
	char full_path[256];
	strcpy(full_path, last_path);
	
	char* tmp;
	char filename[256];
	tmp = strrchr(last_path, '/');
	if (tmp) strcpy(filename, tmp);
	
	Array* last = Array_new();
	while (!exactMatch(last_path, SDCARD_PATH)) {
		Array_push(last, strdup(last_path));
		
		char* slash = strrchr(last_path, '/');
		last_path[(slash-last_path)] = '\0';
	}
	
	while (last->count>0) {
		char* path = Array_pop(last);
		if (!exactMatch(path, ROMS_PATH)) { // romsDir is effectively root as far as restoring state after a game
			char collated_path[256];
			collated_path[0] = '\0';
			if (suffixMatch(")", path) && isConsoleDir(path)) {
				strcpy(collated_path, path);
				tmp = strrchr(collated_path, '(');
				if (tmp) tmp[1] = '\0'; // 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
			}
			
			for (int i=0; i<top->entries->count; i++) {
				Entry* entry = top->entries->items[i];
			
				// NOTE: strlen() is required for collated_path, '\0' wasn't reading as NULL for some reason
				if (exactMatch(entry->path, path) || (strlen(collated_path) && prefixMatch(collated_path, entry->path)) || (prefixMatch(COLLECTIONS_PATH, full_path) && suffixMatch(filename, entry->path))) {
					top->selected = i;
					if (i>=top->end) {
						top->start = i;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>top->entries->count) {
							top->end = top->entries->count;
							top->start = top->end - MAIN_ROW_COUNT;
						}
					}
					if (last->count==0 && !exactMatch(entry->path, FAUX_RECENT_PATH) && !(!exactMatch(entry->path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, entry->path))) break; // don't show contents of auto-launch dirs
				
					if (entry->type==ENTRY_DIR) {
						openDirectory(entry->path, 0);
						break;
					}
				}
			}
		}
		free(path); // we took ownership when we popped it
	}
	
	StringArray_free(last);
}

///////////////////////////////////////

static void Menu_init(void) {
	stack = Array_new(); // array of open Directories
	recents = Array_new();
	openDirectory(SDCARD_PATH, 0);
	loadLast(); // restore state when available
}

static void Menu_quit(void) {
	RecentArray_free(recents);
	DirectoryArray_free(stack);
}

///////////////////////////////////////

static SDL_Rect GFX_scaled_rect(SDL_Rect preview_rect, SDL_Rect image_rect) {
    SDL_Rect scaled_rect;
    
    // Calculate the aspect ratios
    float image_aspect = (float)image_rect.w / (float)image_rect.h;
    float preview_aspect = (float)preview_rect.w / (float)preview_rect.h;
    
    // Determine scaling factor
    if (image_aspect > preview_aspect) {
        // Image is wider than the preview area
        scaled_rect.w = preview_rect.w;
        scaled_rect.h = (int)(preview_rect.w / image_aspect);
    } else {
        // Image is taller than or equal to the preview area
        scaled_rect.h = preview_rect.h;
        scaled_rect.w = (int)(preview_rect.h * image_aspect);
    }
    
    // Center the scaled rectangle within preview_rect
    scaled_rect.x = preview_rect.x + (preview_rect.w - scaled_rect.w) / 2;
    scaled_rect.y = preview_rect.y + (preview_rect.h - scaled_rect.h) / 2;
    
    return scaled_rect;
}

static float selection_offset = 1.0f; 
static int previous_selected = -1; 
static int dirty = 1;
static int dirtyanim = 1;
static int last_selection = -1;
static int remember_selection = 0;
// functionooos for like animation haha
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

void updateSelectionAnimation(int selected) {
	dirtyanim=1;
    if ((selected == previous_selected + 1 || selected == previous_selected - 1) && selection_offset >= 1.0f) {
        selection_offset = 0.0f; 
    }  else if(selection_offset >= 1.0f) {
		previous_selected = selected;
	}
		
    if (selection_offset <= 1.0f) {
        selection_offset += 0.35f; 
        if (selection_offset >= 1.0f) {
            selection_offset = 1.0f;
            previous_selected = selected;
			dirtyanim = 0;
			
        }
    } 
}

///////////////////////////////////////

int main (int argc, char *argv[]) {
	// LOG_info("time from launch to:\n");
	// unsigned long main_begin = SDL_GetTicks();
	// unsigned long first_draw = 0;
	
	if (autoResume()) return 0; // nothing to do
	
	simple_mode = exists(SIMPLE_MODE_PATH);

	LOG_info("MinUI\n");
	InitSettings();
	
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	// LOG_info("- graphics init: %lu\n", SDL_GetTicks() - main_begin);
	
	PAD_init();
	// LOG_info("- input init: %lu\n", SDL_GetTicks() - main_begin);
	
	PWR_init();
	if (!HAS_POWER_BUTTON) PWR_disableSleep();
	// LOG_info("- power init: %lu\n", SDL_GetTicks() - main_begin);
	
	SDL_Surface* version_screen = NULL;
	SDL_Surface *preview = NULL;

	Menu_init();
	// LOG_info("- menu init: %lu\n", SDL_GetTicks() - main_begin);

	show_switcher_screen = exists(GAME_SWITCHER_PERSIST_PATH);
	if (show_switcher_screen) {
		// consider this "consumed", dont bring up the switcher next time we regularly exit a game
		unlink(GAME_SWITCHER_PERSIST_PATH);
		// todo: map recent slot to last used game
	}

	// make sure we have no running games logged as active anymore (we might be launching back into the UI here)
	system("gametimectl.elf stop_all");
	
	// now that (most of) the heavy lifting is done, take a load off
	// PWR_setCPUSpeed(CPU_SPEED_MENU);
	// GFX_setVsync(VSYNC_STRICT);

	PAD_reset();
	
	int show_setting = 0; // 1=brightness,2=volume
	int was_online = PLAT_isOnline();
	int had_thumb = 0;
	int selected_row = top->selected - top->start;
	float targetY;
	float previousY;
	float highlightY;
	SDL_Surface* thumbbmp;
	int ox;
	int oy;
	while (!quit) {
		GFX_startFrame();
		unsigned long now = SDL_GetTicks();
		
		PAD_poll();
			
		int selected = top->selected;
		int total = top->entries->count;
		
		PWR_update(&dirty, &show_setting, NULL, NULL);
		
		int is_online = PLAT_isOnline();
		if (was_online!=is_online) dirty = 1;
		was_online = is_online;
		
		if (show_version_screen) {

			// If the Konami Code was entered, enable/disable Simple Mode.
			int konamiCodeIndex = KONAMI_update(now);
			if (konamiCodeIndex == KONAMI_CODE_LENGTH) {
								
				// Turn Simple Mode On.
				if(!simple_mode) {
					simple_mode = 1;
					FILE *file = fopen(SIMPLE_MODE_PATH, "w");
					if (file) fclose(file);

				// Turn Simple Mode Off.
				} else {
					simple_mode = 0;
					remove(SIMPLE_MODE_PATH);
				}				
				
				SDL_FreeSurface(version_screen);
				version_screen = NULL;

				// Ensure the directory list is rebuilt.
				restore_selected = 0;
				restore_start = 0;
				restore_end = 0;
				top = NULL;
				DirectoryArray_free(stack);
				stack = Array_new();
				openDirectory(SDCARD_PATH, 0);

				dirty = 1;
				
				if (!HAS_POWER_BUTTON) PWR_disableSleep();
				continue;
			
			// Menu, or B button (outside of the Konami Code) closes the version screen.
			} else if (PAD_tappedMenu(now) || !konamiCodeIndex && PAD_justPressed(BTN_B)) {
				show_version_screen = 0;
				dirty = 1;
				if (!HAS_POWER_BUTTON) PWR_disableSleep();
			}
		
		} else if(show_switcher_screen) {
			if (PAD_justPressed(BTN_B) || PAD_justReleased(BTN_SELECT)) {
				show_switcher_screen = 0;
				switcher_selected = 0;
				dirty = 1;
			}
			else if (recents->count > 0 && PAD_justReleased(BTN_A)) {
				// TODO: This is crappy af - putting this here since it works, but
				// super inefficient. Why are Recents not decorated with type, and need
				// to be remade into Entries via getRecents()? - need to understand the 
				// architecture more...
				Entry *selectedEntry = entryFromRecent(recents->items[switcher_selected]);
				should_resume = can_resume;
				Entry_open(selectedEntry);
				dirty = 1;
				Entry_free(selectedEntry);
			}
			else if (PAD_justPressed(BTN_RIGHT)) {
				switcher_selected++;
				if(switcher_selected == recents->count)
					switcher_selected = 0; // wrap
				dirty = 1;
			}
			else if (PAD_justPressed(BTN_LEFT)) {
				switcher_selected--;
				if(switcher_selected < 0)
					switcher_selected = recents->count - 1; // wrap
				dirty = 1;
			}
		}
		else if(!dirtyanim) {
			if (PAD_tappedMenu(now)) {
				show_version_screen = 1;
				show_switcher_screen = 0; // just to be sure
				dirty = 1;
				KONAMI_init();
				if (!HAS_POWER_BUTTON) PWR_enableSleep();
			}
			else if (PAD_justReleased(BTN_SELECT)) {
				show_switcher_screen = 1;
				switcher_selected = 0; 
				show_version_screen = 0; // just to be sure
				dirty = 1;
			}
			else if (total>0) {
				if (PAD_justRepeated(BTN_UP)) {
					if (selected==0 && !PAD_justPressed(BTN_UP)) {
						// stop at top
					}
					else {
						selected -= 1;
						if (selected<0) {
							selected = total-1;
							int start = total - MAIN_ROW_COUNT;
							top->start = (start<0) ? 0 : start;
							top->end = total; 
						}
						else if (selected<top->start) {
							top->start -= 1;
							top->end -= 1;
						}
					}
				}
				else if (PAD_justRepeated(BTN_DOWN)) {
					if (selected==total-1 && !PAD_justPressed(BTN_DOWN)) {
						// stop at bottom
					}
					else {
						selected += 1;
						if (selected>=total) {
							selected = 0;
							top->start = 0;
							top->end = (total<MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
						}
						else if (selected>=top->end) {
							top->start += 1;
							top->end += 1;
						}
					}
				}
				if (PAD_justRepeated(BTN_LEFT)) {
					selected -= MAIN_ROW_COUNT;
					if (selected<0) {
						selected = 0;
						top->start = 0;
						top->end = (total<MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
					}
					else if (selected<top->start) {
						top->start -= MAIN_ROW_COUNT;
						if (top->start<0) top->start = 0;
						top->end = top->start + MAIN_ROW_COUNT;
					}
				}
				else if (PAD_justRepeated(BTN_RIGHT)) {
					selected += MAIN_ROW_COUNT;
					if (selected>=total) {
						selected = total-1;
						int start = total - MAIN_ROW_COUNT;
						top->start = (start<0) ? 0 : start;
						top->end = total;
					}
					else if (selected>=top->end) {
						top->end += MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
		
			if (PAD_justRepeated(BTN_L1) && !PAD_isPressed(BTN_R1) && !PWR_ignoreSettingInput(BTN_L1, show_setting)) { // previous alpha
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha-1;
				if (i>=0) {
					selected = top->alphas->items[i];
					if (total>MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
			else if (PAD_justRepeated(BTN_R1) && !PAD_isPressed(BTN_L1) && !PWR_ignoreSettingInput(BTN_R1, show_setting)) { // next alpha
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha+1;
				if (i<top->alphas->count) {
					selected = top->alphas->items[i];
					if (total>MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
	
			if (selected!=top->selected) {
				top->selected = selected;
				dirty = 1;
			}
	
			if (dirty && total>0) readyResume(top->entries->items[top->selected]);

			if (total>0 && can_resume && PAD_justReleased(BTN_RESUME)) {
				should_resume = 1;
				Entry_open(top->entries->items[top->selected]);
				dirty = 1;
			}
			else if (total>0 && PAD_justPressed(BTN_A)) {
				Entry_open(top->entries->items[top->selected]);
				total = top->entries->count;
				dirty = 1;

				if (total>0) readyResume(top->entries->items[top->selected]);
			}
			else if (PAD_justPressed(BTN_B) && stack->count>1) {
				closeDirectory();
				total = top->entries->count;
				dirty = 1;
				// can_resume = 0;
				if (total>0) readyResume(top->entries->items[top->selected]);
			}
		}
		
		if(dirty || dirtyanim) {
			GFX_clear(screen);
			
			
			
			// simple thumbnail support a thumbnail for a file or folder named NAME.EXT needs a corresponding /.res/NAME.EXT.png 
			// that is no bigger than platform FIXED_HEIGHT x FIXED_HEIGHT
			
			if (!show_version_screen && total > 0) {
				Entry* entry = top->entries->items[top->selected];
			
				char res_root[MAX_PATH];
				strncpy(res_root, entry->path, sizeof(res_root) - 1);
				res_root[sizeof(res_root) - 1] = '\0';  
			
				char tmp_path[MAX_PATH];
				strncpy(tmp_path, entry->path, sizeof(tmp_path) - 1);
				tmp_path[sizeof(tmp_path) - 1] = '\0';
			
				char* res_name = strrchr(tmp_path, '/');
				if (res_name) res_name++;
			
				if (dirty) {
					char path_copy[1024];
					strncpy(path_copy, entry->path, sizeof(path_copy) - 1);
					path_copy[sizeof(path_copy) - 1] = '\0';
			
					char* tmp = strrchr(res_root, '/');
					if (tmp) *tmp = '\0'; 
			
					char res_path[1024];
					snprintf(res_path, sizeof(res_path), "%s/.res/%s.png", res_root, res_name);
			
					char* rompath = dirname(path_copy);
			
					char res_copy[1024];
					strncpy(res_copy, res_name, sizeof(res_copy) - 1);
					res_copy[sizeof(res_copy) - 1] = '\0';
			
					char* dot = strrchr(res_copy, '.');
					if (dot) *dot = '\0'; 
			
					char thumbpath[1024];
					snprintf(thumbpath, sizeof(thumbpath), "%s/.media/%s.png", rompath, res_copy);
			
					had_thumb = 0;
					if (exists(thumbpath)) {
						SDL_Surface* newThumb = IMG_Load(thumbpath);
						if (newThumb) {
							SDL_Surface* optimized = (newThumb->format->format == SDL_PIXELFORMAT_RGB888) ? newThumb : SDL_ConvertSurfaceFormat(newThumb, SDL_PIXELFORMAT_RGB888, 0);
							
							if (newThumb != optimized) {
								SDL_FreeSurface(newThumb);
							}
			
							if (optimized) {
								if (thumbbmp) SDL_FreeSurface(thumbbmp); 

								int img_w = optimized->w;
								int img_h = optimized->h;
								double aspect_ratio = (double)img_h / img_w; 
			
								int max_w = (int)(screen->w * 0.45); 
								int max_h = (int)(screen->h * 0.6);  
			
								int new_w = max_w;
								int new_h = (int)(new_w * aspect_ratio); 
			
			
								if (new_h > max_h) {
									new_h = max_h;
									new_w = (int)(new_h / aspect_ratio);
								}
			
								SDL_Rect scale_rect = {
									0,
									0,
									new_w,
									new_h
								};

								thumbbmp = SDL_CreateRGBSurfaceWithFormat(0, scale_rect.w, scale_rect.h, 32, SDL_PIXELFORMAT_RGB888);
								SDL_BlitScaled(optimized, NULL, thumbbmp, &scale_rect);
								GFX_ApplyRounderCorners(thumbbmp, 20);
								SDL_FreeSurface(optimized);
								had_thumb = 1;
							}
						}
					}
				}
			
				if (had_thumb) { 
					int target_y = (int)(screen->h * 0.48);
					int center_y = target_y - (thumbbmp->h / 2);
					SDL_Rect dest_rect = {
						screen->w-(thumbbmp->w + SCALE1(BUTTON_MARGIN*3)),
						center_y,
						thumbbmp->w,
						thumbbmp->h
					};
				
					ox = (int)(screen->w - thumbbmp->w) - SCALE1(BUTTON_MARGIN*5);
					SDL_BlitSurface(thumbbmp, NULL, screen, &dest_rect);
					
					
				}
			}
						
			int ow = GFX_blitHardwareGroup(screen, show_setting);
			if (show_version_screen) {
				
				// If version screen doesn't exist yet, grab it from version.txt and generate it.
				if (!version_screen) {
					char release[256];
					getFile(ROOT_SYSTEM_PATH "/version.txt", release, 256);
					
					char *tmp,*commit;
					commit = strrchr(release, '\n');
					commit[0] = '\0';
					commit = strrchr(release, '\n')+1;
					tmp = strchr(release, '\n');
					tmp[0] = '\0';

					int key_width = 0;
					int val_width = 0;
					
					// Release.
					SDL_Surface* release_key_txt = TTF_RenderUTF8_Blended(font.large, "Release", COLOR_DARK_TEXT);
					SDL_Surface* release_val_txt = TTF_RenderUTF8_Blended(font.large, release, COLOR_WHITE);
					if (release_key_txt->w > key_width) key_width = release_key_txt->w;
					if (release_val_txt->w > val_width) val_width = release_val_txt->w;
					
					// Commit.				
					SDL_Surface* commit_key_txt = TTF_RenderUTF8_Blended(font.large, "Commit", COLOR_DARK_TEXT);
					SDL_Surface* commit_val_txt = TTF_RenderUTF8_Blended(font.large, commit, COLOR_WHITE);
					if (commit_key_txt->w > key_width) key_width = commit_key_txt->w;
					if (commit_val_txt->w > val_width) val_width = commit_val_txt->w;
					
					// Model.
					SDL_Surface* model_key_txt = TTF_RenderUTF8_Blended(font.large, "Model", COLOR_DARK_TEXT);
					SDL_Surface* model_val_txt = TTF_RenderUTF8_Blended(font.large, PLAT_getModel(), COLOR_WHITE);
					if (model_key_txt->w > key_width) key_width = model_key_txt->w;
					if (model_val_txt->w > val_width) val_width = model_val_txt->w;

					// Simple Mode Status.
					SDL_Surface* lockdown_key_txt = TTF_RenderUTF8_Blended(font.large, "Simple Mode", COLOR_DARK_TEXT);
					SDL_Surface* lockdown_val_txt = TTF_RenderUTF8_Blended(font.large, !simple_mode ? "Off" : "On", !simple_mode ? COLOR_WHITE : COLOR_GOLD);
					if (lockdown_key_txt->w > key_width) key_width = lockdown_key_txt->w;
					if (lockdown_val_txt->w > val_width) val_width = lockdown_val_txt->w;
									
					// Blit lines and clean up.
					#define VERSION_LINE_HEIGHT 24
					int x = key_width + SCALE1(8);
					int w = x + val_width;
					int h = SCALE1(VERSION_LINE_HEIGHT*4);
					version_screen = SDL_CreateRGBSurface(0,w,h,16,0,0,0,0);
					SDL_BlitSurface(release_key_txt, NULL, version_screen, &(SDL_Rect){0, 0});
					SDL_BlitSurface(release_val_txt, NULL, version_screen, &(SDL_Rect){x,0});
					SDL_BlitSurface(commit_key_txt, NULL, version_screen, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT)});
					SDL_BlitSurface(commit_val_txt, NULL, version_screen, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT)});
					SDL_BlitSurface(model_key_txt, NULL, version_screen, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT*2)});
					SDL_BlitSurface(model_val_txt, NULL, version_screen, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT*2)});
					SDL_BlitSurface(lockdown_key_txt, NULL, version_screen, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT*3)});
					SDL_BlitSurface(lockdown_val_txt, NULL, version_screen, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT*3)});
					SDL_FreeSurface(release_key_txt);
					SDL_FreeSurface(release_val_txt);
					SDL_FreeSurface(commit_key_txt);
					SDL_FreeSurface(commit_val_txt);
					SDL_FreeSurface(model_key_txt);
					SDL_FreeSurface(model_val_txt);
					SDL_FreeSurface(lockdown_key_txt);
					SDL_FreeSurface(lockdown_val_txt);
				}
				SDL_BlitSurface(version_screen, NULL, screen, &(SDL_Rect){(screen->w-version_screen->w)/2,(screen->h-version_screen->h)/2});
				
				// buttons (duped and trimmed from below)
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting);
				else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"MENU","SLEEP",  NULL }, 0, screen, 0);
				
				GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 1);
			}
			else if(show_switcher_screen) {
				// For all recents with resumable state (i.e. has savegame), show game switcher carousel

				#define WINDOW_RADIUS 0 // TODO: this logic belongs in blitRect?
				#define PAGINATION_HEIGHT 0
				// unscaled
				int hw = screen->w;
				int hh = screen->h;
				int pw = hw + SCALE1(WINDOW_RADIUS*2);
				int ph = hh + SCALE1(WINDOW_RADIUS*2 + PAGINATION_HEIGHT + WINDOW_RADIUS);
				ox = 0; // screen->w - pw - SCALE1(PADDING);
				oy = 0; // (screen->h - ph) / 2;

				// window
				// GFX_blitRect(ASSET_STATE_BG, screen, &(SDL_Rect){ox,oy,pw,ph});

				if(recents->count > 0) {
					Entry *selectedEntry = entryFromRecent(recents->items[switcher_selected]);
					readyResume(selectedEntry);

					if(has_preview) {
						// lotta memory churn here
						SDL_Surface* bmp = IMG_Load(preview_path);
						SDL_Surface* raw_preview = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA32, 0);
						if (raw_preview) {
							SDL_FreeSurface(bmp); 
							bmp = raw_preview; 
						}
						if(bmp) {
							SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, screen->w, screen->h, 32, SDL_PIXELFORMAT_RGBA32);					
							SDL_Rect image_rect = {0, 0, screen->w, screen->h};
							SDL_BlitScaled(bmp, NULL, scaled, &image_rect);
							SDL_FreeSurface(bmp);
							GFX_ApplyRounderCorners(scaled,30);
							SDL_BlitSurface(scaled, NULL, screen, &image_rect);
							SDL_FreeSurface(scaled);  // Free after rendering
						}
					}
					else {
						SDL_Rect preview_rect = {ox,oy,hw,hh};
						SDL_FillRect(screen, &preview_rect, 0);
						GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					}

					// title pill
					{
						int ow = GFX_blitHardwareGroup(screen, show_setting);
						int max_width = screen->w - SCALE1(PADDING * 2) - ow;
						
						char display_name[256];
						int text_width = GFX_truncateText(font.large, selectedEntry->name, display_name, max_width, SCALE1(BUTTON_PADDING*2));
						max_width = MIN(max_width, text_width);

						SDL_Surface* text;
						text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);
						GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
							SCALE1(PADDING),
							SCALE1(PADDING),
							max_width,
							SCALE1(PILL_SIZE)
						});
						SDL_BlitSurface(text, &(SDL_Rect){
							0,
							0,
							max_width-SCALE1(BUTTON_PADDING*2),
							text->h
						}, screen, &(SDL_Rect){
							SCALE1(PADDING+BUTTON_PADDING),
							SCALE1(PADDING+4)
						});
						SDL_FreeSurface(text);
					}

					// pagination
					{

					}

					if(can_resume) GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 0);
					else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"MENU","SLEEP",  NULL }, 0, screen, 0);

					GFX_blitButtonGroup((char*[]){ "A","RESUME", NULL }, 1, screen, 1);
					
					Entry_free(selectedEntry);
				}
				else {
					SDL_Rect preview_rect = {ox,oy,hw,hh};
					SDL_FillRect(screen, &preview_rect, 0);
					GFX_blitMessage(font.large, "No Recents", screen, &preview_rect);
					GFX_blitButtonGroup((char*[]){ "B","BACK", NULL }, 1, screen, 1);
				}
			}
			else {
				updateSelectionAnimation(top->selected);
				// list
				if (total > 0) {
					selected_row = top->selected - top->start;
					remember_selection = selected_row;
					targetY = selected_row * PILL_SIZE;
					previousY = (previous_selected - top->start) * PILL_SIZE;
				
					highlightY = (last_selection != selected_row) ? lerp(previousY, targetY, selection_offset) : targetY;
					if (selection_offset >= 1.0f) {
						last_selection = selected_row;
					}
				
					for (int i = top->start, j = 0; i < top->end; i++, j++) {
						Entry* entry = top->entries->items[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;
						int available_width = (had_thumb ? ox - SCALE1(BUTTON_MARGIN) : screen->w - SCALE1(BUTTON_PADDING)) - SCALE1(PADDING * 2);
						if (i == top->start && !(had_thumb)) available_width -= ow;
						SDL_Color text_color = COLOR_WHITE;
						

						
						trimSortingMeta(&entry_name);

						if (entry_unique) { // Only render if a unique name exists
							trimSortingMeta(&entry_unique);
						} 
						
						char display_name[256];
						int text_width = GFX_getTextWidth(font.large, entry_unique ? entry_unique : entry_name,display_name, available_width, SCALE1(BUTTON_PADDING * 2));
						int max_width = MIN(available_width, text_width);
						SDL_Surface* text = TTF_RenderUTF8_Blended(font.large, entry_name, text_color);
						SDL_Surface* text_unique = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_DARK_TEXT);
						SDL_Rect text_rect = { 0, 0, max_width - SCALE1(BUTTON_PADDING), text->h };
						SDL_Rect dest_rect = { SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + (j * PILL_SIZE))+4 };
						
						
						SDL_BlitSurface(text_unique, &text_rect, screen, &dest_rect);
						SDL_BlitSurface(text, &text_rect, screen, &dest_rect);
						
						SDL_FreeSurface(text_unique); // Free after use
						SDL_FreeSurface(text); // Free after use
					
					}
					for (int i = top->start, j = 0; i < top->end; i++, j++) {
						Entry* entry = top->entries->items[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;
						int available_width = (had_thumb ? ox + SCALE1(BUTTON_MARGIN) : screen->w - SCALE1(BUTTON_MARGIN)) - SCALE1(PADDING * 2);
						if (i == top->start && !(had_thumb)) available_width -= ow;
						trimSortingMeta(&entry_name);
						char display_name[256];
						int text_width = GFX_getTextWidth(font.large, entry_unique ? entry_unique : entry_name, display_name, available_width, SCALE1(BUTTON_PADDING * 2));
						int max_width = MIN(available_width, text_width);
						float inverted_offset = 1.0f - selection_offset;

						if (j == selected_row) {
							SDL_Surface* text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_BLACK);
							SDL_Rect src_text_rect = {  0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h };
												
							SDL_Rect text_rect = {  SCALE1(BUTTON_PADDING), SCALE1((last_selection != selected_row ? last_selection < selected_row ? (inverted_offset * PILL_SIZE):(inverted_offset * -PILL_SIZE):0) +4) };

							SDL_Rect anim_rect = {  SCALE1(BUTTON_MARGIN),SCALE1(highlightY+PADDING) };
							SDL_Surface *cool = SDL_CreateRGBSurface(SDL_SWSURFACE, max_width, SCALE1(PILL_SIZE), FIXED_DEPTH, RGBA_MASK_565);
							GFX_resetScrollText();
							GFX_blitPillDark(ASSET_WHITE_PILL, cool, &(SDL_Rect){
								0, 0, max_width, SCALE1(PILL_SIZE)
							});

							SDL_BlitSurface(text, &src_text_rect, cool, &text_rect);
							SDL_BlitSurface(cool, NULL, screen, &anim_rect);
							
							SDL_FreeSurface(cool);
							SDL_FreeSurface(text);
						} 
					}
		
				}
				
				else {
					// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
					GFX_blitMessage(font.large, "Empty folder", screen, &(SDL_Rect){0,0,screen->w,screen->h}); //, NULL);
				}
		
				// buttons
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting);
				else if (can_resume) GFX_blitButtonGroup((char*[]){ "X","RESUME",  NULL }, 0, screen, 0);
				else GFX_blitButtonGroup((char*[]){ 
					BTN_SLEEP==BTN_POWER?"POWER":"MENU",
					BTN_SLEEP==BTN_POWER?"SLEEP":"INFO",  
					NULL }, 0, screen, 0);
			
				if (total==0) {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 1);
					}
				}
				else {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OPEN", NULL }, 1, screen, 1);
					}
					else {
						GFX_blitButtonGroup((char*[]){ "A","OPEN", NULL }, 0, screen, 1);
					}
				}
			}
			
			GFX_flip(screen);
			dirty = 0;
		} else {
			int ow = GFX_blitHardwareGroup(screen, show_setting);
			if(!show_switcher_screen && !show_version_screen) {
			// nondirty
			
			Entry* entry = top->entries->items[top->selected];
			char* entry_name = entry->name;
			char* entry_unique = entry->unique;
			trimSortingMeta(&entry_name);
			int available_width = (had_thumb ? ox + SCALE1(BUTTON_MARGIN) : screen->w - SCALE1(BUTTON_PADDING)) - SCALE1(PADDING * 2);
			if (top->selected == top->start && !(had_thumb)) available_width -= ow;
	 
			if (entry_unique) { // Only render if a unique name exists
				trimSortingMeta(&entry_unique);
			} 
			char display_name[256];
			int text_width = GFX_getTextWidth(font.large, entry_unique ? entry_unique : entry_name,display_name, available_width, SCALE1(BUTTON_PADDING * 2));
			int max_width = MIN(available_width, text_width);

			
			SDL_Surface* text2 = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_BLACK);
			SDL_Rect clear_rect = { SCALE1(BUTTON_MARGIN) + SCALE1(BUTTON_PADDING),SCALE1(PADDING + (remember_selection * PILL_SIZE) +4),max_width - ((SCALE1(BUTTON_PADDING*2))),text2->h};
	
			SDL_Rect dest_rect = { SCALE1(BUTTON_PADDING + BUTTON_MARGIN), SCALE1(PADDING + ((remember_selection) * PILL_SIZE) +4) };
			

			SDL_Rect src_text_rect = {  0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text2->h };

			SDL_FillRect(screen, &clear_rect, THEME_COLOR1);
			GFX_scrollTextSurface(font.large, display_name, &text2,max_width - ((SCALE1(BUTTON_PADDING*2))),text2->h, 0, COLOR_BLACK, 1);
			
		
			SDL_BlitSurface(text2, &src_text_rect, screen, &dest_rect);
			SDL_FreeSurface(text2);
			GFX_flip(screen);
			} else {
				GFX_sync();
			}
			dirty = 0;
		}
		// handle HDMI change
		static int had_hdmi = -1;
		int has_hdmi = GetHDMI();
		if (had_hdmi==-1) had_hdmi = has_hdmi;
		if (has_hdmi!=had_hdmi) {
			had_hdmi = has_hdmi;

			Entry* entry = top->entries->items[top->selected];
			LOG_info("restarting after HDMI change... (%s)\n", entry->path);
			saveLast(entry->path); // NOTE: doesn't work in Recents (by design)
			sleep(4);
			quit = 1;
		}
		
	}
	
	if (version_screen) SDL_FreeSurface(version_screen);
	if (preview) SDL_FreeSurface(preview);
	if (thumbbmp) SDL_FreeSurface(thumbbmp);

	Menu_quit();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
}