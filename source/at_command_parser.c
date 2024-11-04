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
* @file at_command_parser.c
* @brief Main file for the AT Command Parser Library.
*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

#include "cy_result.h"
#include "cyabs_rtos.h"
#include "cy_log.h"

#include "at_command_parser.h"
#include "at_command_parser_private.h"

/******************************************************
 *                      Macros
 ******************************************************/

#ifdef ENABLE_AT_CMD_LOGS
#define at_cy_log_msg cy_log_msg
#else
#define at_cy_log_msg(a,b,c,...)
#endif

/******************************************************
 *                    Constants
 ******************************************************/

#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE           (64U)
#endif

#define INPUT_THREAD_STACK_SIZE     (6*1024)

#define AT_CMD_MSG_QUEUE_TIMEOUT    (200)

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/

static at_cmd_parser_t g_cmd_parser;

/******************************************************
 *               Function Definitions
 ******************************************************/

static void at_cmd_reset_command_buffer(at_cmd_parser_t *cmd_parser)
{
    cmd_parser->reading_cmd       = false;
    cmd_parser->cmd_header        = false;
    cmd_parser->cmd_widx          = 0;
    cmd_parser->cmd_size          = 0;
    cmd_parser->at_cmd_prefix_idx = 0;
}


at_cmd_msg_base_t *at_cmd_parse_cmd(at_cmd_parser_t *cmd_parser, uint32_t serial, uint32_t cmd_len, uint8_t *cmd_buf)
{
    at_cmd_msg_base_t *msg;
    at_cmd_table_t *table;
    at_cmd_def_t *cmd;
    uint8_t *ptr;
    uint32_t i;
    int len;

    if (cmd_buf == NULL)
    {
        return NULL;
    }

    at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AT CMD: parsing command: %s\n", (char *)cmd_buf);

    /*
     * Scan until we find a comma or a nul.
     */

    for (ptr = cmd_buf; *ptr != '\0'; ptr++)
    {
        if (*ptr == ',')
        {
            *ptr++ = '\0';
            break;
        }
    }

    /*
     * Time to find a command match.
     */

    len = strlen((char *)cmd_buf);
    for (table = cmd_parser->cmd_tables, cmd = NULL; table != NULL && cmd == NULL; table = table->next)
    {
        for (i = 0; i < table->num_cmds; i++)
        {
            if (table->cmd_table[i].cmd_name != NULL && strlen(table->cmd_table[i].cmd_name) == len && !strncmp((char *)cmd_buf, table->cmd_table[i].cmd_name, len))
            {
                cmd = &table->cmd_table[i];
                break;
            }
        }
    }

    if (cmd == NULL)
    {
        /*
         * Didn't find a matching command.
         */

        return NULL;
    }

    /*
     * Invoke the command callback.
     */

    msg = cmd->cmd_parser(cmd->cmd_id, serial, cmd_len - ((uint32_t)ptr - (uint32_t)cmd_buf), ptr);

    return msg;
}


/** Process a command buffer.
 *
 * @param[in] cmd_parser : Pointer to the main parser structure
 * @param[in] buffer     : Pointer to the command message buffer.
 * @param[in] count      : Number of bytes of data in message buffer.
 *
 * @return    Status of the operation.
 */

