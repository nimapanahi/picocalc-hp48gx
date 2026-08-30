#pragma once

#include <stdbool.h>

typedef enum {
  PORT2_CARD_READY = 0,
  PORT2_CARD_CREATED,
  PORT2_CARD_RECOVERED,
  PORT2_CARD_NO_SD,
  PORT2_CARD_STORAGE_ERROR,
  PORT2_CARD_BAD_IMAGE,
  PORT2_CARD_IO_ERROR,
  PORT2_CARD_MODE_ERROR,
} port2_card_status_t;

/* Import one standard 128 KiB packed x48 card image. Fresh 2.1 installations
 * default to Port 1 so the stock ROM can merge it with user memory. Existing
 * Port 2 images keep their legacy slot unless PORT1.MODE or PORT2.MODE makes
 * the selection explicit. */
port2_card_status_t port2_card_init(void);

/* Export a dirty active card through its .NEW -> .CRD transaction, retaining
 * the previous committed image as .BAK. A clean or unattached card is a
 * successful no-op. */
bool port2_card_save(void);
const char *port2_card_status_label(void);
