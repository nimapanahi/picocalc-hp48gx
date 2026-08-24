#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core_runtime.h"
#include "ff.h"
#include "port2_card.h"
#include "storage.h"

static bool s_storage_ready;
static storage_status_t s_storage_status = STORAGE_NO_CARD;
static uint8_t s_port2[HP48_PORT2_PACKED_SIZE];
static bool s_attached;
static bool s_dirty;
static hp48_card_slot_t s_slot;

bool storage_ready(void) { return s_storage_ready; }
storage_status_t storage_status(void) { return s_storage_status; }

void hp48_port2_attach_blank(void) {
  memset(s_port2, 0, sizeof(s_port2));
  s_attached = true;
  s_dirty = true;
  s_slot = HP48_CARD_PORT2;
}

void hp48_port1_attach_blank(void) {
  memset(s_port2, 0, sizeof(s_port2));
  s_attached = true;
  s_dirty = true;
  s_slot = HP48_CARD_PORT1;
}

void hp48_port2_detach(void) {
  s_attached = false;
  s_dirty = false;
  s_slot = HP48_CARD_NONE;
}

bool hp48_port2_attached(void) { return s_attached; }
bool hp48_port2_dirty(void) { return s_attached && s_dirty; }
void hp48_port2_mark_clean(void) { s_dirty = false; }
uint8_t *hp48_port2_packed_data(void) {
  return s_attached ? s_port2 : NULL;
}
hp48_card_slot_t hp48_card_slot(void) { return s_slot; }

static void make_path(char output[1024], const char *root, const char *name) {
  int written = snprintf(output, 1024, "%s/HP48GX/PROGRAMS/%s", root, name);
  assert(written > 0 && written < 1024);
}

static bool path_exists(const char *path) {
  struct stat metadata;
  return stat(path, &metadata) == 0;
}

static void remove_if_present(const char *path) {
  if (unlink(path) != 0) assert(!path_exists(path));
}

static void clear_images(const char *root) {
  char path[1024];
  make_path(path, root, "PORT2.CRD");
  remove_if_present(path);
  make_path(path, root, "PORT2.NEW");
  remove_if_present(path);
  make_path(path, root, "PORT2.BAK");
  remove_if_present(path);
  make_path(path, root, "PORT1.MODE");
  remove_if_present(path);
  make_path(path, root, "PORT1.CRD");
  remove_if_present(path);
  make_path(path, root, "PORT1.NEW");
  remove_if_present(path);
  make_path(path, root, "PORT1.BAK");
  remove_if_present(path);
}

static void write_pattern(const char *path, size_t size, uint8_t value) {
  FILE *file = fopen(path, "wb");
  assert(file);
  uint8_t block[1024];
  memset(block, value, sizeof(block));
  while (size != 0) {
    size_t chunk = size < sizeof(block) ? size : sizeof(block);
    assert(fwrite(block, 1, chunk, file) == chunk);
    size -= chunk;
  }
  assert(fclose(file) == 0);
}

