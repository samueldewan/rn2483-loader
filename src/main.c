//
//  main.c
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-29.
//  Copyright © 2019 Samuel Dewan.
//

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <assert.h>

#include "intel-hex.h"
#include "rn2483.h"
#include "uart-bootloader.h"


static struct option longopts[] = {
    { "baud-rate", required_argument, NULL, 'b' },
    { "recover", no_argument, NULL, 'r' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

/**
 *  Get a the baudrate constant for a given integer baudrate.
 *
 *  @param baud The desired baudrate in baud
 *
 *  @return The corresponding speed value
 */
static int get_baud(int baud)
{
    switch (baud) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        default:
            return -1;
    }
}

/**
 *  Configure a serial interface.
 *
 *  @param fd File descriptor for serial interface
 *  @param baudrate Desired baudrate in baud
 *
 *  @return 0 if successfull
 */
static int configure_tty (int fd, int baudrate)
{
    struct termios term;
    
    if (tcgetattr(fd, &term) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }
    
    int speed = get_baud(baudrate);
    
    if (speed == -1) {
        printf("Unkown baud rate: %d\n", baudrate);
        return -1;
    }

    cfsetospeed(&term, (speed_t)speed);
    cfsetispeed(&term, (speed_t)speed);

    term.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    term.c_cflag &= ~CSIZE;
    term.c_cflag |= CS8;         /* 8-bit characters */
    term.c_cflag &= ~PARENB;     /* no parity bit */
    term.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    term.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL
                      | IXON);
    term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &term) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/**
 *  Print a progress bar.
 *
 *  @param percentage Percent completion to be displayed
 *  @param width The width of the progress bar
 */
static void print_progress (int percentage, int width)
{
    printf("\r%3d%% [", percentage);
    for (int i = 0; i < width; i++) {
        if (i <= ((percentage * width) / 100)) {
            printf("|");
        } else {
            printf(" ");
        }
    }
    printf("]");
}

/**
 *  Get module firmware version, ask user for confirmation and erase module.
 *
 *  @param fd File descriptor for module
 *  @param file Name of new firmware file
 *
 *  @return 0 if successfull
 */
static int enter_bootloader (int fd, char *file)
{
    char buffer[40];
    
    /* Check existing firmware version */
    int ret = rn2483_get_version(fd, buffer, 40, 1000);
    
    if (ret != 0) {
        return -1;
    }
    
    /* Ask for confirmation */
    printf("Current firwmare version: %s\nNew firmware: %s\n\n"
           "The firmware on the radio module will now be erased.\n", buffer,
           file);
    
    for (;;) {
        char *resp = readline("Are you sure that you would like to continue? "
                              "(y/n): ");
        
        if (resp == NULL) {
            return -1;
        } else if ((strcasecmp("y", resp) == 0) ||
                   (strcasecmp("yes", resp) == 0)) {
            free(resp);
            break;
        } else if ((strcasecmp("n", resp) == 0) ||
                   (strcasecmp("no", resp) == 0)) {
            free(resp);
            exit(0);
        } else {
            free(resp);
        }
    }
    
    /* Erase existing firmware */
    printf("\nErasing firmware...\n");
    ret = rn2483_erase(fd);
    
    if (ret != 0) {
        return -1;
    }
    
    return 0;
}

/**
 *  Erase flash, write firmware and verify checksums.
 *
 *  @param fd File desriptor for module
 *  @param hex Hex file structure to be written to module
 *
 *  @return 0 if successfull
 */
static int download_firmware (int fd, struct intel_hex_file *hex)
{
    /* Check bootloader version */
    struct rn_bootloader_rsp_version *version;
    
    int ret = rn_bootloader_get_version_info (fd, &version);
    
    if (ret != 0) {
        fprintf(stderr, "Could not get bootloader version.\n");
        goto free_version;
    }
    
    printf("\nBootloader version: 0x%04X\nDevice ID: 0x%04X\n\n",
           rn_bootloader_get_version(version),
           rn_bootloader_get_device_id(version));
    
    /* Erase flash */
    printf("Erasing flash...");
    
    ret = rn_bootloader_erase(fd, 0x300, 0x10000 - 0x300, version);
    
    if (ret != 0) {
        printf("\n");
        fprintf(stderr, "Failed to erase flash.\n");
        goto free_version;
    }
    
    printf(" done\n");
    
    /* Write flash */
    printf("Writing flash...\n");
    
    struct intel_hex_record *record = intel_hex_get_first_record(hex);
    int total_records = intel_hex_num_records(hex);
    int records_complete = 0;
    
    while (record != NULL) {
        print_progress((100 * records_complete) / total_records, 60);
        
        uint8_t *data;
        uint32_t address;
        uint8_t length;
        
        record = intel_hex_get_next_record(record, &data, &address, &length);
        
        ret = rn_bootloader_write(fd, address, length, data, version);
        
        if (ret != 0) {
            printf("\n");
            fprintf(stderr, "Failed to write record.\n");
            goto free_version;
        }
        
        records_complete++;
    }
    
    print_progress(100, 60);
    printf("\n");
    
    /* Get checksum */
    printf("Verifying...\n");
    
    record = intel_hex_get_first_record(hex);;
    records_complete = 0;
    while (record != NULL) {
        print_progress((100 * records_complete) / total_records, 60);
        
        uint8_t *data;
        uint32_t address;
        uint8_t length;
        
        record = intel_hex_get_next_record(record, &data, &address, &length);
        
        
        uint16_t checksum;
        ret = rn_bootloader_checksum(fd, address, length, &checksum);
        
        if (ret != 0) {
            fprintf(stderr, "Failed to check record.\n");
            goto free_version;
        }
        
        uint16_t calc_checksum = 0;
        if (address == 0x300000) {
            // Configuration row is handled specially because masks need to be
            // applied
            assert(length == 14);
            calc_checksum = rn_bootloader_calc_config_checksum(data);
        } else {
            calc_checksum = rn_bootloader_calc_checksum(data, length);
        }
        
        
        if (checksum != calc_checksum) {
            printf("\n");
            fprintf(stderr, "Checksum for address 0x%04X failed (got %04X, "
                            "calculated %04X).\n", address, checksum,
                    calc_checksum);
            goto free_version;
        }
        
        records_complete++;
    }
    
    print_progress(100, 60);
    printf("\n");
    
    /* Reset device */
    printf("Reseting device...");
    
    ret = rn_bootloader_reset(fd);
    
    if (ret != 0) {
        printf("\n");
        fprintf(stderr, "Failed to reset device.\n");
        goto free_version;
    }
    
    printf(" done\n");
    free(version);
    
    return 0;
free_version:
    free(version);
    return -1;
}

