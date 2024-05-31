# AT Command Parser Library
AT command parser is a library which helps application developers to quickly connect to Cloud via MQTT using AT commands.


## Overview
The AT command parser library provides APIs to intialize the AT command parser library and to register the AT Command table.
The AT command library parses string-based AT commands from the UART.
The library verifies that the command is a valid command in the correct format and then processes all of the common fields. 
The Command_Name field is extracted and compared against the command tables that have been registered with the library via API at_cmd_parser_register_commands(), when a match is found,
the associated callback routine is invoked parse the message arguments. The callback routine creates a command message and returns that to the library to be passed to the application for processing.

The library provide APIs for

- Library initialization
- Registering a command table
- Sending response and asynchronous messages

## AT Command format

### AT+XXXX#;Command_Name[,JSON_Text];\n

- XXXX: Four-digit length indicating the number of bytes in the command. Count denotes the number of characters between the semicolons.
- #: Serial number of the command. Serial number may be a variable number of digits.
- Command_Name: The command name from the registered command table.
- JSON_Text: Command specific JSON text with any arguments for the command. The JSON test is parsed by the command callback function.

NOTE: If XXXX command length is zero then the command length will be end of carriage return/line feed.

### Successful Response

+SXXXX,#;0[,JSON_Text];\n

- XXXX: Four-digit length indicating the number of bytes in the response message. Count denotes the number of characters between the semicolons.
- #: Serial number that is given by host in AT command.
- JSON_Text: Optional command specific response information.

### Error Response

+SXXXX,#;NN[,Error_Message];\n

- XXXX: Four-digit length indicating the number of bytes in the response message. Count denotes the number of characters between the semicolons.
- #: Serial number that is given by host in AT command.
- NN: Non-zero error number returned for command.
- Error_Message: Optional error message string.

### Asynchronous Message
Some commands may initiate actions which cause later messages to be generated. For example, starting a scan and the subsequent scan results.

+HXXXX,#;Message_Name,JSON_Text;\n

- XXXX: Four-digit length indicating the number of bytes in the response message. Count denotes the number of characters between the semicolons.
- #: Serial number of the AT command which caused this message to be generated.
- Message_Name: String name of the message. For example: 'ScanResult'
- JSON_Text: Message specific response information.


## Supported platforms

- CYW955913EVK-01 Wi-Fi Bluetooth&reg; prototyping kit

## More information

For more information, refer to the following documents:

* [AT Command Parser RELEASE.md](./RELEASE.md)
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software)
* [Infineon Technologies AG](https://www.infineon.com)


---
© 2024, Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation.
