#include "rpl_object.h"

#include <limits.h>
#include <string.h>

#include "hp48_emu.h"

/* HP 48G/GX System RPL variables in revision-R system RAM. */
#define TEMPTOP  0x806eeu
#define RSKTOP   0x806f3u
#define DSKTOP   0x806f8u
#define EDITLINE 0x806fdu
#define AVMEM    0x807edu
#define GARBAGECOL 0x0613eu

#define DOBINT   0x02911u
#define DOREAL   0x02933u
#define DOEREL   0x02955u
#define DOCMP    0x02977u
#define DOECMP   0x0299du
#define DOCHAR   0x029bfu
#define DOARRY   0x029e8u
#define DOLNKARRY 0x02a0au
#define DOCSTR   0x02a2cu
#define DOHSTR   0x02a4eu
#define DOLIST   0x02a74u
#define DORRP    0x02a96u
#define DOSYMB   0x02ab8u
#define DOEXT    0x02adau
#define DOTAG    0x02afcu
#define DOGROB   0x02b1eu
#define DOLIB    0x02b40u
#define DOBAK    0x02b62u
#define DOEXT0   0x02b88u
#define DOEXT1   0x02baau
#define DOEXT2   0x02bccu
#define DOEXT3   0x02beeu
#define DOEXT4   0x02c10u
#define DOCOL    0x02d9du
#define DOCODE   0x02dccu
#define DOIDNT   0x02e48u
#define DOLAM    0x02e6du
#define DOROMP   0x02e92u
#define SEMI     0x0312bu

#define SATURN_ADDRESS_SPACE 0x100000u
#define MAX_OBJECT_NESTING 64u
#define GC_INSTRUCTION_BUDGET 10000000u
#define BAD_SIZE SIZE_MAX

extern int device_check;
extern int enter_debugger;
extern long schedule_event;
extern unsigned long instructions;

static uint32_t read5(uint32_t address) {
  return (uint32_t)read_nibbles((long)address, 5) & 0xfffffu;
}

static void write5(uint32_t address, uint32_t value) {
  write_nibbles((long)address, (long)(value & 0xfffffu), 5);
}

static bool span_ok(uint32_t address, size_t length) {
  return address < SATURN_ADDRESS_SPACE &&
         length <= SATURN_ADDRESS_SPACE - address;
}

/* Bounded adaptation of Emu48's RPL_ObjectSize. Unknown five-nibble values
 * are valid threaded System RPL pointers inside programs, so the default size
 * is deliberately five rather than an error. */
