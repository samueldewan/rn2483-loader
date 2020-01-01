//
//  uart-bootloader.c
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-31.
//  Copyright © 2019 Samuel Dewan.
//

#include "uart-bootloader.h"
#include "bootloader-commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alloca.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/select.h>


#define HOST_TO_LE_16(x) __builtin_bswap16(htons(x))
#define LE_TO_HOST_16(x) ntohs(__builtin_bswap16(x))

#define HOST_TO_LE_32(x) __builtin_bswap32(htonl(x))
#define LE_TO_HOST_32(x) ntohl(__builtin_bswap32(x))


/**
 *  Send command to bootloader and get response.
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param command Command to be sent
 *  @param length Length field for command, must not be greater than
 *                RN_BOOTLOADER_MAX_LENGTH
 *  @param key_one Value for key_one field of command
 *  @param key_two Value for key_two field of command
 *  @param address Value for address field of command
 *  @param data Pointer to data to be send in command, if NULL no data will be
 *              sent, if not NULL `length` bytes of data will be sent
 *  @param response Pointer to where response should be stored
 *  @param reponse_length Length of response
 *
 *  @return 0 if successfull
 */
static int rn_bootloader_do_command (int fd, enum rn_bootloader_command command,
                                     uint16_t length, uint8_t key_one,
                                     uint8_t key_two, uint32_t address,
                                     char *data, char *response,
                                     ssize_t response_length)
{
    size_t total_length = (sizeof(struct rn_bootloader_cmd_pkt) +
                           ((data == NULL) ? 0 : length));
    
    struct rn_bootloader_cmd_pkt *cmd = alloca(total_length);
    
    if (length > RN_BOOTLOADER_MAX_LENGTH) {
        fprintf(stderr, "Invalid length for bootloader command.\n");
        return -1;
    }
    
    /* Marshal command */
    cmd->magic = RN_BOOTLOADER_MAGIC;
    cmd->command = command;
    cmd->length = HOST_TO_LE_16(length);
    cmd->key_one = key_one;
    cmd->key_two = key_two;
    cmd->address = HOST_TO_LE_32(address);
    
    if ((data != NULL) && (length != 0)) {
        memcpy(cmd->data, data, length);
    }
    
    /* Send command */
    ssize_t len = 0;
    
    while (len < (ssize_t)total_length) {
        ssize_t nbytes = write(fd, ((char*)cmd) + len, total_length - len);
        
        if (nbytes == -1) {
            fprintf(stderr, "Could not write command to bootloader: %s.\n",
                    strerror(errno));
            return -1;
        }
        
        len += nbytes;
    }
    
    /* Get response */
    len = 0;
    
    while (len < response_length) {
        fd_set set;
        FD_SET(fd, &set);
        
        select(fd + 1, &set, NULL, NULL, NULL);
        
        if (FD_ISSET(fd, &set)) {
            // There is data available to be read from the file descriptor
            ssize_t nbytes = read(fd, response + len, response_length - len);
            len += nbytes;
            
            if (nbytes == -1) {
                fprintf(stderr, "Could not read from bootloader: %s.\n",
                        strerror(errno));
                return -1;
            }
        }
    }
    
    return 0;
}



int rn_bootloader_get_version_info (int fd,
                                    struct rn_bootloader_rsp_version **version)
{
    *version = malloc(sizeof(struct rn_bootloader_rsp_version));
    
    if (*version == NULL) {
        fprintf(stderr, "Could not allocate memory for bootloader version "
                        "information.\n");
        return -1;
    }
    
    int ret = rn_bootloader_do_command(fd, RN_BOOTLOADER_CMD_GET_VERSION, 0, 0,
                                       0, 0, NULL, (char*)*version,
                                    sizeof(struct rn_bootloader_rsp_version));
    
    if (ret != 0) {
        free(*version);
        return -1;
    }
    
    /* Make sure that endianness of response data is correct */
    (*version)->version = LE_TO_HOST_16((*version)->version);
    (*version)->max_packet_size = LE_TO_HOST_16((*version)->max_packet_size);
    (*version)->ack_packet_size = LE_TO_HOST_16((*version)->ack_packet_size);
    (*version)->device_id = LE_TO_HOST_16((*version)->device_id);
    
    return 0;
}

int rn_bootloader_get_version (struct rn_bootloader_rsp_version *version)
{
    return (int)version->version;
}

int rn_bootloader_get_device_id (struct rn_bootloader_rsp_version *version)
{
    return (int)version->device_id;
}

int rn_bootloader_get_write_size (struct rn_bootloader_rsp_version *version)
{
    return (int)version->write_latch_size;
}