static cy_rslt_t at_cmd_process_command_buffer(at_cmd_parser_t *cmd_parser, uint8_t *buffer, uint32_t count)
{
    at_cmd_msg_queue_t msg_queue_entry;
    at_cmd_msg_base_t *msg;
    cy_rslt_t result;
    char *ptr;
    char *end;
    uint32_t serial;
    uint32_t size;
    int i;

    at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AT CMD: incoming command: %s\n", (char *)buffer);

    if (cmd_parser->echo_cmd)
    {
        /*
         * Echo the AT command.
         */

        cmd_parser->write_data(buffer, count, cmd_parser->opaque);
        cmd_parser->write_data((uint8_t *)"\n\r", 2, cmd_parser->opaque);
    }

    /*
     * Make sure we have a valid message header.
     */

    if (count < AT_CMD_MIN_HEADER_SIZE || strncmp((const char *)buffer, cmd_parser->at_cmd_prefix, AT_CMD_PREFIX_CHARS))
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid msg header: %.*s\n", AT_CMD_MIN_HEADER_SIZE, (char *)buffer);
        at_cmd_parser_send_cmd_response(0, 1, "Invalid command");
        return CY_AT_CMD_PARSER_ERROR;
    }

    /*
     * Extract the data size.
     */

    for (size = 0, i = 0; i < AT_CMD_SIZE_CHARS; i++)
    {
        if (!isdigit(buffer[AT_CMD_PREFIX_CHARS + i]))
        {
            at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid size digit %c\n", buffer[AT_CMD_PREFIX_CHARS + i]);
            at_cmd_parser_send_cmd_response(0, 1, "Invalid size digit");
            return CY_AT_CMD_PARSER_ERROR;
        }
        size = (size * 10) + buffer[AT_CMD_PREFIX_CHARS + i] - '0';
    }

    if (size > AT_CMD_MAX_SIZE)
    {
        at_cmd_parser_send_cmd_response(0, 1, "Invalid size");
        return CY_AT_CMD_PARSER_ERROR;
    }

    /*
     * There must be a serial number specified.
     */

    ptr = (char *)&buffer[AT_CMD_PREFIX_CHARS + AT_CMD_SIZE_CHARS];
    serial = 0;
    while (isdigit((int)*ptr))
    {
        serial = (serial * 10) + (*ptr++) - '0';
    }

    if (*ptr != AT_CMD_TERMINATOR_CHAR)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid msg separator: %c\n", *ptr);
        at_cmd_parser_send_cmd_response(0, 1, "Invalid format");
        return CY_AT_CMD_PARSER_ERROR;
    }
    ptr++;

    /*
     * Strip off the trailing ;
     */

    end = (char *)((uint32_t)buffer + count - 1);
    if (*end == AT_CMD_TERMINATOR_CHAR)
    {
        *end = 0;
        count--;
    }

    /*
     * Send the command to the command parser.
     */

    msg = at_cmd_parse_cmd(cmd_parser, serial, count - ((uint32_t)ptr - (uint32_t)buffer), (uint8_t *)ptr);
    if (msg == NULL)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid cmd\n");
        at_cmd_parser_send_cmd_response(0, 1, "Invalid cmd");
        return CY_AT_CMD_PARSER_ERROR;
    }

    /*
     * And send it off.
     */

    memset(&msg_queue_entry, 0, sizeof(msg_queue_entry));
    msg_queue_entry.msg = msg;

    if ((result = cy_rtos_queue_put(cmd_parser->msg_queue, &msg_queue_entry, AT_CMD_MSG_QUEUE_TIMEOUT)) != CY_RSLT_SUCCESS)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: error sending msg\n");
        at_cmd_parser_send_cmd_response(0, 1, "queue error");
        free(msg);
    }

    return result;
}


static uint32_t at_cmd_scan_cmd_header(at_cmd_parser_t *cmd_parser, uint8_t *chars, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
    {

        /*
         * Are we extracting the command length field?
         */

        if (cmd_parser->cmd_widx < (AT_CMD_PREFIX_CHARS + AT_CMD_SIZE_CHARS))
        {
            if (!isdigit(chars[i]))
            {
                at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid size digit %c\n", chars[i]);
                at_cmd_parser_send_cmd_response(0, 1, "Invalid size digit");
                at_cmd_reset_command_buffer(cmd_parser);

                return count;
            }
            cmd_parser->cmd_size = (cmd_parser->cmd_size * 10) + chars[i] - '0';
            cmd_parser->command_buffer[cmd_parser->cmd_widx++] = chars[i];
            continue;
        }

        if (cmd_parser->cmd_widx >= (AT_CMD_PREFIX_CHARS + AT_CMD_SIZE_CHARS))
        {
            /*
             * We need at least one digit for the serial number.
             * After that it's just digits until we hit the ';' character.
             */

            if ((cmd_parser->cmd_widx == (AT_CMD_PREFIX_CHARS + AT_CMD_SIZE_CHARS)) && !isdigit(chars[i]))
            {
                at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid serial number digit %c\n", chars[i]);
                at_cmd_parser_send_cmd_response(0, 1, "Invalid serial digit");
                at_cmd_reset_command_buffer(cmd_parser);

                return count;
            }
            else if (!isdigit(chars[i]) && chars[i] != AT_CMD_TERMINATOR_CHAR)
            {
                at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: invalid format %c\n", chars[i]);
                at_cmd_parser_send_cmd_response(0, 1, "Invalid format");
                at_cmd_reset_command_buffer(cmd_parser);

                return count;
            }

            if (chars[i] == AT_CMD_TERMINATOR_CHAR)
            {
                cmd_parser->command_buffer[cmd_parser->cmd_widx++] = chars[i];
                cmd_parser->cmd_header = false;
                if (cmd_parser->cmd_size > 0)
                {
                    /*
                     * The specified command size is the number of characters between the ';'
                     * characters. Add in what we've buffered so far and the trailing
                     * ';' to the total size.
                     */

                    cmd_parser->cmd_size += cmd_parser->cmd_widx + 1;
                    at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG1, "AT CMD: Total cmd size %lu\n", cmd_parser->cmd_size);

                    /*
                     * Make sure the command size isn't too large for the input buffer.
                     */

                    if (cmd_parser->cmd_size > AT_CMD_PARSER_BUFFER_SIZE)
                    {
                        at_cmd_parser_send_cmd_response(0, 1, "Input buffer size exceeded");
                        at_cmd_reset_command_buffer(cmd_parser);

                        return count;
                    }
                }
                at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG1, "AT CMD: Header complete\n");
                break;
            }
        }
        cmd_parser->command_buffer[cmd_parser->cmd_widx++] = chars[i];
    }

    return i;
}


