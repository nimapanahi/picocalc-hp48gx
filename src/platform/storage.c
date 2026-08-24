#include "storage.h"

#include <stddef.h>
#include <stdint.h>

#include "ff.h"
#include "hardware/spi.h"
#include "hw_config.h"
#include "sd_card.h"
#include "version.h"

#define PICOCALC_SD_MISO_GPIO 16u
#define PICOCALC_SD_CS_GPIO 17u
#define PICOCALC_SD_SCK_GPIO 18u
#define PICOCALC_SD_MOSI_GPIO 19u
#define PICOCALC_SD_DETECT_GPIO 22u
#define PICOCALC_SD_BAUD_RATE 12500000u

#define HP48_SD_ROOT "0:/HP48GX"
#define HP48_PROGRAMS_DIR HP48_SD_ROOT "/PROGRAMS"
#define HP48_PROGRAMS_README HP48_PROGRAMS_DIR "/README.TXT"
#define HP48_PORT2_README HP48_PROGRAMS_DIR "/PORT2.TXT"
#define HP48_FILES_README HP48_PROGRAMS_DIR "/FILES.TXT"
#define HP48_INBOX_DIR HP48_PROGRAMS_DIR "/INBOX"
#define HP48_OUTBOX_DIR HP48_PROGRAMS_DIR "/OUTBOX"

static spi_t s_sd_spi = {
    .hw_inst = spi0,
    .miso_gpio = PICOCALC_SD_MISO_GPIO,
    .mosi_gpio = PICOCALC_SD_MOSI_GPIO,
    .sck_gpio = PICOCALC_SD_SCK_GPIO,
    .baud_rate = PICOCALC_SD_BAUD_RATE,
    .spi_mode = 0,
};

static sd_spi_if_t s_sd_spi_if = {
    .spi = &s_sd_spi,
    .ss_gpio = PICOCALC_SD_CS_GPIO,
};

static sd_card_t s_sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &s_sd_spi_if,
    .use_card_detect = true,
    .card_detect_gpio = PICOCALC_SD_DETECT_GPIO,
    .card_detected_true = 0,
    .card_detect_use_pull = true,
    .card_detect_pull_hi = true,
};

static storage_status_t s_status = STORAGE_NO_CARD;

size_t sd_get_num(void) { return 1; }

sd_card_t *sd_get_by_num(size_t num) {
  return num == 0 ? &s_sd_card : NULL;
}

static bool make_directory(const char *path) {
  FRESULT result = f_mkdir(path);
  return result == FR_OK || result == FR_EXIST;
}

static bool create_programs_readme(void) {
  static const char contents[] =
      "HP 48GX PicoCalc program folder\r\n"
      "\r\n"
      "Version " HP48GX_VERSION " supports a writable card and files.\r\n"
      "Read PORT2.TXT and FILES.TXT for the two storage workflows.\r\n"
      "Existing user files are not modified or deleted.\r\n";
  FIL file;
  FRESULT result = f_open(&file, HP48_PROGRAMS_README,
                          FA_WRITE | FA_CREATE_NEW);
  if (result == FR_EXIST) return true;
  if (result != FR_OK) return false;

  UINT written = 0;
  result = f_write(&file, contents, sizeof(contents) - 1, &written);
  FRESULT close_result = f_close(&file);
  return result == FR_OK && close_result == FR_OK &&
         written == sizeof(contents) - 1;
}

