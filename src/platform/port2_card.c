#include "port2_card.h"

#include <stdio.h>

#include "core_runtime.h"
#include "ff.h"
#include "storage.h"

#define PORT2_IMAGE "0:/HP48GX/PROGRAMS/PORT2.CRD"
#define PORT2_NEW "0:/HP48GX/PROGRAMS/PORT2.NEW"
#define PORT2_BACKUP "0:/HP48GX/PROGRAMS/PORT2.BAK"
#define PORT1_MODE "0:/HP48GX/PROGRAMS/PORT1.MODE"
#define PORT1_IMAGE "0:/HP48GX/PROGRAMS/PORT1.CRD"
#define PORT1_NEW "0:/HP48GX/PROGRAMS/PORT1.NEW"
#define PORT1_BACKUP "0:/HP48GX/PROGRAMS/PORT1.BAK"

typedef enum {
  IMAGE_OK,
  IMAGE_MISSING,
  IMAGE_BAD_SIZE,
  IMAGE_IO_ERROR,
} image_result_t;

static port2_card_status_t s_status = PORT2_CARD_NO_SD;
static hp48_card_slot_t s_slot = HP48_CARD_PORT2;

static const char *image_path(void) {
  return s_slot == HP48_CARD_PORT1 ? PORT1_IMAGE : PORT2_IMAGE;
}

static const char *new_path(void) {
  return s_slot == HP48_CARD_PORT1 ? PORT1_NEW : PORT2_NEW;
}

static const char *backup_path(void) {
  return s_slot == HP48_CARD_PORT1 ? PORT1_BACKUP : PORT2_BACKUP;
}

static image_result_t image_info(const char *path) {
  FILINFO info;
  FRESULT result = f_stat(path, &info);
  if (result == FR_NO_FILE || result == FR_NO_PATH) return IMAGE_MISSING;
  if (result != FR_OK) return IMAGE_IO_ERROR;
  return info.fsize == HP48_PORT2_PACKED_SIZE ? IMAGE_OK : IMAGE_BAD_SIZE;
}

static image_result_t load_image(const char *path) {
  image_result_t info = image_info(path);
  if (info != IMAGE_OK) return info;

  if (s_slot == HP48_CARD_PORT1) hp48_port1_attach_blank();
  else hp48_port2_attach_blank();
  FIL file;
  if (f_open(&file, path, FA_READ) != FR_OK) {
    hp48_port2_detach();
    return IMAGE_IO_ERROR;
  }
  UINT read = 0;
  FRESULT result = f_read(&file, hp48_port2_packed_data(),
                          HP48_PORT2_PACKED_SIZE, &read);
  FRESULT close_result = f_close(&file);
  if (result != FR_OK || close_result != FR_OK ||
      read != HP48_PORT2_PACKED_SIZE) {
    hp48_port2_detach();
    return IMAGE_IO_ERROR;
  }
  hp48_port2_mark_clean();
  return IMAGE_OK;
}

static bool unlink_if_present(const char *path) {
  FRESULT result = f_unlink(path);
  return result == FR_OK || result == FR_NO_FILE || result == FR_NO_PATH;
}

