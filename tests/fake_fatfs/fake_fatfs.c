#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
typedef DIR system_dir_t;

#include "ff.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char s_root[768];

void fake_fatfs_set_root(const char *path) {
  snprintf(s_root, sizeof(s_root), "%s", path ? path : "");
}

static bool host_path(const char *fat_path, char output[1024]) {
  if (!fat_path || strncmp(fat_path, "0:", 2) != 0 || s_root[0] == '\0')
    return false;
  int written = snprintf(output, 1024, "%s%s", s_root, fat_path + 2);
  return written >= 0 && written < 1024;
}

static FRESULT missing_result(void) {
  return errno == ENOENT ? FR_NO_FILE : FR_DISK_ERR;
}

FRESULT f_opendir(FF_DIR *directory, const char *path) {
  if (!directory || !host_path(path, directory->host_path))
    return FR_INVALID_NAME;
  system_dir_t *handle = opendir(directory->host_path);
  if (!handle) return errno == ENOENT ? FR_NO_PATH : FR_DISK_ERR;
  directory->handle = handle;
  return FR_OK;
}

FRESULT f_readdir(FF_DIR *directory, FILINFO *info) {
  if (!directory || !directory->handle || !info) return FR_INT_ERR;
  errno = 0;
  struct dirent *entry;
  do {
    entry = readdir((system_dir_t *)directory->handle);
  } while (entry && (strcmp(entry->d_name, ".") == 0 ||
                     strcmp(entry->d_name, "..") == 0));
  if (!entry) {
    info->fname[0] = '\0';
    return errno == 0 ? FR_OK : FR_DISK_ERR;
  }
  snprintf(info->fname, sizeof(info->fname), "%s", entry->d_name);
  char path[1280];
  int written = snprintf(path, sizeof(path), "%s/%s", directory->host_path,
                         entry->d_name);
  if (written < 0 || written >= (int)sizeof(path)) return FR_INVALID_NAME;
  struct stat metadata;
  if (stat(path, &metadata) != 0) return FR_DISK_ERR;
  info->fsize = (FSIZE_t)metadata.st_size;
  info->fattrib = S_ISDIR(metadata.st_mode) ? AM_DIR : 0;
  return FR_OK;
}

FRESULT f_closedir(FF_DIR *directory) {
  if (!directory || !directory->handle) return FR_INT_ERR;
  int result = closedir((system_dir_t *)directory->handle);
  directory->handle = NULL;
  return result == 0 ? FR_OK : FR_DISK_ERR;
}

FRESULT f_open(FIL *file, const char *path, uint8_t mode) {
  if (!file) return FR_INT_ERR;
  char translated[1024];
  if (!host_path(path, translated)) return FR_INVALID_NAME;
  FILE *stream = NULL;
  if ((mode & FA_WRITE) && (mode & FA_CREATE_NEW)) {
    int descriptor = open(translated, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (descriptor < 0) return errno == EEXIST ? FR_EXIST : missing_result();
    stream = fdopen(descriptor, "wb");
    if (!stream) {
      close(descriptor);
      return FR_DISK_ERR;
    }
  } else if ((mode & FA_WRITE) && (mode & FA_CREATE_ALWAYS)) {
    stream = fopen(translated, "wb");
  } else {
    stream = fopen(translated, "rb");
  }
  if (!stream) return missing_result();
  file->stream = stream;
  if (fseeko(stream, 0, SEEK_END) != 0) {
    fclose(stream);
    return FR_DISK_ERR;
  }
  off_t size = ftello(stream);
  if (size < 0 || fseeko(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return FR_DISK_ERR;
  }
  file->size = (FSIZE_t)size;
  return FR_OK;
}

FRESULT f_read(FIL *file, void *buffer, UINT length, UINT *read_count) {
  if (!file || !file->stream || !read_count) return FR_INT_ERR;
  *read_count = (UINT)fread(buffer, 1, length, file->stream);
  return *read_count == length || feof(file->stream) ? FR_OK : FR_DISK_ERR;
}

FRESULT f_write(FIL *file, const void *buffer, UINT length,
                UINT *write_count) {
  if (!file || !file->stream || !write_count) return FR_INT_ERR;
  *write_count = (UINT)fwrite(buffer, 1, length, file->stream);
  if (*write_count != length) return FR_DISK_ERR;
  file->size += length;
  return FR_OK;
}

FRESULT f_sync(FIL *file) {
  if (!file || !file->stream || fflush(file->stream) != 0) return FR_DISK_ERR;
  return fsync(fileno(file->stream)) == 0 ? FR_OK : FR_DISK_ERR;
}

FRESULT f_close(FIL *file) {
  if (!file || !file->stream) return FR_INT_ERR;
  int result = fclose(file->stream);
  file->stream = NULL;
  return result == 0 ? FR_OK : FR_DISK_ERR;
}

FRESULT f_stat(const char *path, FILINFO *info) {
  char translated[1024];
  if (!host_path(path, translated)) return FR_INVALID_NAME;
  struct stat metadata;
  if (stat(translated, &metadata) != 0) return missing_result();
  if (info) {
    info->fsize = (FSIZE_t)metadata.st_size;
    info->fattrib = S_ISDIR(metadata.st_mode) ? AM_DIR : 0;
  }
  return FR_OK;
}

FRESULT f_rename(const char *old_path, const char *new_path) {
  char old_translated[1024];
  char new_translated[1024];
  if (!host_path(old_path, old_translated) ||
      !host_path(new_path, new_translated))
    return FR_INVALID_NAME;
  return rename(old_translated, new_translated) == 0 ? FR_OK : FR_DISK_ERR;
}

FRESULT f_unlink(const char *path) {
  char translated[1024];
  if (!host_path(path, translated)) return FR_INVALID_NAME;
  return unlink(translated) == 0 ? FR_OK : missing_result();
}