static void verify_pattern(const char *path, size_t size, uint8_t value) {
  FILE *file = fopen(path, "rb");
  assert(file);
  uint8_t block[1024];
  size_t remaining = size;
  while (remaining != 0) {
    size_t chunk = remaining < sizeof(block) ? remaining : sizeof(block);
    assert(fread(block, 1, chunk, file) == chunk);
    for (size_t i = 0; i < chunk; ++i) assert(block[i] == value);
    remaining -= chunk;
  }
  assert(fgetc(file) == EOF);
  assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const char *root = argv[1];
  fake_fatfs_set_root(root);

  char current[1024];
  char pending[1024];
  char backup[1024];
  make_path(current, root, "PORT2.CRD");
  make_path(pending, root, "PORT2.NEW");
  make_path(backup, root, "PORT2.BAK");
  clear_images(root);

  s_storage_ready = false;
  s_storage_status = STORAGE_NO_CARD;
  assert(port2_card_init() == PORT2_CARD_NO_SD);
  assert(!s_attached);
  s_storage_status = STORAGE_MOUNT_FAILED;
  assert(port2_card_init() == PORT2_CARD_STORAGE_ERROR);
  assert(!s_attached);

  s_storage_ready = true;
  s_storage_status = STORAGE_READY;
  assert(port2_card_init() == PORT2_CARD_CREATED);
  assert(s_attached && !s_dirty);
  verify_pattern(current, HP48_PORT2_PACKED_SIZE, 0x00);
  assert(!path_exists(pending));
  assert(!path_exists(backup));

  memset(s_port2, 0xa5, sizeof(s_port2));
  s_dirty = true;
  assert(port2_card_save());
  assert(!s_dirty);
  verify_pattern(current, HP48_PORT2_PACKED_SIZE, 0xa5);
  verify_pattern(backup, HP48_PORT2_PACKED_SIZE, 0x00);
  assert(!path_exists(pending));

  memset(s_port2, 0, sizeof(s_port2));
  assert(port2_card_init() == PORT2_CARD_READY);
  assert(s_attached && !s_dirty && s_port2[0] == 0xa5 &&
         s_port2[sizeof(s_port2) - 1] == 0xa5);

  /* A complete temporary image is authoritative when power was lost after
   * the previous image was moved away but before promotion completed. */
  assert(unlink(current) == 0);
  write_pattern(pending, HP48_PORT2_PACKED_SIZE, 0x5a);
  assert(port2_card_init() == PORT2_CARD_RECOVERED);
  assert(s_attached && !s_dirty && s_port2[0] == 0x5a);
  verify_pattern(current, HP48_PORT2_PACKED_SIZE, 0x5a);
  assert(!path_exists(pending));

  /* An incomplete temporary is ignored in favor of a complete committed
   * backup. The bad temporary is preserved for diagnosis until a later save. */
  assert(unlink(current) == 0);
  write_pattern(pending, 17, 0xee);
  write_pattern(backup, HP48_PORT2_PACKED_SIZE, 0x3c);
  assert(port2_card_init() == PORT2_CARD_RECOVERED);
  assert(s_attached && !s_dirty && s_port2[0] == 0x3c);
  verify_pattern(current, HP48_PORT2_PACKED_SIZE, 0x3c);
  verify_pattern(pending, 17, 0xee);
  assert(!path_exists(backup));

  /* Never replace an unexpected current file, even when a valid backup is
   * available or the in-memory card is dirty. */
  clear_images(root);
  write_pattern(current, 23, 0xbd);
  write_pattern(backup, HP48_PORT2_PACKED_SIZE, 0x77);
  assert(port2_card_init() == PORT2_CARD_BAD_IMAGE);
  assert(!s_attached);
  verify_pattern(current, 23, 0xbd);
  verify_pattern(backup, HP48_PORT2_PACKED_SIZE, 0x77);

  hp48_port2_attach_blank();
  memset(s_port2, 0x99, sizeof(s_port2));
  assert(!port2_card_save());
  assert(s_dirty);
  verify_pattern(current, 23, 0xbd);
  verify_pattern(backup, HP48_PORT2_PACKED_SIZE, 0x77);
  assert(!path_exists(pending));

  /* A marker selects a separately persisted non-covered Port 1 card for old
   * machine-language libraries, reusing the single packed SRAM allocation. */
  clear_images(root);
  char mode[1024];
  char port1_current[1024];
  make_path(mode, root, "PORT1.MODE");
  make_path(port1_current, root, "PORT1.CRD");
  write_pattern(mode, 0, 0);
  assert(port2_card_init() == PORT2_CARD_CREATED);
  assert(s_attached && !s_dirty && s_slot == HP48_CARD_PORT1);
  verify_pattern(port1_current, HP48_PORT2_PACKED_SIZE, 0x00);
  memset(s_port2, 0x48, sizeof(s_port2));
  s_dirty = true;
  assert(port2_card_save());
  assert(!s_dirty);
  verify_pattern(port1_current, HP48_PORT2_PACKED_SIZE, 0x48);

  clear_images(root);
  return 0;
}
