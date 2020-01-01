//
//  uart-bootloader.h
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-31.
//  Copyright © 2019 Samuel Dewan.
//

#ifndef uart_bootloader_h
#define uart_bootloader_h

#include <inttypes.h>


struct rn_bootloader_rsp_version;

/**
 *  Get version information from bootloader. The pointer provided by this
 *  function is malloced and must be freed by the caller.
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param version Pointer to where pointer to version information should be
 *                 stored
 *
 *  @return 0 if successfull
 */
extern int rn_bootloader_get_version_info (int fd,
                                    struct rn_bootloader_rsp_version **version);

/**
 *  Get the version number of the bootloader.
 *
 *  @param version Pointer to bootloaders version information
 *
 *  @return The bootloader's version number
 */
extern int rn_bootloader_get_version (
                                    struct rn_bootloader_rsp_version *version);

/**
 *  Get the device ID of the bootloader.
 *
 *  @param version Pointer to bootloaders version information
 *
 *  @return The bootloader's device ID
 */
extern int rn_bootloader_get_device_id (
                                    struct rn_bootloader_rsp_version *version);

/**
 *  Get the write latch size of the bootloader.
 *
 *  @param version Pointer to bootloaders version information
 *
 *  @return The bootloader's write latch size
 */
extern int rn_bootloader_get_write_size (
                                    struct rn_bootloader_rsp_version *version);


/**
 *  Erase a section of memory on the radio module.
 *
 *  @note The length should be a multiple of the bootloader's erase row size
 *        (usually 64)
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param start_address Begining of section to be erased
 *  @param length The length of the section to be erased
 *  @param version Pointer to bootloaders version information
 *
 *  @return 0 if successfull
 */
extern int rn_bootloader_erase (int fd, uint32_t start_address, uint16_t length,
                                struct rn_bootloader_rsp_version *version);

/**
 *  Write data to radio module.
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param address Address where data should be written
 *  @param length The number of bytes to be written
 *  @param data Pointer to the data to be written
 *  @param version Pointer to bootloaders version information
 *
 *  @return 0 if successfull
 */
extern int rn_bootloader_write (int fd, uint32_t address, uint16_t length,
                                uint8_t *data,
                                struct rn_bootloader_rsp_version *version);

/**
 *  Get checksum for data in radio module.
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param address Address of data to be checksummed
 *  @param length The number of bytes to be checksummed
 *  @param checksum Pointer to where checksum will be stored
 *
 *  @return 0 if successfull
 */
extern int rn_bootloader_checksum (int fd, uint32_t address, uint16_t length,
                                   uint16_t *checksum);

/**
 *  Calculate checksum.
 *
 *  @param data Data to be checksummed
 *  @param length The number of bytes to be checksummed
 *
 *  @return 16 bit sum of the data
 */
extern uint16_t rn_bootloader_calc_checksum(uint8_t *data, uint8_t length);

/**
 *  Calculate checksum for configuration row. This row must be handled specially
 *  because the data needs to be masked before it is summed.
 *
 *  @param data Pointer to the configuration row data
 *
 *  @return 16 bit sum of the masked data
 */
extern uint16_t rn_bootloader_calc_config_checksum(uint8_t *data);

/**
 *  Reset the module.
 *
 *  @param fd File descriptor for serial connection to radio
 *
 *  @return 0 if successfull
 */
extern int rn_bootloader_reset (int fd);

#endif /* uart_bootloader_h */
