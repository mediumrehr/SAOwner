/**
 * 
 * SAOwner
 * Shitty add-on hacking tool
 * @mediumrehr
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include <asf.h>
#include <stdio.h>
#include <string.h>

void i2c_read_request_callback(struct i2c_slave_module *const module);
void i2c_slave_read_complete_callback(struct i2c_slave_module *const module);
void i2c_write_request_callback(struct i2c_slave_module *const module);
void i2c_slave_write_complete_callback(struct i2c_slave_module *const module);
void usart_read_callback(struct usart_module *const usart_module);
void usart_write_callback(struct usart_module *const usart_module);
void i2c_master_write_complete_callback(struct i2c_master_module *const module);
void i2c_master_read_complete_callback(struct i2c_master_module *const module);

void configure_i2c_slave(void);
void configure_i2c_slave_callbacks(void);
void configure_i2c_master(void);
void configure_i2c_master_callbacks(void);
void configure_usart(void);
void configure_usart_callbacks(void);

void set_genie_mode(uint8_t new_mode);
uint8_t set_slave_address(char* newAddr);
uint8_t set_destination_address(char* newAddr);
void prompt_user(char* prompt, uint8_t promptLen, char* received, uint8_t maxInputLen);

uint8_t maxCmdLen = 0;
void (*usart_handler)(uint8_t *, uint8_t);
void new_slave_handler(uint8_t *newSlaveAddr, uint8_t strLen);
uint8_t wait_for_user_input(char *inputBuffer, uint8_t bufferLen);

/* i2c packets */
#define PACKET_BUFFER_SIZE 20
uint8_t packet_in_index = 0;
static uint8_t dest_addr = 0;
static struct i2c_slave_packet packet_in;
struct i2c_master_packet wr_packet_out;
struct i2c_master_packet rd_packet_out;

#define DATA_LENGTH 10
static uint8_t write_buffer_in[DATA_LENGTH] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
static uint8_t read_buffer_in [DATA_LENGTH];

