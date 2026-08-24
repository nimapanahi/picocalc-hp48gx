#include "object_files.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "rpl_object.h"
#include "storage.h"

#define INBOX_DIR "0:/HP48GX/PROGRAMS/INBOX"
#define OUTBOX_DIR "0:/HP48GX/PROGRAMS/OUTBOX"

static char s_selected[FF_LFN_BUF + 1];
static char s_last_output[16];

static bool extension_equal(const char *actual, const char *expected) {
  while (*actual && *expected) {
    if (toupper((unsigned char)*actual++) !=
        toupper((unsigned char)*expected++))
      return false;
  }
  return *actual == '\0' && *expected == '\0';
}

static bool supported_name(const char *name) {
  if (!name || name[0] == '\0' || name[0] == '.') return false;
  const char *extension = strrchr(name, '.');
  if (!extension) return false;
  return extension_equal(extension, ".48G") ||
         extension_equal(extension, ".48") ||
         extension_equal(extension, ".48P") ||
         extension_equal(extension, ".HP") ||
         extension_equal(extension, ".LIB") ||
         extension_equal(extension, ".BIN");
}

static object_files_status_t scan_next(void) {
  if (!storage_ready()) return OBJECT_FILES_NO_SD;
  DIR directory;
  if (f_opendir(&directory, INBOX_DIR) != FR_OK) return OBJECT_FILES_IO_ERROR;

  char first[FF_LFN_BUF + 1] = {0};
  char next[FF_LFN_BUF + 1] = {0};
  FILINFO info;
  FRESULT result = FR_OK;
  while ((result = f_readdir(&directory, &info)) == FR_OK &&
         info.fname[0] != '\0') {
    if ((info.fattrib & AM_DIR) || !supported_name(info.fname)) continue;
    if (first[0] == '\0' || strcmp(info.fname, first) < 0)
      snprintf(first, sizeof(first), "%s", info.fname);
    if (s_selected[0] != '\0' && strcmp(info.fname, s_selected) > 0 &&
        (next[0] == '\0' || strcmp(info.fname, next) < 0))
      snprintf(next, sizeof(next), "%s", info.fname);
  }
  FRESULT close_result = f_closedir(&directory);
  if (result != FR_OK || close_result != FR_OK) return OBJECT_FILES_IO_ERROR;
  if (first[0] == '\0') {
    s_selected[0] = '\0';
    return OBJECT_FILES_NO_INPUT;
  }
  snprintf(s_selected, sizeof(s_selected), "%s", next[0] ? next : first);
  return OBJECT_FILES_OK;
}

object_files_status_t object_files_init(void) {
  s_selected[0] = '\0';
  s_last_output[0] = '\0';
  return scan_next();
}

object_files_status_t object_files_cycle(void) { return scan_next(); }

static object_files_status_t from_rpl_status(hp48_rpl_status_t status) {
  switch (status) {
    case HP48_RPL_OK: return OBJECT_FILES_OK;
    case HP48_RPL_NOT_READY: return OBJECT_FILES_HP_NOT_READY;
    case HP48_RPL_BAD_OBJECT: return OBJECT_FILES_BAD_OBJECT;
    case HP48_RPL_NO_MEMORY: return OBJECT_FILES_HP_MEMORY_FULL;
    case HP48_RPL_GC_FAILED: return OBJECT_FILES_HP_GC_FAILED;
    case HP48_RPL_EMPTY_STACK: return OBJECT_FILES_EMPTY_STACK;
    case HP48_RPL_OUT_OF_RANGE: return OBJECT_FILES_TOO_LARGE;
    default: return OBJECT_FILES_BAD_OBJECT;
  }
}

object_files_status_t object_files_import_selected(void) {
  if (!storage_ready()) return OBJECT_FILES_NO_SD;
  if (s_selected[0] == '\0') {
    object_files_status_t scan = scan_next();
    if (scan != OBJECT_FILES_OK) return scan;
  }

  char path[sizeof(INBOX_DIR) + FF_LFN_BUF + 2];
  snprintf(path, sizeof(path), "%s/%s", INBOX_DIR, s_selected);
  FIL file;
  if (f_open(&file, path, FA_READ) != FR_OK) return OBJECT_FILES_IO_ERROR;

  static const uint8_t prefix[7] = {'H', 'P', 'H', 'P', '4', '8', '-'};
  uint8_t header[8];
  UINT got = 0;
  FRESULT result = f_read(&file, header, sizeof(header), &got);
  if (result != FR_OK || got != sizeof(header)) {
    f_close(&file);
    return OBJECT_FILES_BAD_HEADER;
  }
  if (memcmp(header, prefix, sizeof(prefix)) != 0) {
    f_close(&file);
    return OBJECT_FILES_BAD_HEADER;
  }
  /* The eighth byte is producer/ROM-revision metadata, not a calculator-model
   * discriminator. Real HP48 binaries in the wild use several values (for
   * example A, E, R, and W) while retaining the same packed RPL payload.
   * Compatibility is determined by validating that payload below. */
  FSIZE_t file_size = f_size(&file);
  if (file_size <= sizeof(header) || file_size - sizeof(header) > SIZE_MAX) {
    f_close(&file);
    return OBJECT_FILES_BAD_OBJECT;
  }

  hp48_rpl_import_t transfer;
  hp48_rpl_status_t rpl = hp48_rpl_import_begin(
      &transfer, (size_t)(file_size - sizeof(header)));
  if (rpl != HP48_RPL_OK) {
    f_close(&file);
    return from_rpl_status(rpl);
  }

  uint8_t buffer[256];
  while (transfer.bytes_written < transfer.payload_bytes) {
    UINT wanted = transfer.payload_bytes - transfer.bytes_written;
    if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
    got = 0;
    result = f_read(&file, buffer, wanted, &got);
    if (result != FR_OK || got != wanted ||
        hp48_rpl_import_write(&transfer, buffer, got) != HP48_RPL_OK) {
      hp48_rpl_import_abort(&transfer);
      f_close(&file);
      return OBJECT_FILES_IO_ERROR;
    }
  }
  FRESULT close_result = f_close(&file);
  if (close_result != FR_OK) {
    hp48_rpl_import_abort(&transfer);
    return OBJECT_FILES_IO_ERROR;
  }
  return from_rpl_status(hp48_rpl_import_commit(&transfer));
}

