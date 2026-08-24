#pragma once

#include <stdint.h>
#include <stdio.h>

#define FF_LFN_BUF 255

typedef uint32_t UINT;
typedef uint64_t FSIZE_t;
typedef int FRESULT;

enum {
  FR_OK = 0,
  FR_DISK_ERR,
  FR_INT_ERR,
  FR_NOT_READY,
  FR_NO_FILE,
  FR_NO_PATH,
  FR_INVALID_NAME,
  FR_DENIED,
  FR_EXIST,
};

#define AM_DIR 0x10u

#define FA_READ 0x01u
#define FA_WRITE 0x02u
#define FA_CREATE_NEW 0x04u
#define FA_CREATE_ALWAYS 0x08u

typedef struct {
  FILE *stream;
  FSIZE_t size;
} FIL;

typedef struct {
  void *handle;
  char host_path[1024];
} FF_DIR;

/* FatFs calls this type DIR. Keep the POSIX DIR name available privately to
 * fake_fatfs.c by defining this macro only after it includes <dirent.h>. */
#define DIR FF_DIR

typedef struct {
  FSIZE_t fsize;
  uint8_t fattrib;
  char fname[FF_LFN_BUF + 1];
} FILINFO;

#define f_size(file) ((file)->size)

void fake_fatfs_set_root(const char *path);
FRESULT f_opendir(FF_DIR *directory, const char *path);
FRESULT f_readdir(FF_DIR *directory, FILINFO *info);
FRESULT f_closedir(FF_DIR *directory);
FRESULT f_open(FIL *file, const char *path, uint8_t mode);
FRESULT f_read(FIL *file, void *buffer, UINT length, UINT *read_count);
FRESULT f_write(FIL *file, const void *buffer, UINT length,
                UINT *write_count);
FRESULT f_sync(FIL *file);
FRESULT f_close(FIL *file);
FRESULT f_stat(const char *path, FILINFO *info);
FRESULT f_rename(const char *old_path, const char *new_path);
FRESULT f_unlink(const char *path);
