#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "lfs.h"

#define QSPI_SECTOR_SIZE 4096
#define QSPI_PAGE_SIZE   256
#define QSPI_SECTORS     (32 * 1024 / QSPI_SECTOR_SIZE)

/* Emulated flash */
static uint8_t g_flash[QSPI_SECTOR_SIZE * QSPI_SECTORS];

/* LittleFS block device functions */
static int disk_read(const struct lfs_config *c, lfs_block_t block,
                     lfs_off_t off, void *buffer, lfs_size_t size)
{
  memcpy(buffer, &g_flash[block * c->block_size + off], size);
  return 0;
}

static int disk_prog(const struct lfs_config *c, lfs_block_t block,
                     lfs_off_t off, const void *buffer, lfs_size_t size)
{
  memcpy(&g_flash[block * c->block_size + off], buffer, size);
  return 0;
}

static int disk_erase(const struct lfs_config *c, lfs_block_t block)
{
  memset(&g_flash[block * c->block_size], 0xFF, c->block_size);
  return 0;
}

static int disk_sync(const struct lfs_config *c)
{
  (void)c;
  return 0;
}

/* LittleFS config */
static struct lfs_config cfg = {
  .read  = disk_read,
  .prog  = disk_prog,
  .erase = disk_erase,
  .sync  = disk_sync,

  .read_size = QSPI_PAGE_SIZE,
  .prog_size = QSPI_PAGE_SIZE,
  .block_size = QSPI_SECTOR_SIZE,
  .block_count = QSPI_SECTORS,
  .cache_size = QSPI_PAGE_SIZE,
  .lookahead_size = 16,
  .block_cycles = 500,
};

static rtems_task Init(rtems_task_argument ignored)
{
  (void) ignored;

  lfs_t lfs;

  printf("\n=== LittleFS RTEMS TEST Start ===\n");

  printf("Initializing emulated flash (size: %lu bytes)...\n", (unsigned long)sizeof(g_flash));
  memset(g_flash, 0xFF, sizeof(g_flash));

  printf("Formatting LittleFS...\n");
  int res = lfs_format(&lfs, &cfg);
  if (res < 0) {
    printf("ERROR: Format failed with code %d\n", res);
    rtems_shutdown_executive(1);
  }
  printf("SUCCESS: Filesystem formatted\n");

  printf("Mounting filesystem...\n");
  res = lfs_mount(&lfs, &cfg);
  if (res < 0) {
    printf("ERROR: Mount failed with code %d\n", res);
    rtems_shutdown_executive(1);
  }
  printf("SUCCESS: Filesystem mounted\n");

  printf("Writing data to file 'test.txt'...\n");
  {
    lfs_file_t file;
    const char *filename = "test.txt";
    const char *data = "Hello LittleFS from RTEMS!\n";

    res = lfs_file_open(&lfs, &file, filename, LFS_O_RDWR | LFS_O_CREAT);
    if (res < 0) {
      printf("ERROR: Failed to open file for writing (%d)\n", res);
      rtems_shutdown_executive(1);
    }

    lfs_file_write(&lfs, &file, data, strlen(data));
    lfs_file_close(&lfs, &file);

    printf("SUCCESS: Written data to '%s'\n", filename);
  }

  printf("Reading data back from file...\n");
  {
    lfs_file_t file;
    char buffer[128] = {0};

    res = lfs_file_open(&lfs, &file, "test.txt", LFS_O_RDONLY);
    if (res < 0) {
      printf("ERROR: Failed to open file for reading (%d)\n", res);
      rtems_shutdown_executive(1);
    }

    lfs_file_read(&lfs, &file, buffer, sizeof(buffer) - 1);
    lfs_file_close(&lfs, &file);

    printf("SUCCESS: Read content:\n%s\n", buffer);
  }

  printf("Listing files in root directory...\n");
  {
    lfs_dir_t dir;
    struct lfs_info info;

    res = lfs_dir_open(&lfs, &dir, "/");
    if (res < 0) {
      printf("ERROR: Failed to open directory (%d)\n", res);
      rtems_shutdown_executive(1);
    }

    int count = 0;
    while (lfs_dir_read(&lfs, &dir, &info) > 0) {
      if (info.type == LFS_TYPE_REG) {
        printf("  File[%d]: %s (Size: %d bytes)\n", count, info.name, info.size);
        count++;
      }
    }

    lfs_dir_close(&lfs, &dir);
    printf("SUCCESS: Total files found: %d\n", count);
  }

  printf("Unmounting filesystem...\n");
  lfs_unmount(&lfs);
  printf("SUCCESS: Filesystem unmounted\n");

  printf("=== LittleFS RTEMS TEST Completed Successfully ===\n");
  printf("Shutting down RTEMS...\n");

  rtems_shutdown_executive(0);
}

/* Drivers */
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER

/* Resources */
#define CONFIGURE_MAXIMUM_TASKS 5
#define CONFIGURE_MAXIMUM_SEMAPHORES 5

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT

#include <rtems/confdefs.h>
