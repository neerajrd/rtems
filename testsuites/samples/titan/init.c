#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems.h>
#include <tmacros.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <rtems/jffs2.h>
#include <rtems/libio.h>

const char rtems_test_name[] = "HELLO JFFS2";

/* -------- Flash Configuration -------- */
#define BLOCK_SIZE (32UL * 1024UL)
#define FLASH_SIZE (32UL * BLOCK_SIZE)

/* -------- Flash Control Structure -------- */
typedef struct {
  rtems_jffs2_flash_control super;
  unsigned char area[FLASH_SIZE];
} flash_control;

static flash_control flash_instance;

/* -------- Helper -------- */
static flash_control *get_flash_control(rtems_jffs2_flash_control *super)
{
  return (flash_control *) super;
}

/* -------- Flash Ops -------- */
static int flash_read(
  rtems_jffs2_flash_control *super,
  uint32_t offset,
  unsigned char *buffer,
  size_t size
)
{
  flash_control *self = get_flash_control(super);

  printf("[FLASH READ] offset=%u size=%zu\n", offset, size);

  if ((offset + size) > FLASH_SIZE)
    return -1;

  memcpy(buffer, &self->area[offset], size);
  return 0;
}

static int flash_write(
  rtems_jffs2_flash_control *super,
  uint32_t offset,
  const unsigned char *buffer,
  size_t size
)
{
  flash_control *self = get_flash_control(super);
  size_t i;

  printf("[FLASH WRITE] offset=%u size=%zu\n", offset, size);

  if ((offset + size) > FLASH_SIZE)
    return -1;

  for (i = 0; i < size; ++i) {
    self->area[offset + i] &= buffer[i];
  }

  return 0;
}

static int flash_erase(
  rtems_jffs2_flash_control *super,
  uint32_t offset
)
{
  flash_control *self = get_flash_control(super);

  printf("[FLASH ERASE] offset=%u size=%lu\n", offset, BLOCK_SIZE);

  if ((offset + BLOCK_SIZE) > FLASH_SIZE)
    return -1;

  memset(&self->area[offset], 0xFF, BLOCK_SIZE);
  return 0;
}

/* -------- Compressor -------- */
static rtems_jffs2_compressor_zlib_control compressor_instance = {
  .super = {
    .compress = rtems_jffs2_compressor_zlib_compress,
    .decompress = rtems_jffs2_compressor_zlib_decompress
  }
};

/* -------- Mount Data -------- */
static const rtems_jffs2_mount_data mount_data = {
  .flash_control = &flash_instance.super,
  .compressor_control = &compressor_instance.super
};

/* -------- Init Task -------- */
static rtems_task Init(rtems_task_argument ignored)
{
  int rv;
  int fd;
  const char *msg = "Hello from JFFS2!\n";
  char read_buf[64] = {0};

  (void) ignored;

  rtems_print_printer_fprintf_putc(&rtems_test_printer);
  TEST_BEGIN();

  /* Initialize flash */
  memset(flash_instance.area, 0xFF, FLASH_SIZE);

  /* Setup flash control */
  flash_instance.super.block_size = BLOCK_SIZE;
  flash_instance.super.flash_size = FLASH_SIZE;
  flash_instance.super.read = flash_read;
  flash_instance.super.write = flash_write;
  flash_instance.super.erase = flash_erase;
  flash_instance.super.device_identifier = 0xc01dc0fe;

  /* Mount filesystem */
  rv = mount_and_make_target_path(
    NULL,
    "/jffs2",
    RTEMS_FILESYSTEM_TYPE_JFFS2,
    RTEMS_FILESYSTEM_READ_WRITE,
    &mount_data
  );

  if (rv != 0) {
    printf("Mount failed: %s\n", strerror(errno));
    TEST_END();
    rtems_test_exit(1);
  }

  printf("Mounted JFFS2 at /jffs2\n");

  /* -------- File Create & Write -------- */
  fd = open("/jffs2/test.txt", O_CREAT | O_WRONLY, 0777);
  if (fd < 0) {
    printf("File open failed: %s\n", strerror(errno));
    TEST_END();
    rtems_test_exit(1);
  }

  write(fd, msg, strlen(msg));
  close(fd);
  printf("File written\n");

  /* -------- File Read -------- */
  fd = open("/jffs2/test.txt", O_RDONLY);
  if (fd < 0) {
    printf("File open (read) failed: %s\n", strerror(errno));
    TEST_END();
    rtems_test_exit(1);
  }

  read(fd, read_buf, sizeof(read_buf) - 1);
  close(fd);

  printf("Read from file: %s\n", read_buf);

  printf("Hello World\n");

  TEST_END();
  rtems_test_exit(0);
}

/* -------- RTEMS Config -------- */
#define CONFIGURE_APPLICATION_DOES_NOT_NEED_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER

#define CONFIGURE_FILESYSTEM_JFFS2
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20

#define CONFIGURE_MAXIMUM_TASKS 2
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT
#define CONFIGURE_INITIAL_EXTENSIONS RTEMS_TEST_INITIAL_EXTENSION

#define CONFIGURE_INIT
#include <rtems/confdefs.h>