//
//  intel-hex.h
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-30.
//  Copyright © 2019 Samuel Dewan.
//

#ifndef intel_hex_h
#define intel_hex_h

#include <inttypes.h>

struct intel_hex_record;
struct intel_hex_file;

/**
 *  Parse an intel hex file into a hex file structure.
 *
 *  @param name The name of the file to be parsed
 *  @param file Pointer to where pointer to hex file structure should be placed
 *
 *  @return 0 if successfull
 */
extern int parse_intel_hex_file (const char *name,
                                 struct intel_hex_file **file);

/**
 *  Free a hex file data structue and all of the records it contains.
 *
 *  @param file The hex file structure to be freed
 */
extern void free_intel_hex_file (struct intel_hex_file *file);

/**
 *  Get the first data record from a hex file structure.
 *
 *  @param file The structure from which the first record should be gotten
 *
 *  @return Pointer to first data record in the structure
 */
extern struct intel_hex_record *intel_hex_get_first_record (
                                                struct intel_hex_file *file);

/**
 *  Get the next record from a hex file structure and get the information from
 *  the current record.
 *
 *  @param record The current record
 *  @param data Pointer to where data pointer from current record should be
 *              placed
 *  @param address Pointer to where address from current record should be placed
 *  @param length Pointer to where length from current record should be placed
 *
 *  @return Pointer to next record in the structure
 */
extern struct intel_hex_record *intel_hex_get_next_record (
                                                struct intel_hex_record *record,
                                                uint8_t **data,
                                                uint32_t *address,
                                                uint8_t *length);

/**
 *  Get the length of a record.
 *
 *  @param record The record for which the length should be gotten
 *
 *  @return The length of the record
 */
extern uint8_t intel_hex_get_record_length (struct intel_hex_record *record);

/**
 *  Get the total number of data record in an intel hex file structure.
 *
 *  @param file The hex file structure for which the number of records should be
 *              gotten
 *
 *  @return The number of data records in the hex file structure
 */
extern int intel_hex_num_records (struct intel_hex_file *file);

#endif /* intel_hex_h */