static uint8_t write_buffer_out[DATA_LENGTH] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
static uint8_t write_buffer_out_reversed[DATA_LENGTH] = {0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
static uint8_t read_buffer_out[DATA_LENGTH];

/* i2c definitions and declarations */
uint8_t redirect_address = 0x8; // default redirect slave address

#define GENIE_MODE_OFF 			0
#define GENIE_MODE_PASSTHROUGH 	1
#define GENIE_MODE_REDIRECT		2
#define GENIE_MODE_INJECT		3
#define GENIE_MODE_BLOCK		4

uint8_t genie_mode = GENIE_MODE_OFF;

#define UART_MENU_MAIN				0
#define UART_MENU_MODE				1
#define UART_MENU_REDIRECT			2
#define UART_WAIT_NEW_SLAVE_ADDR	3

uint8_t uart_menu = UART_MENU_MAIN;
uint8_t uart_show_activity = true;
uint8_t enable_master_usart_reader = true;
uint8_t enable_master_usart_writer = true;

/* Init device instance */
struct i2c_slave_module i2c_slave_instance;
struct i2c_master_module i2c_master_instance;
struct usart_module usart_instance;

#define MAX_RX_BUFFER_LENGTH   1
#define MAX_CMD_BUFFER_LENGTH  10

volatile uint8_t rx_buffer[MAX_RX_BUFFER_LENGTH];
uint8_t cmd_buffer[MAX_CMD_BUFFER_LENGTH];

uint16_t cmdLen = 0;

/**
 * Master requests data from slave
 */
void i2c_read_request_callback(struct i2c_slave_module *const module)
{
	uint8_t index = packet_in_index;

	uint32_t addr = module->hw->I2CM.ADDR.reg;
	uint32_t data = module->hw->I2CM.DATA.reg; // dest addr + read bit

	uint16_t buffer[50] = { 0 };
	sprintf(&buffer, "addr: %08X data: %08X\n", addr, data);
	usart_write_buffer_wait(&usart_instance, buffer, sizeof(buffer));

	rd_packet_out.address = module->hw->I2CS.DATA.reg >> 1;
	rd_packet_out.data_length = DATA_LENGTH;
	rd_packet_out.data = read_buffer_in;
	rd_packet_out.high_speed = false;
	rd_packet_out.ten_bit_address = false;

	// /* Read buffer from master */
	while (i2c_master_read_packet_job(&i2c_master_instance, &rd_packet_out) == STATUS_BUSY) {
		;
	}
	
	/* Init i2c packet */
	packet_in.data_length = DATA_LENGTH;
	packet_in.data        = read_buffer_in;

	/* Write buffer to master */
	i2c_slave_write_packet_job(module, &packet_in);
	
	// packet_in_index++;
	// if (packet_in_index >= PACKET_BUFFER_SIZE) {
	// 	packet_in_index = 0;
	// }
}

/**
 * Master writes data to slave
 */
void i2c_write_request_callback(struct i2c_slave_module *const module)
{	
	uint8_t index = packet_in_index;
	// char temp[10];
	// sprintf(temp, "len: %u\n", (unsigned int)module->buffer_remaining);
	// usart_write_buffer_wait(&usart_instance, temp, sizeof(temp));
	/* Init i2c packet */
	packet_in.data_length = DATA_LENGTH;
	packet_in.data        = read_buffer_in;
	
	dest_addr = module->hw->I2CS.DATA.reg >> 1;

	/* Read buffer from master */
	while (i2c_slave_read_packet_job(module, &packet_in) == STATUS_BUSY) {
		;
	}
	
	packet_in_index++;
	if (packet_in_index >= PACKET_BUFFER_SIZE) {
		packet_in_index = 0;
	}
}

/**
 * Slave finishes sending data to master
 */
void i2c_slave_write_complete_callback(struct i2c_slave_module *const module) {
	uint8_t temp[] = "test";
	port_pin_set_output_level(TEST_PIN, true);
	port_pin_set_output_level(TEST_PIN, false);
	// usart_write_buffer_wait(&usart_instance, temp, sizeof(temp));
}

/**
 * Slave finishes receiving data from master
 */
void i2c_slave_read_complete_callback(struct i2c_slave_module *const module) {
	// turn i2c LED on to show activity
	if (genie_mode != GENIE_MODE_OFF) {
		port_pin_set_output_level(LEDI2C_PIN, true);
		
		uint8_t genie_dest_addr = dest_addr;
		uint8_t bytes_received = module->buffer_received;
		
		// print direction
		if (uart_show_activity) {
			if (genie_mode == GENIE_MODE_PASSTHROUGH) {
				char dirStr[29] = {0};
				snprintf(dirStr, sizeof(dirStr), "| Host > Genie > 0x%02X       ", genie_dest_addr);
				usart_write_buffer_wait(&usart_instance, dirStr, sizeof(dirStr));
			} else if (genie_mode == GENIE_MODE_REDIRECT) {
				char dirStr[29] = {0};
				snprintf(dirStr, sizeof(dirStr), "| Host > (0x%02X)Genie > 0x%02X ", genie_dest_addr, redirect_address);
				usart_write_buffer_wait(&usart_instance, dirStr, sizeof(dirStr));
				// change destination address
				genie_dest_addr = redirect_address;
			}
		} else {
			if (genie_mode == GENIE_MODE_REDIRECT) {
				genie_dest_addr = redirect_address;
			}
		}
		
		wr_packet_out.address	  = genie_dest_addr;
		wr_packet_out.data_length = module->buffer_received;
		wr_packet_out.data        = &read_buffer_in[0];
		
		i2c_master_write_packet_wait(&i2c_master_instance, &wr_packet_out);
		
		if (uart_show_activity) {
			switch(genie_mode) {
				case GENIE_MODE_PASSTHROUGH:
				case GENIE_MODE_REDIRECT:
					usart_write_buffer_wait(&usart_instance, "| ", sizeof("| "));
					uint8_t i = 0;
					while(read_buffer_in[i] != 0) {
						uint8_t tempStr[4] = {0};
						snprintf(tempStr, sizeof(tempStr), "%02X ", read_buffer_in[i]);
						usart_write_buffer_wait(&usart_instance, tempStr, sizeof(tempStr));
						i++;
					}
					usart_write_buffer_wait(&usart_instance, "\n", sizeof("\n"));
					break;

				default:
					break;
			}
		}
		
		// toggle LED to show activity
		port_pin_set_output_level(LEDI2C_PIN, false);
		port_pin_set_output_level(TEST_PIN, false);
	}
}

void i2c_master_write_complete_callback(struct i2c_master_module *const module)
{
	/* Initiate new packet read */
	uint8_t temp[] = "test3";
	usart_write_buffer_wait(&usart_instance, temp, sizeof(temp));
	i2c_master_read_packet_job(&i2c_master_instance,&rd_packet_out);
}

void i2c_master_read_complete_callback(struct i2c_master_module *const module)
{
	/* Initiate new packet read */
	uint8_t temp[] = "test4\n";
	usart_write_buffer_wait(&usart_instance, temp, sizeof(temp));
	// i2c_master_write_packet_job(&i2c_master_instance,&rd_packet_out);
	uint16_t buffer[50] = { 0 };
	uint16_t *pos = buffer;
	pos += sprintf(pos, "length: %d data: ", rd_packet_out.data_length);	
	for (uint8_t i=0; i<rd_packet_out.data_length; i++) {
		pos += sprintf(pos, "%c", rd_packet_out.data[i]);
	}
	sprintf(pos, "\n");
	usart_write_buffer_wait(&usart_instance, buffer, sizeof(buffer));
}

void usart_read_callback(struct usart_module *const usart_module)
{
	cmd_buffer[cmdLen] = rx_buffer[0];
	
	char send_error[] = "|                           | [x] unrecognized command!\n";

	if (uart_menu == UART_MENU_MAIN) {
		switch(cmd_buffer[0]) {
			case 'h':
			case 'H':
				; // print commands
				uint8_t send_main_menu[] = "|****************************\n\
|   == Commands ==\n\
|    h - help\n\
|    m - set mode\n\
|****************************\n";
				usart_write_buffer_wait(&usart_instance, send_main_menu, sizeof(send_main_menu));
				break;

			case 'm':
			case 'M':
				// quiet UART output
				uart_show_activity = false;
				uart_menu = UART_MENU_MODE;
				uint8_t send_mode_menu[] = "|****************************\n\
|   == Set Mode ==\n\
|    0 - off\n\
|    1 - passthrough\n\
|    2 - redirect\n\
|****************************\n";
				usart_write_buffer_wait(&usart_instance, send_mode_menu, sizeof(send_mode_menu));
				break;
			
			default:
				break;
		}
	} else if (uart_menu == UART_MENU_MODE) {
		switch(cmd_buffer[0]) {
			case '0':
				set_genie_mode(GENIE_MODE_OFF);
				uart_menu = UART_MENU_MAIN;
				// show uart activity again
				uart_show_activity = true;
				break;

			case '1':
				// set passthrough mode
				set_slave_address("0xFF"); // passthrough all
				set_genie_mode(GENIE_MODE_PASSTHROUGH);
				uart_menu = UART_MENU_MAIN;
				// show uart activity again
				uart_show_activity = true;
				break;

			case '2':
				// set redirect mode
				enable_master_usart_reader = false;
				// char userPrompt_slaveAddr[] = "Enter slave address (0xFF for any/all): ";
				// usart_write_buffer_wait(&usart_instance, userPrompt_slaveAddr, sizeof(userPrompt_slaveAddr));
				// char *userInput_slaveAddr[10] = { 0 };
				// // prompt_user(userPrompt_slaveAddr, sizeof(userPrompt_slaveAddr), userInput_slaveAddr, sizeof(userInput_slaveAddr) - 1);
				// uint8_t inputLen = wait_for_user_input(userInput_slaveAddr, 5);
				// // usart_write_buffer_wait(&usart_instance, "Original address (hex addres or all): ");
				// // set_genie_mode(GENIE_MODE_REDIRECT);
				// set_slave_address(userInput_slaveAddr);
				// uart_menu = UART_WAIT_NEW_SLAVE_ADDR;
		
				char userPrompt_slaveAddr[] = "Enter slave address (0xFF for any/all): ";
				usart_write_buffer_wait(&usart_instance, userPrompt_slaveAddr, sizeof(userPrompt_slaveAddr));
				char userInput_slaveAddr[10] = { 0 };
				// prompt_user(userPrompt_slaveAddr, sizeof(userPrompt_slaveAddr), userInput_slaveAddr, sizeof(userInput_slaveAddr) - 1);
				uint8_t inputLen = wait_for_user_input(userInput_slaveAddr, 9);
				// char* tempBuff[50] = { 0 };
				// sprintf(tempBuff, "str: %02X len: %d\n", userInput_slaveAddr[inputLen-1], strlen(userInput_slaveAddr));
				// usart_write_buffer_wait(&usart_instance, tempBuff, sizeof(tempBuff));
				usart_write_buffer_wait(&usart_instance, "\n", sizeof("\n"));
				set_slave_address(userInput_slaveAddr);

				char userPrompt_destAddr[] = "Enter destination address: ";
				usart_write_buffer_wait(&usart_instance, userPrompt_destAddr, sizeof(userPrompt_destAddr));
				char userInput_destAddr[10] = { 0 };
				inputLen = wait_for_user_input(userInput_destAddr, 9);
				usart_write_buffer_wait(&usart_instance, "\n", sizeof("\n"));
				set_destination_address(userInput_destAddr);

				enable_master_usart_reader = true;
				uart_show_activity = true;
				uart_menu = UART_MENU_MAIN;
				set_genie_mode(GENIE_MODE_REDIRECT);
				break;

			default:
				usart_write_buffer_wait(&usart_instance, send_error, sizeof(send_error));
				uart_menu = UART_MENU_MAIN;
				// show uart activity again
				uart_show_activity = true;
				break;
		}
	} else {

		cmdLen++;
		
		if ((cmdLen < maxCmdLen) && (rx_buffer[0] != 0x0A)) {
			usart_write_buffer_wait(&usart_instance,(uint8_t *)rx_buffer, 1);
			return;
		}

		usart_handler(cmd_buffer, cmdLen);

		memset(&cmd_buffer, 0, MAX_CMD_BUFFER_LENGTH);
		cmdLen = 0;
	}
}

uint8_t wait_for_user_input(char *inputBuffer, uint8_t bufferLen) {
	// disable usart read interrupt
	usart_disable_callback(&usart_instance, USART_CALLBACK_BUFFER_RECEIVED);

	// usart_write_buffer_wait(&usart_instance, "prompt: ", sizeof("prompt: "));

	uint8_t inputIndex = 0;
	uint16_t rx_data = 0;

	while (inputIndex < bufferLen) {
		if (usart_read_wait(&usart_instance, &rx_data) == STATUS_OK) {
			if (rx_data == 0x0A) {
				break;
			} else if (rx_data != 0x0D) { // ignore carriage returns
				inputBuffer[inputIndex] = rx_data;
				uint8_t *tempBuffer[1] = { (uint8_t)rx_data };
				usart_write_buffer_wait(&usart_instance, tempBuffer, 1);
				inputIndex++;
			}
		}
	}

	// re-enable usart read callback
	usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_RECEIVED);

	return inputIndex;
}