static uint32_t at_cmd_scan_for_prefix(at_cmd_parser_t *cmd_parser, uint8_t *chars, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
    {
        if (chars[i] == cmd_parser->at_cmd_prefix[cmd_parser->at_cmd_prefix_idx])
        {
            cmd_parser->command_buffer[cmd_parser->cmd_widx++] = chars[i];
            if (++cmd_parser->at_cmd_prefix_idx == AT_CMD_PREFIX_CHARS)
            {
                at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG1, "AT CMD: Command start detected\n");
                cmd_parser->at_cmd_prefix_idx = 0;
                cmd_parser->reading_cmd       = true;
                cmd_parser->cmd_header        = true;
                i++;
                break;
            }
        }
        else
        {
            if ((cmd_parser->at_cmd_prefix_idx > 0) && (chars[i] == cmd_parser->at_cmd_prefix[0]))
            {
                /*
                 * Special case. We were processing a command start sequence and received a bad character.
                 * But the character matches the start of a possible command start sequence. We need to
                 * save it rather than throwing it away.
                 */

                cmd_parser->command_buffer[0] = chars[i];
                cmd_parser->at_cmd_prefix_idx = 1;
                cmd_parser->cmd_widx          = 1;
            }
            else
            {
                cmd_parser->at_cmd_prefix_idx = 0;
                cmd_parser->cmd_widx          = 0;
            }
        }
    }

    return i;
}


/** Add characters to the incoming command buffer.
 *
 * @param[in] cmd_parser : Pointer to the main parser structure
 * @param[in] chars      : Pointer to the characters to add.
 * @param[in] count      : Number of characters to add.
 *
 * @return    Status of the operation.
 */

static cy_rslt_t at_cmd_add_command_chars(at_cmd_parser_t *cmd_parser, uint8_t *chars, uint32_t count)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t bytes_used;
    uint32_t i;
    int len;

    if (cmd_parser == NULL || chars == NULL || count == 0)
    {
        return CY_AT_CMD_PARSER_BAD_PARAM;
    }

    if (cmd_parser->cmd_widx + count >= AT_CMD_PARSER_BUFFER_SIZE)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: input buffer overflow\n");
        at_cmd_parser_send_cmd_response(0, 1, "Input buffer size exceeded");
        at_cmd_reset_command_buffer(cmd_parser);
        return CY_AT_CMD_PARSER_BUFFER_OVERFLOW;
    }

    /*
     * Are we scanning for the command prefix?
     */

    bytes_used = 0;
    if (!cmd_parser->reading_cmd)
    {
        bytes_used = at_cmd_scan_for_prefix(cmd_parser, chars, count);
    }

    if (!cmd_parser->reading_cmd)
    {
        return CY_RSLT_SUCCESS;
    }

    for (i = bytes_used; i < count; i++)
    {
        /*
         * Are we reading the command header information?
         */

        if (cmd_parser->cmd_header)
        {
            bytes_used = at_cmd_scan_cmd_header(cmd_parser, &chars[i], count - i);
            i += bytes_used;
            continue;
        }

        switch (chars[i])
        {
            case 10: /* line feed - ignore it if size is not specified. */
                if (cmd_parser->cmd_size)
                {
                    cmd_parser->command_buffer[cmd_parser->cmd_widx++] = chars[i];
                }
                break;

            default:
                cmd_parser->command_buffer[cmd_parser->cmd_widx] = chars[i];
                if (!cmd_parser->cmd_size && cmd_parser->command_buffer[cmd_parser->cmd_widx] == '\r')
                {
                    /*
                     * nul terminate the buffer but don't include the trailing nul in the character count.
                     */

                    cmd_parser->command_buffer[cmd_parser->cmd_widx] = '\0';

                    result = at_cmd_process_command_buffer(cmd_parser, cmd_parser->command_buffer, cmd_parser->cmd_widx);
                    at_cmd_reset_command_buffer(cmd_parser);
                }
                else if (cmd_parser->cmd_size && cmd_parser->cmd_widx == cmd_parser->cmd_size - 1)
                {
                    /*
                     * nul terminate the buffer but don't include the trailing nul in the character count.
                     */

                    cmd_parser->command_buffer[++cmd_parser->cmd_widx] = '\0';

                    /*
                     * Since a command size was specified, it's required that the command string end with ;
                     */

                    len = cmd_parser->cmd_widx;
                    if (cmd_parser->command_buffer[len - 1] != AT_CMD_TERMINATOR_CHAR)
                    {
                        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: bad cmd trailer\n");
                        at_cmd_parser_send_cmd_response(0, 1, "bad cmd trailer");
                        at_cmd_reset_command_buffer(cmd_parser);
                        return CY_AT_CMD_PARSER_ERROR;
                    }
                    result = at_cmd_process_command_buffer(cmd_parser, cmd_parser->command_buffer, len);
                    at_cmd_reset_command_buffer(cmd_parser);

                    /*
                     * Skip over any trailing whitespace in the buffer.
                     * Note that i should currently be on the ;.
                     */

                    while (i + 1 < count && isspace((int)chars[i + 1]))
                    {
                        ++i;
                    }
                }
                else
                {
                    cmd_parser->cmd_widx++;
                }
                break;
        }
    }

    return result;
}