/**
 *  Wait for a given amount of time and print a message about waiting.
 *
 *  @param microseconds The amount of time to wait in microseconds
 */
static void wait_for_reset (useconds_t microseconds)
{
    printf("Waiting for module to reset...");
    fflush(stdout);
    usleep(microseconds);
    printf(" done\n");
}

int main(int argc, char * argv[])
{
    char *dev = NULL;
    int baudrate = 57600;
    char *file = NULL;
    
    int recover = 0;
    
    /* Parse arguments */
    int c;
    while (optind < argc) {
        c = getopt_long(argc, argv, "+hrb:", longopts, NULL);
        if (c != -1) {
            // Option
            switch (c) {
                case 'b':
                    ;
                    char *end;
                    baudrate = (int)strtol(optarg, &end, 10);
                    if (*end != '\0') {
                        fprintf(stderr, "Invalid baudrate \"%s\"\n", optarg);
                        return 1;
                    }
                    break;
                case 'r':
                    recover = 1;
                    break;
                case 'h':
                    printf("This is a tool for updateing the firware on "
                           "Microchip RN2483 radio modules.\nIt is used as "
                           "follows:\n\trn2483-loader [options] port "
                           "firmware_image\nThe -b option allows a baud rate to"
                           " be specified.\nThe -r option tries to complete the"
                           " update process on a module that is already in the "
                           "bootloader mode.\nUse the smaller firmware image "
                           "from the archive provided by Microchip, the one "
                           "in the 'offset' folder, not the 'combined' image."
                           "\n");
                    return 0;
                default:
                    return 1;
            }
        } else {
            // Positional argument
            if (dev == NULL) {
                dev = argv[optind];
            } else if (file == NULL) {
                file = argv[optind];
            } else {
                // Already have a device and file
                fprintf(stderr, "Unexpected positional argument \"%s\"\n",
                        argv[optind]);
                return 1;
            }
           
            optind++;
        }
    }
    
    /* Check arguments */
    if (dev == NULL) {
        fprintf(stderr, "No serial device specified\n");
        return 1;
    }
    if (file == NULL) {
        fprintf(stderr, "No hex file specified\n");
        return 1;
    }
    
    printf("Device: %s\n", dev);
    printf("Baudrate: %d\n\n", baudrate);
    
    /* Open and configure tty */
    int fd = open(dev, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd == -1) {
        fprintf(stderr, "Could not open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    
    if (!isatty(fd)) {
        fprintf(stderr, "File %s is not a tty.\n", dev);
        return 1;
    }
    
    int ret = configure_tty(fd, baudrate);
    if (ret == -1) {
        return 1;
    }
    
    /* Parse hex file */
    struct intel_hex_file *hex;
    ret = parse_intel_hex_file(file, &hex);
    
    if (ret != 0) {
        return -1;
    }
    
    /* Enter bootloader on module */
    if (!recover) {
        ret = enter_bootloader(fd, file);
        
        if (ret != 0) {
            return -1;
        }
    }
    
    /* Give the module some time to reset */
    wait_for_reset(500000);
    
    /* Download firmware */
    ret = download_firmware (fd, hex);
    
    if (ret != 0) {
        printf("Module may be stuck in bootloader. To try and complete the "
               "update process you can use this tool with the --recover option."
               "\nYou may need to power cycle the module.\n");
        return -1;
    }
    
    /* Give the module some time to reset */
    wait_for_reset(500000);
    
    /* Display new version */
    char buffer[40];
    ret = rn2483_get_version(fd, buffer, 40, 1000);
    
    if (ret != 0) {
        fprintf(stderr, "Could not get new firmware version.\n");
        return -1;
    }
    
    printf("\nUpdate completed successfully!\nFirmware version is now: %s\n",
           buffer);
    
    free_intel_hex_file(hex);
    
    return 0;
}