static bool create_files_readme(void) {
  static const char contents[] =
      "HP 48GX individual files - version " HP48GX_VERSION "\r\n"
      "\r\n"
      "Copy standard HP 48 binary files with an HPHP48-x header into INBOX.\r\n"
      "The final header byte may vary; extract ZIP archives first.\r\n"
      "Ctrl+F7 selects; Ctrl+F8 imports at HP idle onto stack level 1.\r\n"
      "Ctrl+F9 saves stack level 1 as OUTBOX/STKnnn.48G.\r\n"
      "Exports never overwrite an existing numbered file.\r\n"
      "INBOX files remain unchanged after import.\r\n"
      "Power the PicoCalc off before removing the SD card.\r\n";
  FIL file;
  FRESULT result = f_open(&file, HP48_FILES_README,
                          FA_WRITE | FA_CREATE_NEW);
  if (result == FR_EXIST) return true;
  if (result != FR_OK) return false;
  UINT written = 0;
  result = f_write(&file, contents, sizeof(contents) - 1, &written);
  FRESULT close_result = f_close(&file);
  return result == FR_OK && close_result == FR_OK &&
         written == sizeof(contents) - 1;
}

static bool create_port2_readme(void) {
  static const char contents[] =
      "HP 48GX PicoCalc Port 2 card - version " HP48GX_VERSION "\r\n"
      "\r\n"
      "PORT2.CRD is a standard packed 128 KiB x48-compatible card image.\r\n"
      "The emulator imports it at boot and exports changes on Ctrl+F10 or OFF.\r\n"
      "PORT2.BAK is the previous committed image; PORT2.NEW is temporary.\r\n"
      "For old machine-code libraries, create PORT1.MODE before boot.\r\n"
      "That selects separate PORT1.CRD/NEW/BAK files; store libraries to 1.\r\n"
      "Remove PORT1.MODE while powered off to return to the Port 2 image.\r\n"
      "Power the PicoCalc off before removing the SD card or replacing files.\r\n"
      "Initialize a blank card: left-shift, 2, NXT, PINIT.\r\n"
      "Right-shift, 2 is the attached-library catalog; blank is normal.\r\n"
      "PINIT is silent when the card is already empty and initialized.\r\n"
      "Use :1: NAME in Port 1 mode, or :2: NAME otherwise; then STO.\r\n";
  FIL file;
  FRESULT result = f_open(&file, HP48_PORT2_README,
                          FA_WRITE | FA_CREATE_NEW);
  if (result == FR_EXIST) return true;
  if (result != FR_OK) return false;

  UINT written = 0;
  result = f_write(&file, contents, sizeof(contents) - 1, &written);
  FRESULT close_result = f_close(&file);
  return result == FR_OK && close_result == FR_OK &&
         written == sizeof(contents) - 1;
}

storage_status_t storage_init(void) {
  s_status = STORAGE_NO_CARD;
  if (!sd_init_driver()) {
    s_status = STORAGE_MOUNT_FAILED;
    return s_status;
  }
  if (!sd_card_detect(&s_sd_card)) return s_status;

  FRESULT result = f_mount(&s_sd_card.state.fatfs, "0:", 1);
  if (result != FR_OK) {
    s_status = STORAGE_MOUNT_FAILED;
    return s_status;
  }
  s_sd_card.state.mounted = true;

  if (!make_directory(HP48_SD_ROOT) ||
      !make_directory(HP48_PROGRAMS_DIR) ||
      !make_directory(HP48_INBOX_DIR) ||
      !make_directory(HP48_OUTBOX_DIR) ||
      !create_programs_readme() || !create_port2_readme() ||
      !create_files_readme()) {
    s_status = STORAGE_FOLDER_FAILED;
    return s_status;
  }

  s_status = STORAGE_READY;
  return s_status;
}

void storage_shutdown(void) {
  if (!s_sd_card.state.mounted) return;
  f_mount(NULL, "0:", 0);
  s_sd_card.state.mounted = false;
}

bool storage_ready(void) { return s_status == STORAGE_READY; }
storage_status_t storage_status(void) { return s_status; }

const char *storage_status_label(void) {
  switch (s_status) {
    case STORAGE_READY: return "SD READY";
    case STORAGE_NO_CARD: return "NO SD";
    case STORAGE_MOUNT_FAILED: return "SD MOUNT ERR";
    case STORAGE_FOLDER_FAILED: return "SD FOLDER ERR";
    default: return "SD ERR";
  }
}