void prompt_user(char* prompt, uint8_t promptLen, char* received, uint8_t maxInputLen) {
	usart_write_buffer_wait(&usart_instance, prompt, promptLen);
	maxCmdLen = 5;
	usart_handler = &new_slave_handler;
}

void new_slave_handler(uint8_t *newSlaveAddr, uint8_t strLen) {
	const char *charPtr = (char *)newSlaveAddr;
	set_slave_address(charPtr);
	// usart_write_buffer_wait(&usart_instance, newSlaveAddr, strLen);
}

// enter new address to pose as
// [in] newAddr - hex address as ascii chars
// [out] status
uint8_t set_slave_address(char* newAddr) {

	if (strlen(newAddr) > 2) {
		// check for leading 0x
		// if so, remove it
		char prefix[3];
		strncpy(prefix, newAddr, 2);
		prefix[2] = 0;

		if (!strcmp("0x", prefix)) {
			newAddr+=2;
		} else {
			usart_write_buffer_wait(&usart_instance, "addr error\n", sizeof("addr error\n"));
			return STATUS_ERR_BAD_DATA;
		}
	}

	// check if valid address
	// if so, convert address to int
	uint16_t newAddrInt = 0;
	uint8_t numDigits = strlen(newAddr);
	if (numDigits) {
		if (numDigits <= 2) {
			for (uint8_t i=0; i<numDigits; i++) {
				newAddrInt = strtol(newAddr, NULL, 16);
				if (newAddrInt == 0) {
					// not hex value
					return STATUS_ERR_BAD_DATA;
				}
			}
		} else {
			// too many characters
			return STATUS_ERR_BAD_DATA;
		}
	} else {
		// no address
		return STATUS_ERR_BAD_DATA;
	}

	// set new i2c slave address
	char* slaveAddrSetBuffer[50] = { 0 };
	if (newAddrInt == 255) {
		// disable i2c slave port
		i2c_slave_disable(&i2c_slave_instance);
		// set slave address as range
		i2c_slave_instance.hw->I2CS.CTRLB.reg = SERCOM_I2CS_CTRLB_SMEN | I2C_SLAVE_ADDRESS_MODE_RANGE;
		// clear old i2c slave address settings
		i2c_slave_instance.hw->I2CS.ADDR.reg = ~((0xFFFF << SERCOM_I2CS_ADDR_ADDR_Pos) | (0xFFFF << SERCOM_I2CS_ADDR_ADDRMASK_Pos));
		// set new i2c slave address settings
		i2c_slave_instance.hw->I2CS.ADDR.reg |= 0xFF << SERCOM_I2CS_ADDR_ADDR_Pos | 0x00 << SERCOM_I2CS_ADDR_ADDRMASK_Pos;
		i2c_slave_enable(&i2c_slave_instance);
		configure_i2c_slave_callbacks();
		sprintf(slaveAddrSetBuffer, "|                           | [*] slave address set to: ANY/ALL\n");
		usart_write_buffer_wait(&usart_instance, slaveAddrSetBuffer, sizeof(slaveAddrSetBuffer));
	} else {
		// disable i2c slave port
		i2c_slave_disable(&i2c_slave_instance);
		// set slave address single
		i2c_slave_instance.hw->I2CS.CTRLB.reg = SERCOM_I2CS_CTRLB_SMEN | I2C_SLAVE_ADDRESS_MODE_MASK;
		// clear old i2c slave address settings
		i2c_slave_instance.hw->I2CS.ADDR.reg &= ~((0xFFFF << SERCOM_I2CS_ADDR_ADDR_Pos) | (0xFFFF << SERCOM_I2CS_ADDR_ADDRMASK_Pos));
		// set new i2c slave address settings
		i2c_slave_instance.hw->I2CS.ADDR.reg |= newAddrInt << SERCOM_I2CS_ADDR_ADDR_Pos | 0x00 << SERCOM_I2CS_ADDR_ADDRMASK_Pos;
		i2c_slave_enable(&i2c_slave_instance);
		configure_i2c_slave_callbacks();
		char* slaveAddrSetBuffer[50] = { 0 };
		sprintf(slaveAddrSetBuffer, "|                           | [*] slave address set to: 0x%02X\n", newAddrInt);
		usart_write_buffer_wait(&usart_instance, slaveAddrSetBuffer, sizeof(slaveAddrSetBuffer));
	}

	return STATUS_OK;
}

