#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

int mounted = 0;    // Default

int mdadm_mount(void) {
  if (mounted == 1){
    return -1;    // Failure, If already mounted
  }
  uint32_t op_mount = (JBOD_MOUNT << 12);   // Left-shift 12 bits of position
  int result = jbod_operation(op_mount, NULL);
  if (result == 0){
    mounted = 1;    // Success, Set status 'mounted' to 1
    return 1;
  }
  return -1;    // Failure, Otherwise
}

int mdadm_unmount(void) {
  if (mounted == 0){
    return -1;    // Failure, If already unmounted
  }
  uint32_t op_unmount = (JBOD_UNMOUNT << 12);   // Left-shift 12 bits of position
  int result = jbod_operation(op_unmount, NULL);
  if (result == 0){
    mounted = 0;    // Success, Set status 'mounted' to 0
    return 1;
  }
  return -1;    // Failure, Otherwise
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  // Failure Check

  // Failure -1, Index Out Of Bounds
  int max_size = JBOD_DISK_SIZE * JBOD_NUM_DISKS;
  if (start_addr + read_len > max_size){
    return -1;
  }
  // Failure -2, Length Exceeds 1024
  if (read_len > 1024){
    return -2;
  }
  // Failure -3, Unmounted
  if (mounted == 0){
    return -3;
  }
  // Failure -4, Potential Unspecified Error
  /*
  uint32_t op_mount = (JBOD_MOUNT << 12);
  uint32_t op_unmount = (JBOD_UNMOUNT << 12);
  if (jbod_operation(op_mount, NULL) != 0 || jbod_operation(op_unmount, NULL) != 0){    // JBOD Operation Error
    return -4;
  }*/

  if (read_buf == NULL){    // Invalid Buffer
    if (read_len > 0){
      return -4;
    }
    return 0;
  }

  // End of Failure Check

  uint32_t current_addr = start_addr;   // Current Address
  uint32_t bytes_read = 0;    // Bytes already read
  uint32_t bytes_left = read_len - bytes_read;   // Bytes left to read
  uint32_t offset = 0;    // Offset for read_buf

  while (bytes_read < read_len){
    // Determine the current position
    uint32_t disk = current_addr / JBOD_DISK_SIZE;
    uint32_t block = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    offset = (current_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;

    // Seek for Correct Disk
    uint32_t op_seek_for_disk = (JBOD_SEEK_TO_DISK << 12) | disk;    // Left-shift 12 bits of position
    int result_sfd = jbod_operation(op_seek_for_disk, NULL);
    if (result_sfd == -1){
      return -1;    // Fail to Seek
    }

    // Seek for Correct Block
    uint32_t op_seek_for_block = (JBOD_SEEK_TO_BLOCK << 12) | (block & 0xFF) << 4;    // Left-shift 12 bits of position, limit block id to less than 255 (8 bits)
    int result_sfb = jbod_operation(op_seek_for_block, NULL);
    if (result_sfb == -1){
      return -1;    // Fail to Seek
    }

    // Read the Block
    uint8_t block_buffer[JBOD_BLOCK_SIZE];    // Temp keeping the data
    uint32_t op_read_block = (JBOD_READ_BLOCK << 12);   // Left-shift 12 bits of position
    int result_rb = jbod_operation(op_read_block, block_buffer);
    if (result_rb == -1){
      return -1;    // Fail to Read
    }

    // Copy Bytes From the Block
    uint32_t bytes_need = JBOD_BLOCK_SIZE - offset;   // Get the bytes need to be copy
    if (bytes_need > bytes_left){   // If exceeds, replace with bytes_left to avoid out of bounds
      bytes_need = bytes_left;
    }

    memcpy(read_buf + bytes_read, block_buffer + offset, bytes_need);   // Copy bytes from block to read_buf

    // Updates
    bytes_read = bytes_read + bytes_need;   // Update the bytes already read
    bytes_left = read_len - bytes_read;    // Update the bytes left to read
    current_addr += bytes_need;   // Update the current address to the position of read bytes
  }

  return bytes_read;   // Return the final result of read bytes
}

