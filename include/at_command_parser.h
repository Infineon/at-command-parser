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

/** @file
 *  Defines the AT Command Parser Interface.
 *
 *  This file provides the functions and definitions for using the AT Command Parser library.
 */

/**
 * \defgroup group_at_cmd_parser AT Command Parser API
 * \brief The AT Command Parser API provides functions to facilitate in implementing an AT command interface for an application.
 * \addtogroup group_at_cmd_parser
 * \{
 * \defgroup group_at_cmd_parser_macros Macros
 * \defgroup group_at_cmd_parser_typedefs Typedefs
 * \defgroup group_at_cmd_parser_structures Structures
 * \defgroup group_at_cmd_parser_functions Functions
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"
#include "cyabs_rtos.h"

/******************************************************
 *                    Constants
 ******************************************************/

/**
* \defgroup group_at_cmd_parser_results AT Command Parser results/error codes
 * @ingroup group_at_cmd_parser_macros
 *
 * At Command Parser Library APIs return results of type cy_rslt_t and consist of three parts:
 * - module base
 * - type
 * - error code
 *
 * \par Result Format
 *
   \verbatim
              Module base                        Type    Library-specific error code
      +----------------------------------------+------+------------------------------+
      |CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER | 0x2  |           Error Code         |
      +----------------------------------------+------+------------------------------+
                14 bits                         2 bits            16 bits

   See the macro section of this document for library-specific error codes.
   \endverbatim
 *
 * The data structure cy_rslt_t is part of cy_result.h located in <core_lib/include>
 *
 * Module base: This base is derived from CY_RSLT_MODULE_MIDDLEWARE_BASE (defined in cy_result.h) and is an offset of the CY_RSLT_MODULE_MIDDLEWARE_BASE.
 *
 * Type: This type is defined in cy_result.h and can be one of CY_RSLT_TYPE_FATAL, CY_RSLT_TYPE_ERROR, CY_RSLT_TYPE_WARNING, or CY_RSLT_TYPE_INFO. AWS library error codes are of type CY_RSLT_TYPE_ERROR.
 *
 * Library-specific error code: These error codes are library-specific and defined in the macro section.
 *
 * Helper macros used for creating the library-specific result are provided as part of cy_result.h.
 * \{
 */


/** Temporary until the real define gets added to cy_result.h */
#define CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER     (0x220)

/** General error */
#define CY_AT_CMD_PARSER_ERROR                      CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER, 0)
/** A bad argument was passed into the APIs */
#define CY_AT_CMD_PARSER_BAD_PARAM                  CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER, 1)
/** A memory allocation failed */
#define CY_AT_CMD_PARSER_NO_MEMORY                  CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER, 2)
/** Command buffer overflow */
#define CY_AT_CMD_PARSER_BUFFER_OVERFLOW            CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MIDDLEWARE_AT_CMD_PARSER, 3)
/** \} group_at_cmd_parser_macros */

/******************************************************
 *                      Enums
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/**
 * \addtogroup group_at_cmd_parser_structures
 * \{
 */

/** Command message base structure element.
 *
 * All command data structures must include the at_cmd_msg_base_t structure
 * as the the first element.
 */

typedef struct
{
    uint32_t cmd_id;                /**< Command id for this message      */
    uint32_t serial;                /**< Serial number for this message   */
} at_cmd_msg_base_t;

/**
 * Message queue structure for passing command messages.
 */

typedef struct
{
    at_cmd_msg_base_t *msg;         /**< Pointer to command message */
} at_cmd_msg_queue_t;

/** \} group_at_cmd_parser_structures */

/**
 * \addtogroup group_at_cmd_parser_typedefs
 * \{
 */
/** AT Command Parser callback function prototype.
 *
 * The callback parses the command arguments and allocates a command message structure. The structure
 * is defined by the application but must begin with at_cmd_msg_base_t as the first element of the structure.
 *
 * @param[in] cmd_id        : Command id of the command
 * @param[in] serial        : Serial number of the command
 * @param[in] cmd_args_len  : Length of the argument command string
 * @param[in] cmd_args      : Argument command string,
 *
 * @return Pointer to allocated message structure or NULL
 */

typedef at_cmd_msg_base_t * (*at_cmd_parser_callback_t)(uint32_t cmd_id, uint32_t serial, uint32_t cmd_args_len, uint8_t *cmd_args);

/** \} group_at_cmd_parser_typedefs */

/**
 * \addtogroup group_at_cmd_parser_structures
 * \{
 */