static size_t object_size(uint32_t address, size_t available, unsigned depth) {
  if (depth > MAX_OBJECT_NESTING || available < 5 ||
      !span_ok(address, available))
    return BAD_SIZE;

  uint32_t prolog = read5(address);
  size_t length = 0;
  switch (prolog) {
    case DOBINT: length = 10; break;
    case DOREAL: length = 21; break;
    case DOEREL: length = 26; break;
    case DOCMP: length = 37; break;
    case DOECMP: length = 47; break;
    case DOCHAR: length = 7; break;
    case DOROMP: length = 11; break;

    case DOLIST:
    case DOSYMB:
    case DOEXT:
    case DOCOL: {
      size_t child = 5;
      do {
        if (child > available - length) return BAD_SIZE;
        length += child;
        child = object_size(address + (uint32_t)length,
                            available - length, depth + 1);
        if (child == BAD_SIZE) return BAD_SIZE;
      } while (child != 0);
      if (available - length < 5) return BAD_SIZE;
      length += 5;
      break;
    }

    case SEMI:
      return 0;

    case DOIDNT:
    case DOLAM:
    case DOTAG: {
      if (available < 7) return BAD_SIZE;
      length = 7u + (size_t)(read_nibbles((long)address + 5, 2) & 0xffu) * 2u;
      if (length > available) return BAD_SIZE;
      size_t child = object_size(address + (uint32_t)length,
                                 available - length, depth + 1);
      if (child == BAD_SIZE) return BAD_SIZE;
      length += child;
      break;
    }

    case DORRP: {
      if (available < 13) return BAD_SIZE;
      uint32_t offset = read5(address + 8);
      if (offset == 0) {
        length = 13;
      } else {
        length = 8u + offset;
        if (length > available || available - length < 2) return BAD_SIZE;
        size_t name =
            (size_t)(read_nibbles((long)address + (long)length, 2) & 0xffu) *
                2u +
            4u;
        if (name > available - length) return BAD_SIZE;
        length += name;
        size_t child = object_size(address + (uint32_t)length,
                                   available - length, depth + 1);
        if (child == BAD_SIZE) return BAD_SIZE;
        length += child;
      }
      break;
    }

    case DOARRY:
    case DOLNKARRY:
    case DOCSTR:
    case DOHSTR:
    case DOGROB:
    case DOLIB:
    case DOBAK:
    case DOEXT0:
    case DOEXT2:
    case DOEXT3:
    case DOEXT4:
    case DOCODE:
      if (available < 10) return BAD_SIZE;
      length = 5u + read5(address + 5);
      break;

    /* On the 48G series this reserved object is an access pointer. */
    case DOEXT1:
      length = 15;
      break;

    default:
      length = 5;
      break;
  }
  return length <= available ? length : BAD_SIZE;
}

static bool rpl_pointers(uint32_t *temp_top, uint32_t *return_top,
                         uint32_t *data_top, uint32_t *avmem) {
  uint32_t a = read5(TEMPTOP);
  uint32_t b = read5(RSKTOP);
  uint32_t c = read5(DSKTOP);
  uint32_t d = read5(EDITLINE);
  uint32_t m = read5(AVMEM);
  if (a < 0x80000u || a > b || b > c || c > d || d >= SATURN_ADDRESS_SPACE)
    return false;
  if (temp_top) *temp_top = a;
  if (return_top) *return_top = b;
  if (data_top) *data_top = c;
  if (avmem) *avmem = m;
  return true;
}

unsigned hp48_rpl_stack_depth(void) {
  uint32_t c = 0;
  uint32_t d = 0;
  if (!rpl_pointers(NULL, NULL, &c, NULL)) return 0;
  d = read5(EDITLINE);
  if (d < c + 5) return 0;
  return (unsigned)((d - c) / 5u - 1u);
}

bool hp48_rpl_stack_level1_is_directory(void) {
  uint32_t data_top = 0;
  if (!rpl_pointers(NULL, NULL, &data_top, NULL) ||
      hp48_rpl_stack_depth() == 0)
    return false;
  uint32_t object = read5(data_top);
  return object < SATURN_ADDRESS_SPACE && read5(object) == DORRP;
}

bool hp48_rpl_collect_garbage(void) {
  if (!rpl_pointers(NULL, NULL, NULL, NULL)) return false;

  /* This is the same transaction Emu48 uses: execute the ROM entry with a
   * sentinel return address, retain its HP RAM writes, then restore the whole
   * Saturn chipset. Restore the scheduler counters too because these direct
   * instructions occur outside hp48_core_run(). */
  saturn_t saved_saturn = saturn;
  int saved_debugger = enter_debugger;
  int saved_device_check = device_check;
  long saved_schedule_event = schedule_event;
  unsigned long saved_instructions = instructions;

  saturn.P = 0;
  saturn.hexmode = HEX;
  saturn.PC = GARBAGECOL;
  push_return_addr(0xfffff);
  enter_debugger = 0;

  unsigned remaining = GC_INSTRUCTION_BUDGET;
  while (saturn.PC != 0xfffffu && remaining > 0 && !enter_debugger) {
    step_instruction();
    --remaining;
  }
  bool completed = saturn.PC == 0xfffffu && !enter_debugger;

  saturn = saved_saturn;
  enter_debugger = saved_debugger;
  device_check = saved_device_check;
  schedule_event = saved_schedule_event;
  instructions = saved_instructions;
  return completed && rpl_pointers(NULL, NULL, NULL, NULL);
}

