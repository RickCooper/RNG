/*******************************************************************************

    File:       lib_file.c
    Contents:   Miscellaneous file manipulation functions.
    Author:     Rick Cooper
    Copyright (c) 1997 Richard P. Cooper

    This file is part of COGENT.

    COGENT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    COGENT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with COGENT; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Public procedures:
        int    file_exists(const char *filename);
        int    file_ensure_existence(const char *filename);
        int    file_copy(const char *source, const char *target);
        int    file_replace(const char *old, const char *new);
        long   file_modification_time(const char *path, const char *filename);
        long   file_size(const char *path, const char *filename);

        int    directory_exists(const char *filename);
        int    directory_ensure_existence(const char *pathname);
        int    directory_remove(const char *directory);
        int    directory_create(const char *directory, int warn);
        int    directory_copy(const char *dir1, const char *dir2);
        int    directory_move(const char *source, const char *destination);

        GList *directory_read_contents(const char *path, int warn, int include_condition());
        int directory_count_contents(const char *path, int warn, int include_condition());
        long   directory_get_last_edit_time(char *path);
        void slash_terminate_directoryname(char **dirname);

        int    string_is_non_dot_directory(const char *filename);
        int    string_is_non_dot_file(const char *filename);
        int    directory_contains_history_file(const char *dir);

        int    memory_stats()
        int    usertime()
        int    systime()
        void   printstats(FILE *fp) 

*******************************************************************************/

/******** Include files: ******************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include "lib_file.h"
#include "lib_string.h"

#ifdef MALLOC_CHECK
#include "zmalloc.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

#define GtkWidget void

extern void    gtkx_warn(int warn, char *buffer);

/* A local buffer used for warning messages. It's defined once here instead   */
/* of where it is used to save space.                                         */

#define TEMP_BUFFER_LENGTH 128
char temp_buffer[TEMP_BUFFER_LENGTH];

/******************************************************************************/
/* Is a string the name of a file? -------------------------------------------*/

int file_exists(const char *filename)
{
    struct stat st;

    return( stat(filename, &st) == 0 );
}


/******************************************************************************/
/* Does the named file exist and have sensible content? ----------------------*/

int file_is_contentless(char *filename)
{
    int content = 0;
    FILE *fp;

    if ((fp = fopen(filename, "r")) == NULL) {
        return(TRUE);
    }
    else {
        int c;
        while (!content && ((c = getc(fp)) != EOF)) {
            content = isgraph(c);
        }
        fclose(fp);
    }
    return(!content);
}

/********* Ensure that the file exists ****************************************/

