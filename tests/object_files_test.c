#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ff.h"
#include "object_files.h"
#include "rpl_object.h"

static const uint8_t k_object[] = {
    0x2c, 0x2a, 0x50, 0x01, 0x00, 0x50, 0x49,
    0x43, 0x4f, 0x43, 0x41, 0x4c, 0x43,
};

static bool s_storage_ready = true;
static uint8_t s_imported[sizeof(k_object)];
static size_t s_imported_size;
static unsigned s_import_begin_calls;

bool storage_ready(void) { return s_storage_ready; }

hp48_rpl_status_t hp48_rpl_import_begin(hp48_rpl_import_t *transfer,
                                        size_t payload_bytes) {
  ++s_import_begin_calls;
  if (!transfer || payload_bytes > sizeof(s_imported))
    return HP48_RPL_OUT_OF_RANGE;
  memset(transfer, 0, sizeof(*transfer));
  transfer->payload_bytes = (uint32_t)payload_bytes;
  transfer->active = true;
  s_imported_size = 0;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_import_write(hp48_rpl_import_t *transfer,
                                        const uint8_t *packed,
                                        size_t length) {
  if (!transfer || !transfer->active ||
      s_imported_size + length > sizeof(s_imported))
    return HP48_RPL_OUT_OF_RANGE;
  memcpy(s_imported + s_imported_size, packed, length);
  s_imported_size += length;
  transfer->bytes_written += (uint32_t)length;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_import_commit(hp48_rpl_import_t *transfer) {
  if (!transfer || !transfer->active ||
      transfer->bytes_written != transfer->payload_bytes)
    return HP48_RPL_BAD_OBJECT;
  transfer->active = false;
  return HP48_RPL_OK;
}

void hp48_rpl_import_abort(hp48_rpl_import_t *transfer) {
  if (transfer) transfer->active = false;
}

hp48_rpl_status_t hp48_rpl_export_begin(hp48_rpl_export_t *transfer) {
  memset(transfer, 0, sizeof(*transfer));
  transfer->packed_bytes = sizeof(k_object);
  transfer->object_nibbles = sizeof(k_object) * 2u;
  transfer->active = true;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_export_read(const hp48_rpl_export_t *transfer,
                                       size_t offset, uint8_t *packed,
                                       size_t length) {
  if (!transfer || !transfer->active || offset + length > sizeof(k_object))
    return HP48_RPL_OUT_OF_RANGE;
  memcpy(packed, k_object + offset, length);
  return HP48_RPL_OK;
}

static void make_path(char output[1024], const char *root, const char *suffix) {
  int written = snprintf(output, 1024, "%s/%s", root, suffix);
  assert(written > 0 && written < 1024);
}

static void verify_export(const char *root, const char *name) {
  char path[1024];
  make_path(path, root, name);
  FILE *file = fopen(path, "rb");
  assert(file);
  uint8_t expected[8 + sizeof(k_object)] = {
      'H', 'P', 'H', 'P', '4', '8', '-', 'W',
  };
  memcpy(expected + 8, k_object, sizeof(k_object));
  uint8_t actual[sizeof(expected)];
  assert(fread(actual, 1, sizeof(actual), file) == sizeof(actual));
  assert(fgetc(file) == EOF);
  assert(fclose(file) == 0);
  assert(memcmp(actual, expected, sizeof(expected)) == 0);
}

static void create_file(const char *path, const void *data, size_t size) {
  FILE *file = fopen(path, "wb");
  assert(file);
  assert(fwrite(data, 1, size, file) == size);
  assert(fclose(file) == 0);
}

static void create_hp_file(const char *path, uint8_t revision) {
  uint8_t contents[8 + sizeof(k_object)] = {
      'H', 'P', 'H', 'P', '4', '8', '-', 0,
  };
  contents[7] = revision;
  memcpy(contents + 8, k_object, sizeof(k_object));
  create_file(path, contents, sizeof(contents));
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const char *root = argv[1];
  fake_fatfs_set_root(root);

  s_storage_ready = false;
  assert(object_files_init() == OBJECT_FILES_NO_SD);
  s_storage_ready = true;

  assert(object_files_init() == OBJECT_FILES_OK);
  assert(strcmp(object_files_selected_name(), "PICOCALC.48G") == 0);
  assert(object_files_cycle() == OBJECT_FILES_OK);
  assert(strcmp(object_files_selected_name(), "ZZZ.48G") == 0);
  assert(object_files_cycle() == OBJECT_FILES_OK);
  assert(strcmp(object_files_selected_name(), "PICOCALC.48G") == 0);

  assert(object_files_import_selected() == OBJECT_FILES_OK);
  assert(s_imported_size == sizeof(k_object));
  assert(memcmp(s_imported, k_object, sizeof(k_object)) == 0);
  verify_export(root, "HP48GX/PROGRAMS/INBOX/PICOCALC.48G");

  /* HP48 binary headers use multiple producer/ROM revision bytes. In
   * particular, hpcalc.org's TETRISGX.LIB uses HPHP48-D and must not be
   * rejected merely because our exporter uses W. */
  char revision_input[1024];
  make_path(revision_input, root, "HP48GX/PROGRAMS/INBOX/TETRISGX.LIB");
  create_hp_file(revision_input, 'D');
  assert(object_files_cycle() == OBJECT_FILES_OK);
  assert(strcmp(object_files_selected_name(), "TETRISGX.LIB") == 0);
  s_imported_size = 0;
  assert(object_files_import_selected() == OBJECT_FILES_OK);
  assert(s_imported_size == sizeof(k_object));
  assert(memcmp(s_imported, k_object, sizeof(k_object)) == 0);
  assert(unlink(revision_input) == 0);

  assert(object_files_export_stack() == OBJECT_FILES_OK);
  assert(strcmp(object_files_last_output_name(), "STK000.48G") == 0);
  verify_export(root, "HP48GX/PROGRAMS/OUTBOX/STK000.48G");
  assert(object_files_export_stack() == OBJECT_FILES_OK);
  assert(strcmp(object_files_last_output_name(), "STK001.48G") == 0);
  verify_export(root, "HP48GX/PROGRAMS/OUTBOX/STK001.48G");

  char protected_temp[1024];
  make_path(protected_temp, root,
            "HP48GX/PROGRAMS/OUTBOX/STK002.NEW");
  static const char keep[] = "USER FILE - DO NOT TOUCH";
  create_file(protected_temp, keep, sizeof(keep));
  assert(object_files_export_stack() == OBJECT_FILES_OK);
  assert(strcmp(object_files_last_output_name(), "STK003.48G") == 0);
  verify_export(root, "HP48GX/PROGRAMS/OUTBOX/STK003.48G");
  FILE *temp = fopen(protected_temp, "rb");
  assert(temp);
  char unchanged[sizeof(keep)];
  assert(fread(unchanged, 1, sizeof(unchanged), temp) == sizeof(unchanged));
  assert(fclose(temp) == 0);
  assert(memcmp(unchanged, keep, sizeof(keep)) == 0);

  char bad_file[1024];
  make_path(bad_file, root, "HP48GX/PROGRAMS/INBOX/AAA.BIN");
  static const char bad[] = "not an HP binary";
  create_file(bad_file, bad, sizeof(bad));
  unsigned begins_before = s_import_begin_calls;
  assert(object_files_init() == OBJECT_FILES_OK);
  assert(strcmp(object_files_selected_name(), "AAA.BIN") == 0);
  assert(object_files_import_selected() == OBJECT_FILES_BAD_HEADER);
  assert(s_import_begin_calls == begins_before);
  assert(unlink(bad_file) == 0);

  char first_input[1024];
  char second_input[1024];
  make_path(first_input, root,
            "HP48GX/PROGRAMS/INBOX/PICOCALC.48G");
  make_path(second_input, root, "HP48GX/PROGRAMS/INBOX/ZZZ.48G");
  assert(unlink(first_input) == 0);
  assert(unlink(second_input) == 0);
  assert(object_files_init() == OBJECT_FILES_NO_INPUT);

  return 0;
}
