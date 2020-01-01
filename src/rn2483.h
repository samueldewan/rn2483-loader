//
//  rn2483.h
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-31.
//  Copyright © 2019 Samuel Dewan.
//

#ifndef rn2483_h
#define rn2483_h

/**
 *  Get the version string from a RN2483 radio module.
 *
 *  @param fd File descriptor for serial connection to radio
 *  @param str Pointer to memory where version string should be placed
 *  @param length Maximum length of version string to be read
 *  @param timeout Timeout in milliseconds
 *
 *  @return 0 if successfull
 */
extern int rn2483_get_version (int fd, char *str, int length, long timeout);

/**
 *  Erase an RN2483 radio module and have it enter the bootloader.
 *
 *  @param fd File descriptor for serial connection to radio
 *
 *  @return 0 if successfull
 */
extern int rn2483_erase (int fd);

#endif /* rn2483_h */