hp48_rpl_status_t hp48_rpl_import_begin(hp48_rpl_import_t *transfer,
                                        size_t payload_bytes) {
  if (!transfer) return HP48_RPL_OUT_OF_RANGE;
  memset(transfer, 0, sizeof(*transfer));
  if (payload_bytes == 0 || payload_bytes > (RAM_SIZE_GX - 6u) / 2u)
    return HP48_RPL_OUT_OF_RANGE;

  uint32_t a, b, c, avmem;
  if (!rpl_pointers(&a, &b, &c, &avmem)) return HP48_RPL_NOT_READY;
  uint32_t payload_nibbles = (uint32_t)payload_bytes * 2u;
  uint32_t allocation = payload_nibbles + 6u;

  /* Keep five nibbles free for the new level-1 stack pointer. Ask the stock
   * ROM to compact abandoned temporary objects once before refusing. */
  if (allocation > c - b || c - b - allocation < 5u) {
    if (!hp48_rpl_collect_garbage()) return HP48_RPL_GC_FAILED;
    if (!rpl_pointers(&a, &b, &c, &avmem)) return HP48_RPL_GC_FAILED;
    if (allocation > c - b || c - b - allocation < 5u)
      return HP48_RPL_NO_MEMORY;
  }
  if (!span_ok(a, b - a) || !span_ok(a + allocation, b - a))
    return HP48_RPL_OUT_OF_RANGE;

  for (uint32_t offset = b - a; offset > 0; --offset) {
    int nibble = read_nibble((long)(a + offset - 1u));
    write_nibble((long)(a + allocation + offset - 1u), nibble);
  }
  write5(TEMPTOP, a + allocation);
  write5(RSKTOP, b + allocation);
  write5(AVMEM, (c - b - allocation) / 5u);
  write5(a + allocation - 5u, allocation);

  transfer->object_address = a + 1u;
  transfer->payload_bytes = (uint32_t)payload_bytes;
  transfer->temp_top_before = a;
  transfer->return_top_before = b;
  transfer->data_top_before = c;
  transfer->avmem_before = avmem;
  transfer->allocation_nibbles = allocation;
  transfer->active = true;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_import_write(hp48_rpl_import_t *transfer,
                                        const uint8_t *packed,
                                        size_t length) {
  if (!transfer || !transfer->active || (!packed && length != 0))
    return HP48_RPL_NOT_READY;
  if (length > transfer->payload_bytes - transfer->bytes_written)
    return HP48_RPL_OUT_OF_RANGE;

  uint32_t address = transfer->object_address + transfer->bytes_written * 2u;
  for (size_t i = 0; i < length; ++i) {
    write_nibble((long)address++, packed[i] & 0x0f);
    write_nibble((long)address++, packed[i] >> 4);
  }
  transfer->bytes_written += (uint32_t)length;
  return HP48_RPL_OK;
}

void hp48_rpl_import_abort(hp48_rpl_import_t *transfer) {
  if (!transfer || !transfer->active) return;
  uint32_t a = transfer->temp_top_before;
  uint32_t b = transfer->return_top_before;
  uint32_t shift = transfer->allocation_nibbles;
  for (uint32_t offset = 0; offset < b - a; ++offset) {
    int nibble = read_nibble((long)(a + shift + offset));
    write_nibble((long)(a + offset), nibble);
  }
  write5(TEMPTOP, a);
  write5(RSKTOP, b);
  write5(DSKTOP, transfer->data_top_before);
  write5(AVMEM, transfer->avmem_before);
  transfer->active = false;
}

hp48_rpl_status_t hp48_rpl_import_commit(hp48_rpl_import_t *transfer) {
  if (!transfer || !transfer->active) return HP48_RPL_NOT_READY;
  if (transfer->bytes_written != transfer->payload_bytes) {
    hp48_rpl_import_abort(transfer);
    return HP48_RPL_BAD_OBJECT;
  }

  size_t available = (size_t)transfer->payload_bytes * 2u;
  size_t actual = object_size(transfer->object_address, available, 0);
  /* Packed transfer files may contain one unused final high nibble. */
  if (actual == BAD_SIZE || actual == 0 ||
      !(actual == available || actual + 1u == available)) {
    hp48_rpl_import_abort(transfer);
    return HP48_RPL_BAD_OBJECT;
  }

  uint32_t avmem = read5(AVMEM);
  uint32_t data_top = read5(DSKTOP);
  if (avmem == 0 || data_top < transfer->return_top_before +
                                      transfer->allocation_nibbles + 5u) {
    hp48_rpl_import_abort(transfer);
    return HP48_RPL_NO_MEMORY;
  }
  write5(AVMEM, avmem - 1u);
  write5(DSKTOP, data_top - 5u);
  write5(data_top - 5u, transfer->object_address);
  transfer->active = false;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_export_begin(hp48_rpl_export_t *transfer) {
  if (!transfer) return HP48_RPL_OUT_OF_RANGE;
  memset(transfer, 0, sizeof(*transfer));
  uint32_t data_top = 0;
  if (!rpl_pointers(NULL, NULL, &data_top, NULL)) return HP48_RPL_NOT_READY;
  if (hp48_rpl_stack_depth() == 0) return HP48_RPL_EMPTY_STACK;

  uint32_t object = read5(data_top);
  if (object >= SATURN_ADDRESS_SPACE) return HP48_RPL_BAD_OBJECT;
  size_t nibbles = object_size(object, SATURN_ADDRESS_SPACE - object, 0);
  if (nibbles == BAD_SIZE || nibbles == 0 || nibbles > UINT32_MAX)
    return HP48_RPL_BAD_OBJECT;

  transfer->object_address = object;
  transfer->object_nibbles = (uint32_t)nibbles;
  transfer->packed_bytes = (uint32_t)((nibbles + 1u) / 2u);
  transfer->active = true;
  return HP48_RPL_OK;
}

hp48_rpl_status_t hp48_rpl_export_read(const hp48_rpl_export_t *transfer,
                                       size_t offset, uint8_t *packed,
                                       size_t length) {
  if (!transfer || !transfer->active || (!packed && length != 0))
    return HP48_RPL_NOT_READY;
  if (offset > transfer->packed_bytes ||
      length > transfer->packed_bytes - offset)
    return HP48_RPL_OUT_OF_RANGE;

  uint32_t address = transfer->object_address + (uint32_t)offset * 2u;
  for (size_t i = 0; i < length; ++i) {
    uint8_t low = (uint8_t)read_nibble((long)address++) & 0x0f;
    uint8_t high = 0;
    size_t nibble_index = (offset + i) * 2u + 1u;
    if (nibble_index < transfer->object_nibbles)
      high = ((uint8_t)read_nibble((long)address) & 0x0f) << 4;
    ++address;
    packed[i] = low | high;
  }
  return HP48_RPL_OK;
}

const char *hp48_rpl_status_label(hp48_rpl_status_t status) {
  switch (status) {
    case HP48_RPL_OK: return "OK";
    case HP48_RPL_NOT_READY: return "HP NOT READY";
    case HP48_RPL_BAD_OBJECT: return "BAD HP OBJECT";
    case HP48_RPL_NO_MEMORY: return "HP MEMORY FULL";
    case HP48_RPL_GC_FAILED: return "HP GC FAILED";
    case HP48_RPL_EMPTY_STACK: return "STACK EMPTY";
    case HP48_RPL_OUT_OF_RANGE: return "OBJECT TOO LARGE";
    default: return "OBJECT ERROR";
  }
}