static void at_cmd_input_thread_func(cy_thread_arg_t arg)
{
    at_cmd_parser_t *cmd_parser = (at_cmd_parser_t *)arg;
    uint32_t count;
    uint8_t buffer[INPUT_BUFFER_SIZE];

    at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AT CMD: Input thread starting\n");
    while (1)
    {
        /* Check if host sent any data */
        if (cmd_parser->is_data_ready(cmd_parser->opaque))
        {
            /* Get number of bytes */
            count = cmd_parser->read_data(buffer, INPUT_BUFFER_SIZE, cmd_parser->opaque);
            if (0 != count)
            {
                at_cmd_add_command_chars(cmd_parser, buffer, count);
            }
        }
        else
        {
            /*
             * No data waiting. Sleep for a bit before checking again.
             */

            cy_rtos_delay_milliseconds(1);
        }
    }
}


static cy_rslt_t at_cmd_send_host_message(bool async_msg, uint32_t serial, uint32_t status, char *text)
{
    cy_rslt_t result;
    uint32_t buflen = AT_CMD_PARSER_BUFFER_SIZE;
    uint32_t data_bytes;
    char *data;
    char *size;
    char *ptr;
    int chars;
    int i;

    /*
     * Grab the mutex to make sure no one else is using the output buffer.
     */

    result = cy_rtos_mutex_get(&g_cmd_parser.output_mutex, CY_RTOS_NEVER_TIMEOUT);
    if (result != CY_RSLT_SUCCESS)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: Error getting output mutex\n");
        return CY_AT_CMD_PARSER_ERROR;
    }

    /*
     * We need to construct the proper message header.
     */

    ptr = (char *)g_cmd_parser.output_buffer;
    *ptr++ = '+';

    if (async_msg)
    {
        /*
         * Asynchronous host message.
         */

        *ptr++ = 'H';
    }
    else
    {
        /*
         * Status message.
         */

        *ptr++ = 'S';
    }

    /*
     * Allow 4 digits for the message size.
     */

    size = ptr;
    for (i = 0; i < AT_CMD_SIZE_CHARS; i++)
    {
        *ptr++ = '0';
    }

    /*
     * Add in the serial number.
     */

    *ptr++ = ',';
    chars = snprintf(ptr, buflen - (uint32_t)((uint32_t)ptr - (uint32_t)g_cmd_parser.output_buffer), "%" PRIu32, serial);
    ptr += chars;

    *ptr++ = ';';
    data   = ptr;

    if (!async_msg)
    {
        /*
         * Add in the status value.
         */

        chars = snprintf(ptr, buflen - (uint32_t)((uint32_t)ptr - (uint32_t)g_cmd_parser.output_buffer), "%" PRIu32, status);
        ptr += chars;

        /*
         * And any optional message text.
         */

        if (text != NULL && text[0] != '\0')
        {
            chars = snprintf(ptr, buflen - (uint32_t)((uint32_t)ptr - (uint32_t)g_cmd_parser.output_buffer), ",%s", text);
            ptr += chars;
        }
    }
    else
    {
        /*
         * Add in the asynchronous host message text.
         */

        chars = snprintf(ptr, buflen - (uint32_t)((uint32_t)ptr - (uint32_t)g_cmd_parser.output_buffer), "%s", text);
        ptr += chars;
    }

    /*
     * Add in the data size and the trailing ';'
     */

    data_bytes = (uint32_t)((uint32_t)ptr - (uint32_t)data);
    size += AT_CMD_SIZE_CHARS - 1;
    for (i = 0; i < AT_CMD_SIZE_CHARS; i++)
    {
        *size-- = (data_bytes % 10) + '0';
        data_bytes /= 10;
    }

    *ptr++ = ';';
    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr   = '\0';

    /*
     * Send the message off to the external host.
     */

    result = g_cmd_parser.write_data(g_cmd_parser.output_buffer, (uint32_t)((uint32_t)ptr - (uint32_t)g_cmd_parser.output_buffer), g_cmd_parser.opaque);

    /*
     * Release the mutex.
     */

    cy_rtos_mutex_set(&g_cmd_parser.output_mutex);

    return result;
}


