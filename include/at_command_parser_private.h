/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

/**
 * @file at_command_parser_private.h
 * @brief AT Command Parser Library internal definitions
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "at_command_parser.h"

/******************************************************
 *                     Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define AT_CMD_PARSER_BUFFER_SIZE           (6*1024+40)

#define AT_CMD_PREFIX                       "AT+"

#define AT_CMD_PREFIX_CHARS                 (3)     /* AT+                          */
#define AT_CMD_SIZE_CHARS                   (4)     /* 4 digit command data size    */

#define AT_CMD_MIN_HEADER_SIZE              (AT_CMD_PREFIX_CHARS + AT_CMD_SIZE_CHARS + 1)

#define AT_CMD_TERMINATOR_CHAR              ';'

#define AT_CMD_MAX_SIZE                     (6000)

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct at_cmd_table_def_s
{
    struct at_cmd_table_def_s *next;
    at_cmd_def_t *cmd_table;
    uint32_t num_cmds;
} at_cmd_table_t;

typedef struct
{
    cy_thread_t input_thread;
    cy_queue_t  *msg_queue;

    at_cmd_transport_is_data_ready is_data_ready;
    at_cmd_transport_read_data     read_data;
    at_cmd_transport_write_data    write_data;
    void *opaque;

    at_cmd_table_t *cmd_tables;

    bool echo_cmd;
    bool reading_cmd;
    bool cmd_header;
    char at_cmd_prefix[AT_CMD_PREFIX_CHARS + 1];
    int at_cmd_prefix_idx;

    uint8_t command_buffer[AT_CMD_PARSER_BUFFER_SIZE];
    uint32_t cmd_widx;
    uint32_t cmd_size;

    uint8_t output_buffer[AT_CMD_PARSER_BUFFER_SIZE];
    cy_mutex_t output_mutex;
} at_cmd_parser_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/


#ifdef __cplusplus
}
#endif