int file_ensure_existence(const char *filename)
{
    FILE *fp;

    if (file_exists(filename)) {
        return(TRUE);
    }
    else if ((fp = fopen(filename, "w")) == NULL) {
        return(FALSE);
    }
    else if (fclose(fp) == 0) {
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

int file_copy(const char *source, const char *target)
{
    FILE *fp1 = fopen(source, "r");
    FILE *fp2 = fopen(target, "w");

    if (fp1 == NULL) {
        return(FALSE);
    }
    else if (fp2 == NULL) {
        fclose(fp1);
        return(FALSE);
    }
    else {
        int c;
        while ((c = getc(fp1)) != EOF) {
            putc(c, fp2);
        }
        if ((fclose(fp1) == 0) && (fclose(fp2) == 0)) {
            return(TRUE);
        }
        else {
            return(FALSE);
        }
    }
}

/******************************************************************************/

int file_move(const char *source, const char *destination)
{
    if (rename(source, destination) == 0) {
        return(TRUE);
    }
    /* Rename failed: Perhaps we're attempt to move across file systems */
    /* Try copying and deleting the source.                             */
    else if (!file_copy(source, destination)) {
        /* Copy failed: Bad permission or quota exceeded.               */ 
        return(FALSE);
    }
    else if (remove(source) != 0) {
        /* Copy succeeded: But can't delete the original!               */
        remove(destination);
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

/******************************************************************************/

int file_replace(const char *old, const char *new)
{
    if (file_exists(old) && (unlink(old) < 0)) {
        return(FALSE);
    }
    else if (link(new, old) < 0) {
        return(FALSE);
    }
    else if (unlink(new) < 0) {
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

/******************************************************************************/

int file_remove(const char *path, const char *file)
{
    char buffer[PATHNAME_LENGTH];

    g_snprintf(buffer, PATHNAME_LENGTH, "%s%s", path, file);
    if (directory_exists(buffer)) {
        return(directory_remove(buffer));
    }
    else if (file_exists(buffer)) {
        return(remove(buffer));
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

long file_modification_time(const char *path, const char *filename)
{
    char file[PATHNAME_LENGTH];
    struct stat st;

    g_snprintf(file, PATHNAME_LENGTH, "%s/%s", path, filename);
    if (lstat(file, &st) == 0) {
        return((long) st.st_mtime);
    }
    else {
        return(0);
    }
}

long file_size(const char *path, const char *filename)
{
    char file[PATHNAME_LENGTH];
    struct stat st;

    g_snprintf(file, PATHNAME_LENGTH, "%s/%s", path, filename);
    if (lstat(file, &st) == 0) {
        return((long) st.st_size);
    }
    else {
        return(0);
    }
}

int file_line_count(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    int lines = 0, c;

    if (fp != NULL) {
        while ((c = getc(fp)) != EOF) {
            if (c == '\n') {
                lines++;
            }
        }
        fclose(fp);
    }
    return(lines);
}

/******************************************************************************/
/* Is a string the name of a directory? --------------------------------------*/

int directory_exists(const char *filename)
{
    struct stat st;

    if (stat(filename, &st) == 0) {
        return((st.st_mode & S_IFMT) == S_IFDIR);
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

int directory_ensure_existence(const char *pathname)
{
    if (directory_exists(pathname)) {
        return(TRUE);
    }
    else if (file_exists(pathname)) {
        return(FALSE);
    }
    else {
        return(directory_create(pathname, FALSE));
    }
}

/******************************************************************************/

int directory_remove(const char *directory)
{
    DIR *dp;

    if (!directory_exists(directory)) {
        return(TRUE);
    }
    else if ((dp = opendir(directory)) != NULL) {
        struct dirent *fp;
        char buffer[PATHNAME_LENGTH];
        int l = strlen(directory);

        strcpy(buffer, directory);
        buffer[l] = DIR_TERM_CHAR;
        while ((fp = readdir(dp)) != NULL) {
            if ((strcmp(fp->d_name, ".") != 0) && (strcmp(fp->d_name, "..") != 0)) {
                strcpy(&buffer[l+1], fp->d_name);

                if (directory_exists(buffer)) {
                    if (!directory_remove(buffer)) {
                        return(FALSE);
                    }
                }
                else if (remove(buffer) != 0) {
                    return(FALSE);
                }
            }
        }
        closedir(dp);
    }
    return(rmdir(directory) == 0);
}

/******************************************************************************/

static int make_directory(const char *directory, int warn)
{
    if (!directory_exists(directory)) {
        if (file_exists(directory)) {
            return(FALSE);
        }
        else if (mkdir(directory, S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
            return(TRUE);
        }
        else {
            return(FALSE);
        }
    }
    else {
        return(TRUE);
    }
}

/*----------------------------------------------------------------------------*/

int directory_create(const char *directory, int warn)
{
    if ((directory != NULL) && (directory[0] != '\0')) {
        if (directory_exists(directory)) {
            return(TRUE);
        }
        else {
            int i = 0;
            char *copy = string_copy(directory);
            while (copy[++i] != '\0') {
                if (copy[i] == DIR_TERM_CHAR) {
                    copy[i] = '\0';
                    if (!make_directory(copy, warn)) {
                        free(copy);
                        return(FALSE);
                    }
                    copy[i] = DIR_TERM_CHAR;
                }
            }
            free(copy);
            return(make_directory(directory, warn));
        }
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

int directory_copy(const char *dir1, const char *dir2)
{
    if (!directory_exists(dir1)) {
        return(FALSE);
    }
    else if (!directory_create(dir2, FALSE)) {
        return(FALSE);
    }
    else {
        DIR *dp;

        if ((dp = opendir(dir1)) != NULL) {
            char file1[PATHNAME_LENGTH];
            char file2[PATHNAME_LENGTH];
            int l1 = strlen(dir1);
            int l2 = strlen(dir2);
            struct dirent *fp;

            strcpy(file1, dir1);
            if (file1[--l1] != DIR_TERM_CHAR) {
                file1[++l1] = DIR_TERM_CHAR;
            }
            strcpy(file2, dir2);
            if (file2[--l2] != DIR_TERM_CHAR) {
                file2[++l2] = DIR_TERM_CHAR;
            }

            while ((fp = readdir(dp)) != NULL) {
                if ((strcmp(fp->d_name, ".") != 0) && (strcmp(fp->d_name, "..") != 0)) {
                    strcpy(&file1[l1+1], fp->d_name);
                    strcpy(&file2[l2+1], fp->d_name);

                    if (directory_exists(file1)) {
                        if (!directory_copy(file1, file2)) {
                            closedir(dp);
                            return(FALSE);
                        }
                    }
                    else {
                        if (!file_copy(file1, file2)) {
                            closedir(dp);
                            return(FALSE);
                        }
                    }
                }
            }
            closedir(dp);
        }
        return(TRUE);
    }
}

/******************************************************************************/

int directory_move(const char *source, const char *destination)
{
    if (rename(source, destination) == 0) {
        return(TRUE);
    }
    /* Rename failed: Perhaps we're attempt to move across file systems */
    /* Try copying and deleting the source.                             */
    else if (!directory_copy(source, destination)) {
        /* Copy failed: Bad permission or quota exceeded.               */ 
        return(FALSE);
    }
    else if (!directory_remove(source)) {
        /* Copy succeeded: But can't delete the original!               */
        directory_remove(destination);
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

/******************************************************************************/
/* Read the current directory and return a GList of file names ****************/

static GList *insert_match_in_g_list(GList *matches, char *filename)
{
    int l, i;

    if ((l = g_list_length(matches)) == 0) {
        return(g_list_append(matches, filename));
    }
    else {
        for (i = 0; i < l; i++) {
            char *pf = (char *)g_list_nth_data(matches, i);

            if (strcmp(pf, filename) > 0) {
                return(g_list_insert(matches, filename, i));
            }
        }
        return(g_list_append(matches, filename));
    }
}

GList *directory_read_contents(const char *path, int warn, int include_condition())
{
    char previous[PATHNAME_LENGTH];
    GList *matches = NULL;
    struct dirent *fp;
    DIR *dp;

    /* Open the current directory ... */
    if ((dp = opendir(path)) == NULL) {
        g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error reading %s", path);
        gtkx_warn(warn, temp_buffer);
    }
    else {
        /* Include condition might be relative, so chdir to path: */
        directory_get_current(previous, PATHNAME_LENGTH);
        if (chdir(path) < 0) {
            g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error changing directory to %s", path);
            gtkx_warn(warn, temp_buffer);
        }
        /* Scan all files in the directory ... */
        while ((fp = readdir(dp)) != NULL) {
            char *filename = string_copy(fp->d_name);
            /* And add matching files to the list: */
            if (include_condition(filename)) {
                /* Insert it in the appropriate point in the file list: */
                matches = insert_match_in_g_list(matches, filename);
            }
            else if (filename != NULL) {
                free(filename);
            }
        }
        if (chdir(previous) < 0) {
            g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error changing directory to %s", previous);
            gtkx_warn(warn, temp_buffer);
        }
        /* and close the directory: */
        closedir(dp);
    }
    /* Return the list of matching files: */
    return(matches);
}

int directory_count_contents(const char *path, int warn, int include_condition())
{
    char previous[PATHNAME_LENGTH];
    struct dirent *fp;
    DIR *dp;
    int n = 0;

    /* Open the directory ... */
    if ((dp = opendir(path)) == NULL) {
        g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error reading %s", path);
        gtkx_warn(warn, temp_buffer);
    }
    else {
        /* Include condition might be relative, so chdir to path: */
        directory_get_current(previous, PATHNAME_LENGTH);
        if (chdir(path) < 0) {
            g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error changing directory to %s", path);
            gtkx_warn(warn, temp_buffer);
        }
        /* Scan all files in the directory ... */
        while ((fp = readdir(dp)) != NULL) {
            if (include_condition(fp->d_name)) {
                n++;
            }
        }
        if (chdir(previous) < 0) {
            g_snprintf(temp_buffer, TEMP_BUFFER_LENGTH, "Error changing directory to %s", previous);
            gtkx_warn(warn, temp_buffer);
        }
        /* and close the directory: */
        closedir(dp);
    }
    /* Return the count of matching files: */
    return(n);
}

/******************************************************************************/
/* When was a directory last edited? ******************************************/

long directory_get_last_edit_time(char *path)
{
    long t = 0;

    char buffer[PATHNAME_LENGTH];
    int l = strlen(path);
    DIR *dp;

    if ((l < PATHNAME_LENGTH-1) && ((dp = opendir(path)) != NULL)) {
        struct dirent *fp;

        strncpy(buffer, path, PATHNAME_LENGTH);
        buffer[l] = DIR_TERM_CHAR;
        while ((fp = readdir(dp)) != NULL) {
            if ((fp->d_name[0] != '.') && (strlen(fp->d_name) < (PATHNAME_LENGTH-l-1))) {
                strcpy(&buffer[l+1], fp->d_name);
                if (directory_exists(buffer)) {
                    t = MAX(t, directory_get_last_edit_time(buffer));
                }
                else {
                    buffer[l] = '\0';
                    t = MAX(t, file_modification_time(buffer, fp->d_name));
                    buffer[l] = DIR_TERM_CHAR;
                    buffer[l+1] = '\0';
                }
            }
        }
        closedir(dp);
    }
    return(t);
}

/*----------------------------------------------------------------------------*/

void slash_terminate_directoryname(char **dirname)
{
    if (*dirname != NULL) {
        int l = strlen(*dirname);
        if ((*dirname)[l-1] != DIR_TERM_CHAR) {
            char *temp = string_new(l+2);

            if (temp != NULL) {
                temp[l+1] = '\0';
                temp[l] = DIR_TERM_CHAR;
                while (l-- > 0) {
                    temp[l] = (*dirname)[l];
                }
                free(*dirname);
                *dirname = temp;
            }
        }
    }
}

/******************************************************************************/
/* Boolean utility functions: *************************************************/

int string_is_non_dot_directory(const char *filename)
{
    return((filename[0] != '.') && (directory_exists(filename)));
}

/*----------------------------------------------------------------------------*/

int string_is_non_dot_file(const char *filename)
{
    return((filename[0] != '.')  && file_exists(filename) && !directory_exists(filename));
}

/*----------------------------------------------------------------------------*/

int directory_contains_history_file(const char *dir)
{
    char history_file[PATHNAME_LENGTH];

    g_snprintf(history_file, PATHNAME_LENGTH, "%s/history", dir);

    return(file_exists(history_file));
}

/******************************************************************************/

char *directory_get_current(char *path, int l)
{
#ifdef WIN32
    if (getcwd(path, l) != NULL) {
        char win32_path[MAX_PATH];

        cygwin_conv_to_win32_path(path, win32_path);
        strncpy(path, win32_path, l);

        /* Convert the drive name to upper case: */
        if (islower(path[0]) && (path[1] == ':')) {
            path[0] += 'A' - 'a';
        }

        return(path);
    }
    else {
        return(NULL);
    }
#else
    return(getcwd(path, l));
#endif
}


/*----------------------------------------------------------------------------*/

char *directory_get_home()
{
#ifdef WIN32
    HKEY handle;
    DWORD l = 256; /* Hard limit on length of registry values */
    DWORD dw_type;
    unsigned char *actual_val = (unsigned char *)malloc(l * sizeof(unsigned char));

    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", 0, KEY_QUERY_VALUE, &handle) != ERROR_SUCCESS) {
        free(actual_val);
        return(NULL);
    }
    else if ((RegQueryValueEx(handle, "Personal", 0, &dw_type, actual_val, &l) != ERROR_SUCCESS) || (dw_type != REG_SZ)) {
        RegCloseKey(handle);
        free(actual_val);
        return(NULL);
    }
    else {
        RegCloseKey(handle);
        return((char *)actual_val);
    }
#else
    return(string_copy(getenv("HOME")));
#endif
}

/******************************************************************************/

#include <sys/time.h>
#include <sys/resource.h>

int memory_stats() /* Return maximum resident set size in Kb */
{
    struct rusage resource_usage;

    getrusage(RUSAGE_SELF, &resource_usage);
    return((int) resource_usage.ru_maxrss);
}

int usertime() /* Return total milliseconds of user time */
{
    struct rusage resource_usage;
    struct timeval *user_time = &(resource_usage.ru_utime);

    getrusage(RUSAGE_SELF, &resource_usage);
    return(1000 * user_time->tv_sec + ((int) user_time->tv_usec/1000));
}

int systime() /* Return total milliseconds of system time */
{
    struct rusage resource_usage;
    struct timeval *sys_time = &(resource_usage.ru_stime);

    getrusage(RUSAGE_SELF, &resource_usage);
    return(1000 * sys_time->tv_sec + ((int) sys_time->tv_usec/1000));
}

void printstats(FILE *fp) /* Print runtime statistics */
{
    fprintf(fp, "System time: %7.3f sec; User time: %7.3f msec; Memory: %d KB\n", systime() / 1000.0, usertime() / 1000.0, memory_stats());
}

/******************************************************************************/

