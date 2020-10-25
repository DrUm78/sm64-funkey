#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(TARGET_OD) || defined(TARGET_LINUX)
#include <sys/stat.h>
#include <errno.h>
#endif
#include "fsutils.h"

FILE *fopen_home(const char *filename, const char *mode)
{
#if defined(TARGET_OD) || defined(TARGET_LINUX)
    char fnamepath[2024];
    struct stat info;

    snprintf(fnamepath, sizeof(fnamepath), "%s/.sm64-port/", getenv("HOME"));
    if (stat(fnamepath, &info) != 0) {
        fprintf(stderr, "Creating '%s' for the first time...\n", fnamepath);
        mkdir(fnamepath, 0700);
        if (stat(fnamepath, &info) != 0) {
            fprintf(stderr, "Unable to create '%s': %s\n", fnamepath, strerror(errno));
            abort();
        }
    }

    snprintf(fnamepath, sizeof(fnamepath), "%s/.sm64-port/%s", getenv("HOME"), filename);
    return fopen(fnamepath, mode);
#else
    return fopen(filename, mode);
#endif
} 
