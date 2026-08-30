#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  HP48_RPL_OK = 0,
  HP48_RPL_NOT_READY,
  HP48_RPL_BAD_OBJECT,
  HP48_RPL_NO_MEMORY,
  HP48_RPL_GC_FAILED,
  HP48_RPL_EMPTY_STACK,
  HP48_RPL_OUT_OF_RANGE,
} hp48_rpl_status_t;

/* An import is streamed straight into a newly allocated HP temporary object.
 * This avoids keeping a second copy of a potentially large object in RP2350
 * SRAM. Abort restores every RPL memory pointer and moves the return stack
 * back to its original location. */
typedef struct {
  uint32_t object_address;
  uint32_t payload_bytes;
  uint32_t bytes_written;
  uint32_t temp_top_before;
  uint32_t return_top_before;
  uint32_t data_top_before;
  uint32_t avmem_before;
  uint32_t allocation_nibbles;
  bool active;
} hp48_rpl_import_t;

typedef struct {
  uint32_t object_address;
  uint32_t object_nibbles;
  uint32_t packed_bytes;
  bool active;
} hp48_rpl_export_t;

/* Run the revision-R ROM's own =GARBAGECOL entry while preserving the exact
 * emulated CPU/register state. HP RAM compaction is intentionally retained. */
bool hp48_rpl_collect_garbage(void);

hp48_rpl_status_t hp48_rpl_import_begin(hp48_rpl_import_t *transfer,
                                        size_t payload_bytes);
hp48_rpl_status_t hp48_rpl_import_write(hp48_rpl_import_t *transfer,
                                        const uint8_t *packed,
                                        size_t length);
hp48_rpl_status_t hp48_rpl_import_commit(hp48_rpl_import_t *transfer);
void hp48_rpl_import_abort(hp48_rpl_import_t *transfer);

hp48_rpl_status_t hp48_rpl_export_begin(hp48_rpl_export_t *transfer);
hp48_rpl_status_t hp48_rpl_export_read(const hp48_rpl_export_t *transfer,
                                       size_t offset, uint8_t *packed,
                                       size_t length);

unsigned hp48_rpl_stack_depth(void);
bool hp48_rpl_stack_level1_is_directory(void);
/* The revision-R desktop pointer moves into the Port 1 address range after
 * the stock MERGE1 command expands user memory. */
bool hp48_rpl_port1_is_merged(void);
const char *hp48_rpl_status_label(hp48_rpl_status_t status);
