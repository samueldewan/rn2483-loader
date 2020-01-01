//
//  intel-hex.c
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-30.
//  Copyright © 2019 Samuel Dewan.
//

#include "intel-hex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define INTEL_HEX_RECORD_TYPE_MAX   0x05
enum intel_hex_record_type {
    INTEL_HEX_RECORD_DATA = 0x00,
    INTEL_HEX_RECORD_EOF = 0x01,
    INTEL_HEX_RECORD_EXT_SEG_ADDR = 0x02,
    INTEL_HEX_RECORD_START_SEG_ADDR = 0x03,
    INTEL_HEX_RECORD_EXT_LIN_ADDR = 0x04,
    INTEL_HEX_RECORD_START_LIN_ADDR = 0x05
};

struct intel_hex_record {
    struct intel_hex_record *next;
    uint8_t *data;
    
    uint32_t address;
    uint8_t length;
};

struct intel_hex_file {
    struct intel_hex_record *head;
    
    int num_records;
    
    union {
        uint32_t start_segment_addr;
        uint32_t start_linear_addr;
    };
    
    uint16_t ext_linear_addr;
    uint16_t ext_segment_addr;
    
    uint8_t has_eof;
};

/**
 *  Parse a nibble from a single hexidecimal digit.
 *
 *  @param c Character containing the digit to be parsed
 *  @param dest Pointer to where data should be stored
 *  @param offset Number of bits which data should be offset within byte
 *
 *  @return 0 If successfull
 */
static int parse_nibble (char c, uint8_t *dest, int offset)
{
    if (c < '0') {
        return -1;
    } else if (c >= 'a') {
        c -= 32;
    }
    c -= '0';
    if (c <= 9) {
        goto store_nibble;
    }
    c -= 7;
    if (c <= 9) {
        return -1;
    } else if (c <= 15) {
        goto store_nibble;
    }
    return -1;
store_nibble:
    *dest |= (c << offset);
    return 0;
}

/**
 *  Parse a single record of an intel hex file.
 *
 *  @param line String containing the record to be parsed
 *  @param record Pointer to memory where pointer to parsed record structure
 *                should be placed
 *  @param file Structure representing file to which this record belongs
 *
 *  @return 0 if successfull
 */
static int parse_record (const char *line, struct intel_hex_record **record,
                         struct intel_hex_file *file)
{
    size_t line_length = strlen(line);
    
    uint8_t length;
    uint16_t address;
    enum intel_hex_record_type type;
    uint8_t checksum;
    uint8_t *data = NULL;
    
    /* Sanity check */
    if (line[0] != ':') {
        fprintf(stderr, "Invalid record \"%s\" (does not start with ':').\n",
                line);
        return -1;
    } else if (line_length < 11) {
        fprintf(stderr, "Invalid record \"%s\" (not long enough).\n",
                line);
        return -1;
    }
    
    char *end;
    char tmp[(sizeof(uintmax_t) * 2) + 1];
    tmp[4] = '\0';
    
    /* Parse and verify length */
    tmp[0] = line[1];
    tmp[1] = line[2];
    tmp[2] = '\0';
    length = (uint8_t)strtol(tmp, &end, 16);
    if (*end != '\0') {
        fprintf(stderr, "Invalid record \"%s\" (length not a valid number).\n",
                line);
        return -1;
    } else if (line_length != (size_t)((length * 2) + 11)) {
        fprintf(stderr, "Invalid record \"%s\" (length incorrect).\n",
                line);
        return -1;
    }
    
    /* Parse address */
    tmp[0] = line[3];
    tmp[1] = line[4];
    tmp[2] = line[5];
    tmp[3] = line[6];
    address = (uint16_t)strtol(tmp, &end, 16);
    if (*end != '\0') {
        fprintf(stderr, "Invalid record \"%s\" (address not a valid number).\n",
                line);
        return -1;
    }
    
    /* Parse and verify record type */
    tmp[0] = line[7];
    tmp[1] = line[8];
    tmp[2] = '\0';
    type = (uint8_t)strtol(tmp, &end, 16);
    if (*end != '\0') {
        fprintf(stderr, "Invalid record \"%s\" (type not a valid number).\n",
                line);
        return -1;
    } else if (type > INTEL_HEX_RECORD_TYPE_MAX) {
        fprintf(stderr, "Invalid record \"%s\" (invalid type).\n",
                line);
        return -1;
    }
    
    /* Parse checksum */
    tmp[0] = line[line_length - 2];
    tmp[1] = line[line_length - 1];
    tmp[2] = '\0';
    checksum = (uint8_t)strtol(tmp, &end, 16);
    if (*end != '\0') {
        fprintf(stderr, "Invalid record \"%s\" (checksum is not a valid number).\n",
                line);
        return -1;
    }
    