// enter new address to redirect packets to
// [in] newAddr - hex address as ascii chars
// [out] status
uint8_t set_destination_address(char* newAddr) {

	if (strlen(newAddr) > 2) {
		// check for leading 0x
		// if so, remove it
		char prefix[3];
		strncpy(prefix, newAddr, 2);
		prefix[2] = 0;

		if (!strcmp("0x", prefix)) {
			newAddr+=2;
		} else {
			usart_write_buffer_wait(&usart_instance, "addr error\n", sizeof("addr error\n"));
			return STATUS_ERR_BAD_DATA;
		}
	}

	// check if valid address
	// if so, convert address to int
	uint16_t newAddrInt = 0;
	uint8_t numDigits = strlen(newAddr);
	if (numDigits) {
		if (numDigits <= 2) {
			for (uint8_t i=0; i<numDigits; i++) {
				newAddrInt = strtol(newAddr, NULL, 16);
				if (newAddrInt == 0) {
					// not hex value
					return STATUS_ERR_BAD_DATA;
				}
			}
		} else {
			// too many characters
			return STATUS_ERR_BAD_DATA;
		}
	} else {
		// no address
		return STATUS_ERR_BAD_DATA;
	}

	// set new i2c destination address
	redirect_address = newAddrInt;
	char* destAddrSetBuffer[50] = { 0 };
	sprintf(destAddrSetBuffer, "|                           | [*] destination address set to: 0x%02X\n", newAddrInt);
	usart_write_buffer_wait(&usart_instance, destAddrSetBuffer, sizeof(destAddrSetBuffer));

	return STATUS_OK;
}