static bool path_missing(const char *path, bool *missing) {
  FILINFO info;
  FRESULT result = f_stat(path, &info);
  if (result == FR_NO_FILE || result == FR_NO_PATH) {
    *missing = true;
    return true;
  }
  *missing = false;
  return result == FR_OK;
}

object_files_status_t object_files_export_stack(void) {
  if (!storage_ready()) return OBJECT_FILES_NO_SD;
  hp48_rpl_export_t transfer;
  hp48_rpl_status_t rpl = hp48_rpl_export_begin(&transfer);
  if (rpl != HP48_RPL_OK) return from_rpl_status(rpl);

  char final_path[sizeof(OUTBOX_DIR) + 18];
  char temp_path[sizeof(OUTBOX_DIR) + 18];
  int slot = -1;
  for (int candidate = 0; candidate < 1000; ++candidate) {
    snprintf(s_last_output, sizeof(s_last_output), "STK%03d.48G", candidate);
    snprintf(final_path, sizeof(final_path), "%s/%s", OUTBOX_DIR,
             s_last_output);
    snprintf(temp_path, sizeof(temp_path), "%s/STK%03d.NEW", OUTBOX_DIR,
             candidate);
    bool final_missing = false;
    bool temp_missing = false;
    if (!path_missing(final_path, &final_missing) ||
        !path_missing(temp_path, &temp_missing))
      return OBJECT_FILES_IO_ERROR;
    if (final_missing && temp_missing) {
      slot = candidate;
      break;
    }
  }
  if (slot < 0) {
    s_last_output[0] = '\0';
    return OBJECT_FILES_OUTBOX_FULL;
  }
  FIL file;
  if (f_open(&file, temp_path, FA_WRITE | FA_CREATE_NEW) != FR_OK)
    return OBJECT_FILES_IO_ERROR;
  static const uint8_t header[8] = {'H', 'P', 'H', 'P', '4', '8', '-', 'W'};
  UINT written = 0;
  FRESULT result = f_write(&file, header, sizeof(header), &written);
  bool okay = result == FR_OK && written == sizeof(header);

  uint8_t buffer[256];
  size_t offset = 0;
  while (okay && offset < transfer.packed_bytes) {
    size_t chunk = transfer.packed_bytes - offset;
    if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
    okay = hp48_rpl_export_read(&transfer, offset, buffer, chunk) ==
           HP48_RPL_OK;
    if (!okay) break;
    written = 0;
    result = f_write(&file, buffer, (UINT)chunk, &written);
    okay = result == FR_OK && written == chunk;
    offset += chunk;
  }
  if (okay) okay = f_sync(&file) == FR_OK;
  if (f_close(&file) != FR_OK) okay = false;
  if (!okay || f_rename(temp_path, final_path) != FR_OK) {
    /* This temporary was created successfully by this call, so removing it
     * cannot touch a pre-existing user file. */
    f_unlink(temp_path);
    return OBJECT_FILES_IO_ERROR;
  }
  return OBJECT_FILES_OK;
}

const char *object_files_selected_name(void) { return s_selected; }
const char *object_files_last_output_name(void) { return s_last_output; }

const char *object_files_status_label(object_files_status_t status) {
  switch (status) {
    case OBJECT_FILES_OK: return "OK";
    case OBJECT_FILES_NO_SD: return "NO SD CARD";
    case OBJECT_FILES_NO_INPUT: return "INBOX EMPTY";
    case OBJECT_FILES_BAD_HEADER: return "NOT AN HP BINARY";
    case OBJECT_FILES_IO_ERROR: return "SD FILE ERROR";
    case OBJECT_FILES_HP_NOT_READY: return "HP NOT READY";
    case OBJECT_FILES_BAD_OBJECT: return "BAD HP OBJECT";
    case OBJECT_FILES_HP_MEMORY_FULL: return "HP MEMORY FULL";
    case OBJECT_FILES_HP_GC_FAILED: return "HP GC FAILED";
    case OBJECT_FILES_EMPTY_STACK: return "STACK EMPTY";
    case OBJECT_FILES_TOO_LARGE: return "OBJECT TOO LARGE";
    case OBJECT_FILES_OUTBOX_FULL: return "OUTBOX FULL";
    default: return "FILE ERROR";
  }
}
