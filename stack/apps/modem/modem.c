/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2015 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This is an example application where the stack is running on an a standalone MCU,
// typically used in combination with another MCU where the main application (for instance sensor reading)
// in running. The application accesses the stack using the serial modem interface.

#include "hwleds.h"
#include "hwsystem.h"
#include "debug.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>

#include "d7ap.h"
#include "alp_layer.h"
#include "d7ap_fs.h"
#include "log.h"
#include "dae.h"
#include "platform_defs.h"
#include "modem_interface.h"
#include "platform.h"
#include "hwblockdevice.h"

#include "hwradio.h"

#define ENGINEERING_FILE_ID      0x43
#define ACTION_FILE_ID           0x44
#define INTERFACE_FILE_ID        0x45
#define ENGINEERING_FILE_SIZE    20
#define written_data_size        3

#define DEFAULT_EIRP 0 // TODO
#define MAX_EIRP 0 // TODO

typedef enum {
  MODULATION_CW,
  MODULATION_GFSK,
} modulation_t;

#define HI_RATE_CHANNEL_COUNT 32
#define NORMAL_RATE_CHANNEL_COUNT 32
#define LO_RATE_CHANNEL_COUNT 280

static hw_tx_cfg_t tx_cfg;
static hw_rx_cfg_t rx_cfg;
static uint16_t current_channel_indexes_index = 13;
static modulation_t current_modulation = MODULATION_GFSK; // MODULATION_CW; 
static phy_channel_band_t current_channel_band = PHY_BAND_868;
static phy_channel_class_t current_channel_class = PHY_CLASS_HI_RATE;
static uint16_t channel_index = 0;
static uint16_t channel_count = LO_RATE_CHANNEL_COUNT;
static eirp_t current_eirp_level = DEFAULT_EIRP;
static bool data_predifined = true; // if false, it will send numbers going from 0 to 255
static uint8_t time_period = 5;

#include "hwradio.h"

#define ENGINEERING_FILE_ID      0x43
#define ACTION_FILE_ID           0x44
#define INTERFACE_FILE_ID        0x45
#define ENGINEERING_FILE_SIZE    20
#define written_data_size        3

#define DEFAULT_EIRP 0 // TODO
#define MAX_EIRP 0 // TODO

typedef enum {
  MODULATION_CW,
  MODULATION_GFSK,
} modulation_t;

#define HI_RATE_CHANNEL_COUNT 32
#define NORMAL_RATE_CHANNEL_COUNT 32
#define LO_RATE_CHANNEL_COUNT 280

static hw_tx_cfg_t tx_cfg;
static uint16_t current_channel_indexes_index = 13;
static modulation_t current_modulation = MODULATION_GFSK; // MODULATION_CW; 
static phy_channel_band_t current_channel_band = PHY_BAND_868;
static phy_channel_class_t current_channel_class = PHY_CLASS_HI_RATE;
static uint16_t channel_indexes[LO_RATE_CHANNEL_COUNT] = { 0 }; // reallocated later depending on band/class
static uint16_t channel_count = LO_RATE_CHANNEL_COUNT;
static uint8_t current_eirp_level = DEFAULT_EIRP;
static uint8_t time_period = 5;

// This example application contains a modem which can be used from another MCU through
// the serial interface
void start_tx(){
    start_hw_radio_continuous_tx(time_period);
}

void continuous_tx(uint8_t* data){
    interpret_data(data);

    tx_cfg.channel_id.channel_header.ch_coding = PHY_CODING_PN9;
    tx_cfg.channel_id.channel_header.ch_class = current_channel_class;
    tx_cfg.channel_id.channel_header.ch_freq_band = current_channel_band;
    tx_cfg.channel_id.center_freq_index = channel_index;
    tx_cfg.eirp = current_eirp_level;

    /* Configure */
    hw_radio_continuous_tx(&tx_cfg, current_modulation == MODULATION_CW);

    /* start the radio */
    log_print_string("sending \n");
    sched_register_task(&start_tx);
    //give it time to answer through uart
    timer_post_task_delay(&start_tx, 500);
}

// packet callbacks only here to make hwradio_init() happy, not used
hw_radio_packet_t* alloc_packet_callback(uint8_t length) {
  assert(false);
}

void release_packet_callback(hw_radio_packet_t* p) {
  assert(false);
}

/* if engineering file gets changed */
void engin_file_change_callback(uint8_t file_id){
    uint8_t data[ENGINEERING_FILE_SIZE];

    fs_read_file(ENGINEERING_FILE_ID,0,data,20);
    
    if(data[0] & 0x01){ 
        log_print_string("tx");
        continuous_tx(data);
    } else {
        log_print_string("rx");
        continuous_rx(data);
    }
}

void start(){
    fs_file_header_t file_header = (fs_file_header_t){
        .file_properties.action_protocol_enabled = 0,
        //.file_properties.action_condition = ALP_ACT_COND_WRITE,
        .file_properties.storage_class = FS_STORAGE_VOLATILE,
        .file_permissions = 0, // TODO
        .alp_cmd_file_id = ACTION_FILE_ID,
        .interface_file_id = INTERFACE_FILE_ID,
        .length = ENGINEERING_FILE_SIZE
    };

    fs_init_file(ENGINEERING_FILE_ID, &file_header, 1);

    fs_register_file_modified_callback(ENGINEERING_FILE_ID, &engin_file_change_callback);
}

void bootstrap()
{
    log_print_string("Device booted\n");
#ifdef PLATFORM_USE_MODEM_INTERRUPT_LINES
    modem_interface_init(PLATFORM_MODEM_INTERFACE_UART, PLATFORM_MODEM_INTERFACE_BAUDRATE, MODEM2MCU_INT_PIN, MCU2MODEM_INT_PIN);
#else
    modem_interface_init(PLATFORM_MODEM_INTERFACE_UART, PLATFORM_MODEM_INTERFACE_BAUDRATE, (pin_id_t) 0, (pin_id_t) 0);
#endif


    blockdevice_init(d7_systemfiles_blockdevice);
    d7ap_init(d7_systemfiles_blockdevice);
    alp_layer_init(NULL, true);

    uint8_t uid[8];
    d7ap_fs_read_uid(uid);
    log_print_string("UID %02X%02X%02X%02X%02X%02X%02X%02X\n", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
}