/**
 * Command table entry.
 */

typedef struct
{
    char                        *cmd_name;          /**< String command name                */
    uint32_t                    cmd_id;             /**< Command identifier for the command */
    at_cmd_parser_callback_t    cmd_parser;         /**< Parser callback for the command    */
} at_cmd_def_t;

/** \} group_at_cmd_parser_structures */

/**
 * \addtogroup group_at_cmd_parser_typedefs
 * \{
 */
/** Transport layer is data ready function prototype.
 *
 * Routine is called by the AT Command Parser library to query whether input is available
 * to be read.
 *
 * NOTE: If the application would like the library reader thread to block until data is
 * available rather than poll, this routine should not return until data is available.
 *
 * @param[in] opaque : Optional opaque pointer passed to the library during initialization.
 *
 * @return true if data is available for reading, false otherwise.
 */

typedef bool (*at_cmd_transport_is_data_ready)(void *opaque);

/** Transport layer read data function prototype.
 *
 * Routine is called by the AT Command Parser library to read input data.
 *
 * @param[inout] buffer : Pointer to the buffer to receive read data.
 * @param[in]    size   : Size of the buffer in bytes.
 * @param[in]    opaque : Optional opaque pointer passed to the library during initialization.
 *
 * @return Number of bytes read into the buffer.
 */

typedef uint32_t (*at_cmd_transport_read_data)(uint8_t *buffer, uint32_t size, void *opaque);

/** Transport layer write data function prototype.
 *
 * Routine is called by the AT Command Parser library to write output data.
 *
 * @param[in] buffer : Pointer to the buffer with data to write.
 * @param[in] length : Length of data in the buffer in bytes.
 * @param[in] opaque : Optional opaque pointer passed to the library during initialization.
 *
 * @return Status of the write operation.
 */
typedef cy_rslt_t (*at_cmd_transport_write_data)(uint8_t *buffer, uint32_t length, void *opaque);

/** \} group_at_cmd_parser_typedefs */

/**
 * \addtogroup group_at_cmd_parser_structures
 * \{
 */
/**
 * Initialization parameters.
 */

typedef struct
{
    cy_queue_t                      *cmd_msg_queue;     /**< Pointer to the initialized message queue */
    at_cmd_transport_is_data_ready  is_data_ready;      /**< Pointer to is data ready function        */
    at_cmd_transport_read_data      read_data;          /**< Pointer to read data function            */
    at_cmd_transport_write_data     write_data;         /**< Pointer to write data function           */
    void                            *opaque;            /**< Opaque application pointer               */
} at_cmd_params_t;

/** \} group_at_cmd_parser_structures */

/**
 * \addtogroup group_at_cmd_parser_functions
 * \{
 * The AT Command Parser library creates a worker thread for processing input data.
 * The priority of the thread is CY_RTOS_PRIORITY_NORMAL. The macro CY_RTOS_PRIORITY_NORMAL
 * is defined in abstraction-rtos/include/COMPONENT_THREADX/cyabs_rtos_impl.h.
 */

/******************************************************
 *               Function Declarations
 ******************************************************/

/** Initializes the AT Command Parser library.
 *
 * @param[in] params : Pointer to initialization parameters structure.
 *
 * @return    Status of the operation.
 */

cy_rslt_t at_cmd_parser_init(at_cmd_params_t *params);


/** Register a command table with the AT Command Parser library.
 *
 * \note The library stores a reference to the command table so the table
 * must be resident in persistent memory.
 *
 * @param[in] cmd_table : Pointer to the command table to be registered with the library.
 * @param[in] num_cmds  : Number of entries in the command table.
 *
 * @return    Status of the operation.
 */

cy_rslt_t at_cmd_parser_register_commands(at_cmd_def_t *cmd_table, uint32_t num_cmds);


/** Send a command response message.
 *
 * @param[in] serial : Serial number for the message
 * @param[in] status : Status value for the message
 * @param[in] text   : Optional text to include in the response message
 *
 * @return    Status of the operation.
 */

cy_rslt_t at_cmd_parser_send_cmd_response(uint32_t serial, uint32_t status, char *text);


/** Send an asynchronous command response message.
 *
 * @param[in] serial : Serial number for the message
 * @param[in] text   : Text to include in the response message
 *
 * @return    Status of the operation.
 */

cy_rslt_t at_cmd_parser_send_cmd_async_response(uint32_t serial, char *text);

/** \} group_at_cmd_parser_functions */
#ifdef __cplusplus
}
#endif

/** \} group_at_cmd_parser */