cy_rslt_t at_cmd_parser_init(at_cmd_params_t *params)
{
    cy_rslt_t result;

    if (params == NULL || params->cmd_msg_queue == NULL || params->is_data_ready == NULL ||
        params->read_data == NULL || params->write_data == NULL)
    {
        return CY_AT_CMD_PARSER_BAD_PARAM;
    }

    /*
     * Copy over the parameters.
     */

    g_cmd_parser.msg_queue     = params->cmd_msg_queue;
    g_cmd_parser.is_data_ready = params->is_data_ready;
    g_cmd_parser.read_data     = params->read_data;
    g_cmd_parser.write_data    = params->write_data;
    g_cmd_parser.opaque        = params->opaque;

    /*
     * Initialize the output buffer mutex.
     */

    result = cy_rtos_mutex_init(&g_cmd_parser.output_mutex, true);
    if (result != CY_RSLT_SUCCESS)
    {
        at_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "AT CMD: Error creating output mutex\n");
        return CY_AT_CMD_PARSER_ERROR;
    }

    /*
     * Set up the AT command prefix string for input scanning.
     */

    strcpy(g_cmd_parser.at_cmd_prefix, AT_CMD_PREFIX);

    /*
     * Spawn off our input thread.
     */

    result = cy_rtos_create_thread(&g_cmd_parser.input_thread, at_cmd_input_thread_func, "Input Thread", NULL,
                                    INPUT_THREAD_STACK_SIZE, CY_RTOS_PRIORITY_NORMAL, (cy_thread_arg_t)&g_cmd_parser);

    return result;
}


cy_rslt_t at_cmd_parser_register_commands(at_cmd_def_t *cmd_table, uint32_t num_cmds)
{
    at_cmd_table_t *cmd_table_node;
    at_cmd_table_t *ptr;

    if (cmd_table == NULL || num_cmds == 0)
    {
        return CY_AT_CMD_PARSER_BAD_PARAM;
    }

    /*
     * Allocate a new command table node.
     */

    cmd_table_node = calloc(1, sizeof(at_cmd_table_t));
    if (cmd_table_node == NULL)
    {
        return CY_AT_CMD_PARSER_NO_MEMORY;
    }

    cmd_table_node->cmd_table = cmd_table;
    cmd_table_node->num_cmds  = num_cmds;

    /*
     * Add the command table node to our list.
     */

    if (g_cmd_parser.cmd_tables == NULL)
    {
        g_cmd_parser.cmd_tables = cmd_table_node;
    }
    else
    {
        for (ptr = g_cmd_parser.cmd_tables; ptr->next != NULL; ptr = ptr->next)
            ;
        ptr->next = cmd_table_node;
    }

    return CY_RSLT_SUCCESS;
}

cy_rslt_t at_cmd_parser_send_cmd_response(uint32_t serial, uint32_t status, char *text)
{
    return at_cmd_send_host_message(false, serial, status, text);
}

cy_rslt_t at_cmd_parser_send_cmd_async_response(uint32_t serial, char *text)
{
    return at_cmd_send_host_message(true, serial, 0, text);
}
