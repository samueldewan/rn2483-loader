//
//  rn2483.c
//  rn2483-loader
//
//  Created by Samuel Dewan on 2019-12-31.
//  Copyright © 2019 Samuel Dewan.
//

#include "rn2483.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

static long get_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((long)ts.tv_sec * 1000) + (long)(ts.tv_nsec / 1000000L);
}

/**
 * Send a command to the radio module and wait for a response.
 *
 * @param fd File descriptor for serial connection to radio
 * @param command Command string to be sent
 * @param response String where response should be stored
 * @param length Maximum response length
 * @param timeout Timeout in milliseconds
 */
static int rn2483_do_command (int fd, const char *command, char *response,
                              int length, long timeout)
{
    ssize_t len = 0;
    
    /* Send command */
    while (len < (ssize_t)strlen(command)) {
        ssize_t nbytes = write(fd, command + len, strlen(command) - len);
        
        if (nbytes == -1) {
            fprintf(stderr, "Could not write command to RN2483: %s.\n",
                    strerror(errno));
            return -1;
        }
        
        len += nbytes;
    }
    
    if ((response == NULL) || (length == 0)) {
        // No response expected
        return 0;
    }
    
    long start_time = get_millis();
    
    len = 0;
    int have_line = 0;
    /* Get response */
    while (!have_line && (len < (length - 1))) {
        fd_set set;
        FD_SET(fd, &set);
        
        // Calculate remaining timeout
        long remaining_time = 0;
        struct timeval t;
        
        if (timeout) {
            remaining_time = timeout - (get_millis() - start_time);
            
            if (remaining_time <= 0) {
                fprintf(stderr, "Timed out waiting for response from RN2483.\n");
                return -1;
            }
            
            t.tv_sec = remaining_time / 1000;
            t.tv_usec = (remaining_time % 1000) * 1000;
        }
        
        select(fd + 1, &set, NULL, NULL, timeout ? &t : NULL);
        
        if (FD_ISSET(fd, &set)) {
            // There is data available to be read from the file descriptor
            ssize_t nbytes = read(fd, response + len, length - len);
            len += nbytes;
            
            if (nbytes == -1) {
                fprintf(stderr, "Could not read from RN2483: %s.\n",
                        strerror(errno));
                return -1;
            } else if (strchr(response, '\n')) {
                // Add terminating char
                response[len] = '\0';
                // Remove end of line characters
                // (last two chars should be "\r\n")
                response[len - 2] = '\0';
                break;
            }
        }
    }
    
    // Make sure that response is terminated
    if (!have_line) {
        response[length - 1] = '\0';
    }
    
    return 0;
}

int rn2483_get_version (int fd, char *response, int length, long timeout)
{
    return rn2483_do_command(fd, "sys get ver\r\n", response, length, timeout);
}

extern int rn2483_erase (int fd)
{
    return rn2483_do_command(fd, "sys eraseFW\r\n", NULL, 0, 0);
}
