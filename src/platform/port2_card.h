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
} port2_card_status_t;

/* Import a standard 128 KiB packed x48 card image. PORT2.CRD is the default.
 * If /HP48GX/PROGRAMS/PORT1.MODE exists, mount PORT1.CRD instead as the GX's
 * non-covered Port 1 for old machine-language library compatibility. */
port2_card_status_t port2_card_init(void);

/* Export a dirty active card through its .NEW -> .CRD transaction, retaining
 * the previous committed image as .BAK. A clean or unattached card is a
 * successful no-op. */
bool port2_card_save(void);
const char *port2_card_status_label(void);
