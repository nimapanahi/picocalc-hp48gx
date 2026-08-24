#pragma once

#include <stdbool.h>

typedef enum {
  OBJECT_FILES_OK = 0,
  OBJECT_FILES_NO_SD,
  OBJECT_FILES_NO_INPUT,
  OBJECT_FILES_BAD_HEADER,
  OBJECT_FILES_IO_ERROR,
  OBJECT_FILES_HP_NOT_READY,
  OBJECT_FILES_BAD_OBJECT,
  OBJECT_FILES_HP_MEMORY_FULL,
  OBJECT_FILES_HP_GC_FAILED,
  OBJECT_FILES_EMPTY_STACK,
  OBJECT_FILES_TOO_LARGE,
  OBJECT_FILES_OUTBOX_FULL,
} object_files_status_t;

/* Scan /HP48GX/PROGRAMS/INBOX and select its first supported file. */
object_files_status_t object_files_init(void);

/* Select the next supported file, wrapping to the first one. */
object_files_status_t object_files_cycle(void);

/* Import the selected HPHP48-x binary object onto stack level 1. The final
 * header byte identifies its producer/ROM revision, not a different model. */
object_files_status_t object_files_import_selected(void);

/* Export stack level 1 to the first unused OUTBOX/STKnnn.48G file. */
object_files_status_t object_files_export_stack(void);

const char *object_files_selected_name(void);
const char *object_files_last_output_name(void);
const char *object_files_status_label(object_files_status_t status);
