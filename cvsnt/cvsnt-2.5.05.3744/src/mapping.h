#ifndef MAPPING__H
#define MAPPING__H

typedef struct
{
	const char *directory;
	const char *translation;
	const char *regex;
	int local;
	int no_recursion;
	int lookupid;
} modules2_module_struct;

typedef struct
{
	const char *name;
	int module_count;
	modules2_module_struct *module;
} modules2_struct;

typedef struct rename_struct_t
{
	struct rename_struct_t *next;
	char *from;
	char *to;
} rename_struct;

/* modules2 helper routines */
char *map_repository(const char *repository);
char *map_filename(const char *repository, const char *name, const char **repository_out);
int nonrecursive_module(const char *repository);
const char *lookup_regex(const char *repository);
int regex_filename_match(const char *regex, const char *filename);
int free_modules2();

/* Repository version helper routines */
int open_directory(const char *repository, const char *dir, const char *tag, const char *date, int nonbranch, const char *version, int remote);
int commit_directory(const char *update_dir, const char *repository, const char *message);
int close_directory();
int free_directory();
char *map_fixed_rename(const char *repos, char *name);
int set_mapping(const char *directory, const char *oldfile, const char *newfile);
int add_mapping(const char *directory, const char *oldfile, const char *newfile);
const char *get_directory_version();
int get_directory_finfo(const char *repository, const char *dir, const char *update_dir, struct file_info *finfo);
int upgrade_entries(const char *repository, const char *dir, List **entries, List **renamed_files);

/* Called by find_names */
int find_virtual_dirs (const char *repository, List *list);
int find_virtual_rcs (const char *repository, List *list);
int find_rename_rcs (const char *repository, List *list);
int find_rename_dirs(const char *repository, List *list);


#endif

