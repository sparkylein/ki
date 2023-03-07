
char *get_fullpath(char *filepath);
int file_exists(const char *path);
int dir_exists(const char *path);
char *get_dir_from_path(char *path);
char *get_path_basename(char *path);
char *strip_ext(char *fn);
void makedir(char *dir, int mod);
char *get_binary_dir();
char *get_storage_path();

Array *get_subfiles(char *dir, bool dirs, bool files);
int mod_time(char *path);
void write_file(char *filepath, char *content, bool append);
Str *file_get_contents(char *path);