int rn_bootloader_erase (int fd, uint32_t start_address, uint16_t length,
                         struct rn_bootloader_rsp_version *version)
{
    int total_blocks = length / version->erase_row_size;
    int remaining_blocks = total_blocks;
    
    while (remaining_blocks) {
        int blocks = (remaining_blocks > 256) ? 256 : remaining_blocks;
        
        uint32_t address = start_address + ((total_blocks - remaining_blocks) *
                                            version->erase_row_size);
        
        struct rn_bootloader_rsp_status response;
        
        int ret = rn_bootloader_do_command(fd, RN_BOOTLOADER_CMD_ERASE,
                                           (blocks == 256) ? 0 : blocks,
                                           RN_BOOTLOADER_KEY_ONE,
                                           RN_BOOTLOADER_KEY_TWO, address, NULL,
                                           (char*)&response, sizeof(response));
        
        if (ret != 0) {
            return -1;
        } else if (response.status != RN_BOOTLOADER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to erase blocks.\n");
            return -1;
        }
        
        remaining_blocks -= blocks;
    }
    
    return 0;
}


int rn_bootloader_write (int fd, uint32_t address, uint16_t length,
                         uint8_t *data,
                         struct rn_bootloader_rsp_version *version)
{
    uint16_t bytes_written = 0;
    
    while (bytes_written < length) {
        uint16_t nbytes = (length > version->write_latch_size) ?
                                            version->write_latch_size : length;
        
        struct rn_bootloader_rsp_status response;
        
        int ret = rn_bootloader_do_command(fd, RN_BOOTLOADER_CMD_WRITE,
                                           nbytes, RN_BOOTLOADER_KEY_ONE,
                                           RN_BOOTLOADER_KEY_TWO,
                                           address + bytes_written,
                                           (char*)data + bytes_written,
                                           (char*)&response, sizeof(response));
        
        if (ret != 0) {
            return -1;
        } else if (response.status != RN_BOOTLOADER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to write block.\n");
            return -1;
        }
        
        bytes_written += nbytes;
    }
    
    return 0;
}


int rn_bootloader_checksum (int fd, uint32_t address, uint16_t length,
                            uint16_t *checksum)
{
    struct rn_bootloader_rsp_checksum response;
    
    int ret = rn_bootloader_do_command(fd, RN_BOOTLOADER_CMD_CHECKSUM, length,
                                       0, 0, address, NULL, (char*)&response,
                                       sizeof(response));
    
    if (ret != 0) {
        return -1;
    }
    
    /* Make sure that endianness of response data is correct */
    *checksum = LE_TO_HOST_16(response.checksum);
    
    return 0;
}

uint16_t rn_bootloader_calc_checksum(uint8_t *data, uint8_t length)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < length; i += 2) {
        uint16_t n = 0;
        
        ((uint8_t*)&n)[0] = data[i];
        ((uint8_t*)&n)[1] = ((i + 1) < length) ? data[i + 1] : 0xff;
        
        sum += n;
    }
    
    return sum;
}

uint16_t rn_bootloader_calc_config_checksum(uint8_t *data)
{
    // Masks are from table 24-1 of PIC18LF46K22 datasheet.
    uint16_t sum = 0;
    uint16_t n = 0;
    
    // CONFIG1
    ((uint8_t*)&n)[0] = data[0];
    ((uint8_t*)&n)[1] = data[1];
    sum += n & 0xFF00;
    // CONFIG2
    ((uint8_t*)&n)[0] = data[2];
    ((uint8_t*)&n)[1] = data[3];
    sum += n & 0x3F1F;
    // CONFIG3
    ((uint8_t*)&n)[0] = data[4];
    ((uint8_t*)&n)[1] = data[5];
    sum += n & 0xBF00;
    // CONFIG4
    ((uint8_t*)&n)[0] = data[6];
    ((uint8_t*)&n)[1] = data[7];
    sum += n & 0x00C5;
    // CONFIG5
    ((uint8_t*)&n)[0] = data[8];
    ((uint8_t*)&n)[1] = data[9];
    sum += n & 0xC00F;
    // CONFIG6
    ((uint8_t*)&n)[0] = data[10];
    ((uint8_t*)&n)[1] = data[11];
    sum += n & 0xE00F;
    // CONFIG7
    ((uint8_t*)&n)[0] = data[12];
    ((uint8_t*)&n)[1] = data[13];
    sum += n & 0x400F;
    
    return sum;
}


int rn_bootloader_reset (int fd)
{
    int ret = rn_bootloader_do_command(fd, RN_BOOTLOADER_CMD_RESET, 0,
                                       0, 0, 0, NULL, NULL, 0);
    
    if (ret != 0) {
        return -1;
    }
    
    return 0;
}