void usart_write_callback(struct usart_module *const usart_module)
{
	
}

void configure_i2c_slave(void)
{
	/* Initialize config structure and module instance */
	struct i2c_slave_config config_i2c_slave;
	i2c_slave_get_config_defaults(&config_i2c_slave);

	/* Change address and address_mode */
	// config_i2c_slave.address      = 0x78;
	// config_i2c_slave.address_mode = I2C_SLAVE_ADDRESS_MODE_MASK;
	config_i2c_slave.address	  = 0xFF; // upper i2c address range
	config_i2c_slave.address_mask = 0x00; // lower i2c address range
	config_i2c_slave.address_mode = I2C_SLAVE_ADDRESS_MODE_RANGE;
	config_i2c_slave.pinmux_pad0  = PINMUX_PA22C_SERCOM3_PAD0;
	config_i2c_slave.pinmux_pad1  = PINMUX_PA23C_SERCOM3_PAD1;

	/* Initialize and enable device with config */
	i2c_slave_init(&i2c_slave_instance, SERCOM3, &config_i2c_slave);

	i2c_slave_enable(&i2c_slave_instance);
}

void configure_i2c_slave_callbacks(void)
{
	/* Register and enable callback functions */
	i2c_slave_register_callback(&i2c_slave_instance, i2c_read_request_callback,
			I2C_SLAVE_CALLBACK_READ_REQUEST);
	i2c_slave_enable_callback(&i2c_slave_instance,
			I2C_SLAVE_CALLBACK_READ_REQUEST);
			
	i2c_slave_register_callback(&i2c_slave_instance, i2c_slave_read_complete_callback,
			I2C_SLAVE_CALLBACK_READ_COMPLETE);
	i2c_slave_enable_callback(&i2c_slave_instance,
			I2C_SLAVE_CALLBACK_READ_COMPLETE);

	i2c_slave_register_callback(&i2c_slave_instance, i2c_write_request_callback,
			I2C_SLAVE_CALLBACK_WRITE_REQUEST);
	i2c_slave_enable_callback(&i2c_slave_instance,
			I2C_SLAVE_CALLBACK_WRITE_REQUEST);
			
	i2c_slave_register_callback(&i2c_slave_instance, i2c_slave_write_complete_callback,
			I2C_SLAVE_CALLBACK_WRITE_COMPLETE);
	i2c_slave_enable_callback(&i2c_slave_instance,
			I2C_SLAVE_CALLBACK_WRITE_COMPLETE);
}

