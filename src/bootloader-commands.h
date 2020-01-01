//
//  bootloader-commands.h
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-31.
//  Copyright © 2019 Samuel Dewan.
//

#ifndef bootloader_commands_h
#define bootloader_commands_h

#include <inttypes.h>

/** Magic value for auto-baud */
#define RN_BOOTLOADER_MAGIC 0x55
/** First key value */
#define RN_BOOTLOADER_KEY_ONE   0x55
/** Second key value */
#define RN_BOOTLOADER_KEY_TWO   0xAA
/** Maximum value for length field of commands */
#define RN_BOOTLOADER_MAX_LENGTH    0xFF


enum rn_bootloader_command {
    RN_BOOTLOADER_CMD_GET_VERSION = 0x00,
    RN_BOOTLOADER_CMD_WRITE = 0x02,
    RN_BOOTLOADER_CMD_ERASE = 0x03,
    RN_BOOTLOADER_CMD_CHECKSUM = 0x08,
    RN_BOOTLOADER_CMD_RESET = 0x09
};

enum rn_bootloader_status {
    RN_BOOTLOADER_STATUS_FAILED = 0,
    RN_BOOTLOADER_STATUS_SUCCESS = 1
};

struct rn_bootloader_cmd_base {
    /* Magic value for autobaud (0x55) */
    uint8_t magic;
    /* Command */
    enum rn_bootloader_command command:8;
    /* Length of data (must be less than RN_BOOTLOADER_MAX_LENGTH) */
    uint16_t length;
    /* Key 1 (must be RN_BOOTLOADER_KEY_ONE) */
    uint8_t key_one;
    /* Key 2 (must be RN_BOOTLOADER_KEY_TWO) */
    uint8_t key_two;
    /* Address */
    uint32_t address;
} __attribute__ ((packed));

struct rn_bootloader_cmd_pkt {
    /* Magic value for autobaud (0x55) */
    uint8_t magic;
    /* Command */
    enum rn_bootloader_command command:8;
    /* Length of data (must be less than RN_BOOTLOADER_MAX_LENGTH) */
    uint16_t length;
    /* Key 1 (must be RN_BOOTLOADER_KEY_ONE) */
    uint8_t key_one;
    /* Key 2 (must be RN_BOOTLOADER_KEY_TWO) */
    uint8_t key_two;
    /* Address */
    uint32_t address;
    /* Data */
    char data[];
} __attribute__ ((packed));

struct rn_bootloader_rsp_version {
    /* Echoed command */
    struct rn_bootloader_cmd_base command;
    /* Bootloader version */
    uint16_t version;
    /* Maximum packet size (not used) */
    uint16_t max_packet_size;
    /* ACK packet size (not used) */
    uint16_t ack_packet_size;
    /* Device ID */
    uint16_t device_id;
    /* Not used */
    uint16_t RESERVED;
    /* Erase row size */
    uint8_t erase_row_size;
    /* Write latch size */
    uint8_t write_latch_size;
    /* User ID 1 */
    uint8_t user_id_1;
    /* User ID 2 */
    uint8_t user_id_2;
    /* User ID 3 */
    uint8_t user_id_3;
    /* User ID 4 */
    uint8_t user_id_4;
} __attribute__ ((packed));

/**
 *  Response for write and erase commands.
 */
struct rn_bootloader_rsp_status {
    /* Echoed command */
    struct rn_bootloader_cmd_base command;
    /* Command status */
    enum rn_bootloader_status status:8;
} __attribute__ ((packed));

struct rn_bootloader_rsp_checksum {
    /* Echoed command */
    struct rn_bootloader_cmd_base command;
    /* Checksum */
    uint16_t checksum;
} __attribute__ ((packed));

#endif /* bootloader_commands_h */