    /* Parse data */
    if (length > 0) {
        data = calloc(length, sizeof(uint8_t));
        if (data == NULL) {
            fprintf(stderr, "Could not allocate memory for record data.\n");
            return -1;
        }
        
        for (uint8_t i = 0; i < length; i++) {
            int ret = parse_nibble(line[9 + (2 * i)], data + i, 4);
            ret |= parse_nibble(line[9 + (2 * i) + 1], data + i, 0);
            if (ret != 0) {
                fprintf(stderr, "Invalid data in record \"%s\".\n", line);
                return -1;
            }
        }
    }
    
    /* Verify checksum */
    uint8_t sum = (length + type + (uint8_t)(address & 0xFF) +
                   (uint8_t)(address >> 8) + checksum);
    
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    
    if (sum != 0) {
        fprintf(stderr, "Invalid record \"%s\" (checksum is not correct).\n",
                line);
        goto free_data;
    }
    
    /* Handle record */
    switch (type) {
        case INTEL_HEX_RECORD_DATA:
            *record = malloc(sizeof(struct intel_hex_record));
            if (record == NULL) {
                fprintf(stderr, "Could not allocate memory for record.\n");
                goto free_data;
            }
            
            (*record)->next = NULL;
            (*record)->data = data;
            (*record)->address = ((address + (file->ext_segment_addr * 16)) |
                                  (file->ext_linear_addr << 16));
            (*record)->length = length;
            
            file->num_records++;
            break;
        case INTEL_HEX_RECORD_EOF:
            file->has_eof = 1;
            break;
        case INTEL_HEX_RECORD_EXT_SEG_ADDR:
            file->ext_segment_addr = (data[0] << 8) | data[1];
            free(data);
            break;
        case INTEL_HEX_RECORD_START_SEG_ADDR:
            file->start_segment_addr = ((data[0] << 24) | (data[1] << 16) |
                                        (data[2] << 8) | data[3]);
            free(data);
            break;
        case INTEL_HEX_RECORD_EXT_LIN_ADDR:
            file->ext_linear_addr = (data[0] << 8) | data[1];
            free(data);
            break;
        case INTEL_HEX_RECORD_START_LIN_ADDR:
            file->start_linear_addr = ((data[0] << 24) | (data[1] << 16) |
                                       (data[2] << 8) | data[3]);
            free(data);
            break;
    }
    
    return 0;

free_data:
    free(data);
    return -1;
}

/**
 *  Free a record and its associated data.
 *
 *  @param record The record to be freed
 */
static void free_record (struct intel_hex_record *record)
{
    free(record->data);
    free(record);
}

int parse_intel_hex_file (const char *name, struct intel_hex_file **file)
{
    /* Open file */
    FILE *f = fopen(name, "r");
    
    if (f == NULL) {
        fprintf(stderr, "Could not open file %s: %s.\n", name, strerror(errno));
        return -1;
    }
    
    /* Allocate and initialize a intel_hex_file struct */
    *file = calloc(1, sizeof(struct intel_hex_file));
    
    if (*file == NULL) {
        fprintf(stderr, "Could not alocate memory to parse hex file.\n");
        goto close_file;
    }
    
    /* Parse records */
    char line[256];
    struct intel_hex_record **record = &((*file)->head);
    while (fgets(line, 256, f)) {
        // Trim line delimiters from line
        if ((line[strlen(line) - 2] == '\n') ||
            (line[strlen(line) - 2] == '\r')) {
            line[strlen(line) - 2] = '\0';
        } else if ((line[strlen(line) - 1] == '\n') ||
                   (line[strlen(line) - 1] == '\r')) {
            line[strlen(line) - 1] = '\0';
        }
        // Check for empty line
        if (*line == '\0') {
            continue;
        }
        // Check if we are about to parse a record that it past the EOF record
        if ((*file)->has_eof) {
            fprintf(stderr, "Hit EOF record in hex file before end of file.\n");
            goto free_records;
        }
        // Parse line
        int ret = parse_record(line, record, *file);
        if (ret != 0) {
            goto free_records;
        }
        // If a new record was stored, updated our pointer to the next record
        if (*record != NULL) {
            record = &((*record)->next);
        }
    }
    
    if (!(*file)->has_eof) {
        // Reached end of file without reading an EOF record
        fprintf(stderr, "No EOF record in hex file.\n");
        goto free_records;
    }
    
    return 0;
    
free_records:
    free_intel_hex_file(*file);
close_file:
    fclose(f);
    return -1;
}

struct intel_hex_record *intel_hex_get_first_record (
                                                struct intel_hex_file *file)
{
    return file->head;
}

struct intel_hex_record *intel_hex_get_next_record (
                                                struct intel_hex_record *record,
                                                uint8_t **data,
                                                uint32_t *address,
                                                uint8_t *length)
{
    *data = record->data;
    *address = record->address;
    *length = record->length;
    
    return record->next;
}

uint8_t intel_hex_get_record_length (struct intel_hex_record *record)
{
    return record->length;
}

void free_intel_hex_file (struct intel_hex_file *file)
{
    for (struct intel_hex_record *i = file->head; i != NULL;) {
        struct intel_hex_record *r = i;
        i = i->next;
        free_record(r);
    }
    free(file);
}

int intel_hex_num_records (struct intel_hex_file *file)
{
    return file->num_records;
}
