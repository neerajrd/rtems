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

/* Block Configuration */
#define BLOCK_SIZE (32UL * 1024UL)
#define STORAGE_SIZE (32UL * BLOCK_SIZE)

/* Block Control Structure */
typedef struct {
  rtems_jffs2_flash_control super;
  unsigned char area[STORAGE_SIZE];
} block_control;

static block_control block_instance;

/* Block Ops */
static int block_read(
  rtems_jffs2_flash_control *super,
  uint32_t offset,
  unsigned char *buffer,
  size_t size
)
{
  block_control *self = (block_control *) super;

  // printf("[BLOCK READ] offset=%u size=%zu\n", offset, size);

  if ((offset + size) > STORAGE_SIZE)
    return -1;

  memcpy(buffer, &self->area[offset], size);
  return 0;
}

static int block_write(
  rtems_jffs2_flash_control *super,
  uint32_t offset,
  const unsigned char *buffer,
  size_t size
)
{
  block_control *self = (block_control *) super;
  size_t i;

  // printf("[BLOCK WRITE] offset=%u size=%zu\n", offset, size);

  if ((offset + size) > STORAGE_SIZE)
    return -1;

  for (i = 0; i < size; ++i) {
    self->area[offset + i] &= buffer[i];
  }

  return 0;
}

static int block_erase(
  rtems_jffs2_flash_control *super,
  uint32_t offset
)
{
  block_control *self = (block_control *) super;

  // printf("[BLOCK ERASE] offset=%u size=%lu\n", offset, BLOCK_SIZE);

  if ((offset + BLOCK_SIZE) > STORAGE_SIZE)
    return -1;

  memset(&self->area[offset], 0xFF, BLOCK_SIZE);
  return 0;
}

/* Mount Data */
static const rtems_jffs2_mount_data mount_data = {
  .flash_control = &block_instance.super,
  .compressor_control = NULL
};

/* Init Task */
static rtems_task Init(rtems_task_argument ignored)
{
  int rv;
  int fd;
  const char *msg = "Hello from JFFS2!\n";
  char read_buf[64] = {0};

  (void) ignored;

  rtems_print_printer_fprintf_putc(&rtems_test_printer);
  TEST_BEGIN();

  /* Initialize block storage */
  memset(block_instance.area, 0xFF, STORAGE_SIZE);

  /* Setup block control */
  block_instance.super.block_size = BLOCK_SIZE;
  block_instance.super.flash_size = STORAGE_SIZE;
  block_instance.super.read = block_read;
  block_instance.super.write = block_write;
  block_instance.super.erase = block_erase;
  block_instance.super.device_identifier = 0xc01dc0fe;

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

  /* File Write */
  fd = open("/jffs2/test.txt", O_CREAT | O_WRONLY, 0777);
  if (fd < 0) {
    printf("File open failed: %s\n", strerror(errno));
    TEST_END();
    rtems_test_exit(1);
  }

  write(fd, msg, strlen(msg));
  close(fd);
  printf("File written\n");

  /* File Read */
  fd = open("/jffs2/test.txt", O_RDONLY);
  if (fd < 0) {
    printf("File open (read) failed: %s\n", strerror(errno));
    TEST_END();
    rtems_test_exit(1);
  }

  read(fd, read_buf, sizeof(read_buf) - 1);
  close(fd);

  printf("Read from file: %s\n", read_buf);

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