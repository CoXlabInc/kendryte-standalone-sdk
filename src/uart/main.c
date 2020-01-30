/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include "fpioa.h"
#include <string.h>
#include "uart.h"
#include "gpiohs.h"
#include "sysctl.h"
#include <stdlib.h>

#define RECV_LENTH  2
#define SEND_LENTH 16

#define offset_len 4
#define length_len 4

#define CLOSLIGHT   0x55555555
#define OPENLIGHT   0xAAAAAAAA

#define UART_NUM    UART_DEVICE_2
// UART_DEVICE_3 for debug
// UART_DEVICE_2

int release_cmd(char *cmd)
{
    switch(*((int *)cmd)){
        case CLOSLIGHT:
        gpiohs_set_pin(3, GPIO_PV_LOW);
        break;
        case OPENLIGHT:
        gpiohs_set_pin(3, GPIO_PV_HIGH);
        break;
    }
    return 0;
}


void io_mux_init(void)
{
    fpioa_set_function(6, FUNC_UART1_RX + UART_NUM * 2);
    fpioa_set_function(7, FUNC_UART1_TX + UART_NUM * 2);
    fpioa_set_function(24, FUNC_GPIOHS3);
}

static void io_set_power(void)
{
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V33);
}

int main()
{
    io_mux_init();
    io_set_power();
    plic_init();
    sysctl_enable_irq();

    gpiohs_set_drive_mode(3, GPIO_DM_OUTPUT);
    gpio_pin_value_t value = GPIO_PV_HIGH;
    gpiohs_set_pin(3, value);

    uart_init(UART_NUM);
    uart_configure(UART_NUM, 115200, 8, UART_STOP_1, UART_PARITY_NONE);

    char *hel = {"hello world!\n"};
    uart_send_data(UART_NUM, hel, strlen(hel));
    char recv = 0;
    char send = 0;

    int rec_flag = 0;
    int img_flag = 0;
    char cmd[8];
    char length[2];
    char offset[4];
    char file_len[4];
    char fn_size;
    char *rec_filename = "";
    char format = 0;
    int i = 0;
    int j = 0;
    char* Snapanswer = {"1122334455667788\n"};
    
    char checksum;
    char calcchecksum;
    char msg_type; 
    
    while (1)
    {
        while(uart_receive_data(UART_NUM, &recv, 1))
        {
            switch(rec_flag)
            {
                case 0: //type
                    msg_type = recv;
                    (msg_type == 0x00) || (msg_type == 0x02) ? (rec_flag = 1) : rec_flag;
                    break;
                case 1: //Length
                    length[i++] = recv;
                    // recv == 0xAA ? (rec_flag = 2) : (rec_flag = 0);
                    if (i >= RECV_LENTH)
                    {
                        i = 0;
                        if (msg_type == 0x00){
                            rec_flag = 2;
                        }
                        else{
                            rec_flag = 4;
                        }
                    }
                    break;
                case 2: //format
                    format = recv;
                    rec_flag = 3;
                    break;
                case 3:
                    checksum = recv;
                    calcchecksum = length[0] + length[1] + format;
                    if (calcchecksum == checksum){
                        printf("file successfully received\r\n");
                        uint8_t payload[100];

                        uint8_t sn_type = 0x01; //Type
                        payload[0] = sn_type; 

                        uint8_t sn_length[2] = { 0x12, 0x34 }; // Length
                        payload[1] = sn_length[1];
                        printf("payload[1] : %x\r\n",payload[1]);
                        payload[2] = sn_length[0];
                        for (int i = 3; i< 7 ; i++){
                            payload[i] = 0x02; //size
                        }
                        char* filename = "1234.jpg"; //filename
                        uint8_t sn_fnsize = (uint8_t)strlen(filename); //filename size
                        payload[7] = sn_fnsize;
                        printf("sn_fnsize : %d\r\n", sn_fnsize);
                        for (int i = 8; i<16 ; i++){
                            payload[i] = filename[i-8];
                        }
                        //checksum
                        uint8_t checksum = 0;
                        for (int i = 0 ; i < 16; i++){
                            checksum += payload[i];
                        }
                        payload[16] = checksum;
                        uart_send_data(UART_NUM,payload, 17);
                    }
                    rec_flag = 0;
                    break;
                case 4:
                    if (img_flag == 0){
                        offset[j++] = recv;
                        if (j >= offset_len){
                            j = 0;
                            img_flag = 1;
                        }
                        
                    }
                    else if (img_flag == 1){
                        file_len[j++] = recv;
                        if (j >= length_len){
                            j = 0;
                            img_flag = 2;
                        }
                    }
                    else if (img_flag == 2){
                        fn_size = recv;
                        img_flag = 3;
                        rec_filename = malloc(fn_size);
                    }
                    else if (img_flag == 3){
                        rec_filename[j++] = recv;
                        if (j >= fn_size){
                            j = 0;
                            img_flag = 4;
                            printf("[debug] rec_filename down ok\r\n");
                        }
                        
                    }
                    else{
                        checksum = recv;

                        uint8_t kayload[100];
                        uint8_t chunk_type = 0x03;
                        kayload[0] = chunk_type;

                        uint8_t kn_length[2] = { 0x00, 0x01 }; // Length
                        kayload[1] = kn_length[1];
                        kayload[2] = kn_length[0];

                        uint8_t img_chunk[10] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x08, 0x09 };
                        for (int i = 9, j = 3 ; i >= 0 ; i--, j++ ){
                            kayload[j] = img_chunk[i];
                        }
                        //checksum
                        uint8_t khecksum = 0;
                        for (int i = 0 ; i < 13; i++){
                            khecksum += kayload[i];
                        }
                        kayload[13] = khecksum;
                        uart_send_data(UART_NUM, kayload, 14);
                        
                        for (int i = 0 ; i < 14; i++){
                            printf("payload[%d] = %x\r\n",i,kayload[i]);
                        }

                        free(rec_filename);
                        rec_flag = 0;
                        img_flag = 0;
                        printf(" -------------finish line----------\r\n");
                    }
                    break;
            }
        }
        
    }
    while(1);
}

