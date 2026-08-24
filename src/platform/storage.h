#pragma once

#include <stdbool.h>

typedef enum {
  STORAGE_READY = 0,
  STORAGE_NO_CARD,
  STORAGE_MOUNT_FAILED,
  STORAGE_FOLDER_FAILED,
} storage_status_t;

/* Mount the PicoCalc SD card without formatting or deleting anything. When a
 * writable FAT/exFAT card is present, this creates /HP48GX/PROGRAMS plus its
 * INBOX/OUTBOX directories and short guides if they do not already exist. */
storage_status_t storage_init(void);
void storage_shutdown(void);
bool storage_ready(void);
storage_status_t storage_status(void);
const char *storage_status_label(void);