bool port2_card_save(void) {
  if (hp48_card_slot() == HP48_CARD_NONE || !hp48_port2_dirty()) return true;
  if (!storage_ready()) return false;
  const char *current_path = image_path();
  const char *pending_path = new_path();
  const char *previous_path = backup_path();
  if (!unlink_if_present(pending_path)) return false;

  FIL file;
  if (f_open(&file, pending_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    return false;
  UINT written = 0;
  FRESULT result = f_write(&file, hp48_port2_packed_data(),
                           HP48_PORT2_PACKED_SIZE, &written);
  if (result == FR_OK && written == HP48_PORT2_PACKED_SIZE)
    result = f_sync(&file);
  FRESULT close_result = f_close(&file);
  if (result != FR_OK || close_result != FR_OK ||
      written != HP48_PORT2_PACKED_SIZE) {
    unlink_if_present(pending_path);
    return false;
  }

  bool moved_previous = false;
  image_result_t current = image_info(current_path);
  if (current == IMAGE_OK) {
    if (!unlink_if_present(previous_path) ||
        f_rename(current_path, previous_path) != FR_OK) {
      unlink_if_present(pending_path);
      return false;
    }
    moved_previous = true;
  } else if (current != IMAGE_MISSING) {
    /* Never replace an unexpected user file. */
    unlink_if_present(pending_path);
    return false;
  }

  if (f_rename(pending_path, current_path) != FR_OK) {
    if (moved_previous) f_rename(previous_path, current_path);
    unlink_if_present(pending_path);
    return false;
  }

  hp48_port2_mark_clean();
  return true;
}

static port2_card_status_t recover_missing_image(void) {
  const char *current_path = image_path();
  const char *pending_path = new_path();
  const char *previous_path = backup_path();
  image_result_t pending = load_image(pending_path);
  if (pending == IMAGE_OK) {
    if (f_rename(pending_path, current_path) != FR_OK) {
      hp48_port2_detach();
      return PORT2_CARD_IO_ERROR;
    }
    return PORT2_CARD_RECOVERED;
  }

  /* PORT2.NEW belongs to us and may be incomplete after power loss. Ignore it
   * when a valid committed backup is available. */
  if (pending != IMAGE_MISSING) hp48_port2_detach();

  image_result_t backup = load_image(previous_path);
  if (backup == IMAGE_OK) {
    if (f_rename(previous_path, current_path) != FR_OK) {
      hp48_port2_detach();
      return PORT2_CARD_IO_ERROR;
    }
    return PORT2_CARD_RECOVERED;
  }
  if (backup == IMAGE_BAD_SIZE || backup == IMAGE_IO_ERROR) {
    hp48_port2_detach();
    return backup == IMAGE_BAD_SIZE ? PORT2_CARD_BAD_IMAGE
                                    : PORT2_CARD_IO_ERROR;
  }

  if (pending == IMAGE_BAD_SIZE || pending == IMAGE_IO_ERROR) {
    hp48_port2_detach();
    return pending == IMAGE_BAD_SIZE ? PORT2_CARD_BAD_IMAGE
                                     : PORT2_CARD_IO_ERROR;
  }

  if (s_slot == HP48_CARD_PORT1) hp48_port1_attach_blank();
  else hp48_port2_attach_blank();
  if (!port2_card_save()) {
    hp48_port2_detach();
    return PORT2_CARD_IO_ERROR;
  }
  return PORT2_CARD_CREATED;
}

port2_card_status_t port2_card_init(void) {
  hp48_port2_detach();
  if (!storage_ready()) {
    s_status = storage_status() == STORAGE_NO_CARD
                   ? PORT2_CARD_NO_SD
                   : PORT2_CARD_STORAGE_ERROR;
    return s_status;
  }

  FILINFO mode_info;
  FRESULT mode_result = f_stat(PORT1_MODE, &mode_info);
  if (mode_result != FR_OK && mode_result != FR_NO_FILE &&
      mode_result != FR_NO_PATH) {
    s_status = PORT2_CARD_IO_ERROR;
    return s_status;
  }
  s_slot = mode_result == FR_OK ? HP48_CARD_PORT1 : HP48_CARD_PORT2;

  image_result_t current = load_image(image_path());
  if (current == IMAGE_OK) {
    s_status = PORT2_CARD_READY;
  } else if (current == IMAGE_MISSING) {
    s_status = recover_missing_image();
  } else if (current == IMAGE_BAD_SIZE) {
    s_status = PORT2_CARD_BAD_IMAGE;
  } else {
    s_status = PORT2_CARD_IO_ERROR;
  }

  printf("[HP48] Virtual card: %s\n", port2_card_status_label());
  return s_status;
}

const char *port2_card_status_label(void) {
  switch (s_status) {
    case PORT2_CARD_READY:
      return s_slot == HP48_CARD_PORT1 ? "P1 READY" : "P2 READY";
    case PORT2_CARD_CREATED:
      return s_slot == HP48_CARD_PORT1 ? "P1 NEW" : "P2 NEW";
    case PORT2_CARD_RECOVERED:
      return s_slot == HP48_CARD_PORT1 ? "P1 RECOVERED" : "P2 RECOVERED";
    case PORT2_CARD_NO_SD:
      return s_slot == HP48_CARD_PORT1 ? "P1 NO SD" : "P2 NO SD";
    case PORT2_CARD_STORAGE_ERROR:
      return s_slot == HP48_CARD_PORT1 ? "P1 SD ERR" : "P2 SD ERR";
    case PORT2_CARD_BAD_IMAGE:
      return s_slot == HP48_CARD_PORT1 ? "P1 BAD IMAGE" : "P2 BAD IMAGE";
    case PORT2_CARD_IO_ERROR:
      return s_slot == HP48_CARD_PORT1 ? "P1 IO ERR" : "P2 IO ERR";
    default: return "CARD ERR";
  }
}