void configure_usart(void)
{
	struct usart_config config_usart;
	usart_get_config_defaults(&config_usart);

	config_usart.baudrate    = 9600;
	config_usart.mux_setting = EXT1_UART_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = EXT1_UART_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = EXT1_UART_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = EXT1_UART_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = EXT1_UART_SERCOM_PINMUX_PAD3;

	while (usart_init(&usart_instance,
	EXT1_UART_MODULE, &config_usart) != STATUS_OK) {
	}

	usart_enable(&usart_instance);
}

void configure_i2c_master(void)
{
	/* Initialize config structure and software module */
	struct i2c_master_config config_i2c_master;
	i2c_master_get_config_defaults(&config_i2c_master);

	/* Change buffer timeout to something longer */
	config_i2c_master.buffer_timeout = 65535;
	config_i2c_master.pinmux_pad0    = PINMUX_PA16C_SERCOM1_PAD0;
	config_i2c_master.pinmux_pad1    = PINMUX_PA17C_SERCOM1_PAD1;

	/* Initialize and enable device with config */
	while(i2c_master_init(&i2c_master_instance, SERCOM1, &config_i2c_master) != STATUS_OK);

	i2c_master_enable(&i2c_master_instance);
}

void configure_i2c_master_callbacks(void)
{
	/* Register callback function. */
	i2c_master_register_callback(&i2c_master_instance, i2c_master_write_complete_callback, I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_register_callback(&i2c_master_instance, i2c_master_read_complete_callback, I2C_MASTER_CALLBACK_READ_COMPLETE);

	i2c_master_enable_callback(&i2c_master_instance, I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance, I2C_MASTER_CALLBACK_READ_COMPLETE);
}

void configure_usart_callbacks(void)
{
	usart_register_callback(&usart_instance,
	usart_write_callback, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_register_callback(&usart_instance,
	usart_read_callback, USART_CALLBACK_BUFFER_RECEIVED);

	usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_RECEIVED);
}

/** Configure LED0, turn it off*/
static void config_led(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LEDI2C_PIN, &pin_conf);
	port_pin_set_config(TEST_PIN, &pin_conf);
	port_pin_set_output_level(LEDI2C_PIN, false);
	port_pin_set_output_level(TEST_PIN, false);
}

