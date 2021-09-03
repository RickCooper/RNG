#ifndef _lib_file_h_

#define  _lib_file_h_

#include <glib.h>
#include <stdio.h>
#include <unistd.h>

#define PATHNAME_LENGTH      512

#ifdef WIN32
#define DIR_TERM_CHAR       '\\'
#else
#define DIR_TERM_CHAR       '/'
#endif

/* The following should be defined in system include files, but often aren't: */

// extern char   *getcwd(char *path, int length);
// extern int     chdir(const char *dir);
// extern int     remove(const char *file);
// extern int     rmdir(const char *dir);
// extern int     link(const char *new, const char *old);
// extern int     unlink(const char *old);

/* The following are mostly Boolean functions, and return 1 on success.       */

extern int    file_exists(const char *filename);
extern int    file_is_contentless(char *filename);
extern int    file_ensure_existence(const char *filename);
extern int    file_copy(const char *source, const char *target);
extern int    file_move(const char *path, const char *file);
extern int    file_remove(const char *path, const char *file);
extern int    file_replace(const char *old, const char *new);
extern long   file_modification_time(const char *path, const char *filename);
extern long   file_size(const char *path, const char *filename);
extern int    file_line_count(const char *filename);

extern int    directory_exists(const char *filename);
extern int    directory_ensure_existence(const char *pathname);
extern int    directory_remove(const char *directory);
extern int    directory_create(const char *directory, int warn);
extern int    directory_copy(const char *dir1, const char *dir2);
extern int    directory_move(const char *source, const char *destination);
extern GList *directory_read_contents(const char *path, int warn, int include_condition());
extern int    directory_count_contents(const char *path, int warn, int include_condition());
extern long   directory_get_last_edit_time(char *path);
extern char  *directory_get_current(char *path, int l);
extern char  *directory_get_home();
extern void slash_terminate_directoryname(char **dirname);

extern int    string_is_non_dot_directory(const char *filename);
extern int    string_is_non_dot_file(const char *filename);
extern int    directory_contains_history_file(const char *dir);

extern int    memory_stats();
extern int    usertime();
extern int    systime();
extern void   printstats(FILE *fp);

/******************************************************************************/

#endif