void set_genie_mode(uint8_t new_mode) {
	switch(new_mode) {
		case GENIE_MODE_PASSTHROUGH:
		; // print new mode
		uint8_t printStr1[] = "|                           | [*] mode set to: passthrough\n";
		usart_write_buffer_wait(&usart_instance, printStr1, sizeof(printStr1));
		break;

		case GENIE_MODE_INJECT:
		; // print new mode
		uint8_t printStr2[] = "|                           | [*] mode set to: inject\n";
		usart_write_buffer_wait(&usart_instance, printStr2, sizeof(printStr2));
		break;

		case GENIE_MODE_REDIRECT:
		; // print new mode
		uint8_t printStr3[] = "|                           | [*] mode set to: redirect\n";
		usart_write_buffer_wait(&usart_instance, printStr3, sizeof(printStr3));
		break;

		default:
		; // default/off
		uint8_t printStr0[] = "|                           | [*] mode set to: off\n";
		usart_write_buffer_wait(&usart_instance, printStr0, sizeof(printStr0));
		break;
	}

	genie_mode = new_mode;
}

int main(void)
{
	system_init();

	configure_i2c_slave();
	configure_i2c_slave_callbacks();
	
	configure_i2c_master();
	configure_i2c_master_callbacks();
	
	configure_usart();
	configure_usart_callbacks();
	
	config_led();

	system_interrupt_enable_global();

	uint8_t aboutStr[] = "\n********************\n   SAO GENIE v0.1\n     mediumrehr\n********************\n\n";
	usart_write_buffer_wait(&usart_instance, aboutStr, sizeof(aboutStr));

	uint8_t headerStr[] = "/---------------------------|-------------------------------------------------\\\n|         Direction         |  Data                                           |\n|---------------------------|-------------------------------------------------|\n";
	usart_write_buffer_wait(&usart_instance, headerStr, sizeof(headerStr));

	set_genie_mode(GENIE_MODE_PASSTHROUGH);

	uint16_t *testBuffer[10] = { 0 };
	uart_show_activity = true;

	while (true) {
		/* Infinite loop while waiting for I2C master interaction */
		if (enable_master_usart_reader) {
			usart_read_buffer_job(&usart_instance,
				(uint8_t *)rx_buffer, MAX_RX_BUFFER_LENGTH);
		}
		// uint8_t inputLen = wait_for_user_input(testBuffer, 9);

		// usart_write_buffer_wait(&usart_instance, testBuffer, sizeof(inputLen));
	}
	
}
