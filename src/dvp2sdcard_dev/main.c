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
#include <string.h>
#include "board_config.h"
#include "dvp.h"
#include "ff.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "iomem.h"
#include "lcd.h"
#include "nt35310.h"
#include "ov2640.h"
#include "ov5640.h"
#include "plic.h"
#include "rgb2bmp.h"
#include "rtc.h" //for rtc set
#include "sdcard.h"
#include "sysctl.h"
#include "tiny_jpeg.h"
#include "uart.h"
#include "uarths.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RECV_LENTH 2
#define SEND_LENTH 16

#define offset_len 4
#define length_len 2

#define CLOSLIGHT 0x55555555
#define OPENLIGHT 0xAAAAAAAA
#define UART_NUM UART_DEVICE_2

// UART_DEVICE_3 for debug
// UART_DEVICE_2

/* SPI and DMAC usage
 *
 * SPI0 - LCD
 * SPI1 - SD card
 * SPI2 - unused
 * SPI3 - Flash
 *
 * DMAC Channel 0 - LCD
 * DMAC Channel 1 - SD card
 *
 */

#define KEY_GPIONUM 0

uint32_t *g_lcd_gram0;
uint32_t *g_lcd_gram1;

volatile uint8_t g_dvp_finish_flag;
volatile uint8_t g_ram_mux;
volatile uint8_t g_save_flag = 0;

void irq_key(void *gp)
{
    g_save_flag = 1;
}

static int on_irq_dvp(void *ctx)
{
    if(dvp_get_interrupt(DVP_STS_FRAME_FINISH))
    {
        /* switch gram */
        dvp_set_display_addr(g_ram_mux ? (uint32_t)g_lcd_gram0 : (uint32_t)g_lcd_gram1);

        dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
        g_dvp_finish_flag = 1;
    } else
    {
        if(g_dvp_finish_flag == 0)
            dvp_start_convert();
        dvp_clear_interrupt(DVP_STS_FRAME_START);
    }

    return 0;
}

static void io_mux_init(void)
{

#if BOARD_LICHEEDAN
    /* SD card */
    fpioa_set_function(27, FUNC_SPI1_SCLK);
    fpioa_set_function(28, FUNC_SPI1_D0);
    fpioa_set_function(26, FUNC_SPI1_D1);
    fpioa_set_function(29, FUNC_GPIOHS7);

    /* Init DVP IO map and function settings */
    
    fpioa_set_function(42, FUNC_CMOS_RST);
    fpioa_set_function(44, FUNC_CMOS_PWDN);
    fpioa_set_function(46, FUNC_CMOS_XCLK);
    fpioa_set_function(43, FUNC_CMOS_VSYNC);
    fpioa_set_function(45, FUNC_CMOS_HREF);
    fpioa_set_function(47, FUNC_CMOS_PCLK);
    fpioa_set_function(41, FUNC_SCCB_SCLK);
    fpioa_set_function(40, FUNC_SCCB_SDA);

    /* Init SPI IO map and function settings */
    // fpioa_set_function(38, FUNC_GPIOHS0 + DCX_GPIONUM);
    // fpioa_set_function(36, FUNC_SPI0_SS3);
    // fpioa_set_function(39, FUNC_SPI0_SCLK);
    // fpioa_set_function(37, FUNC_GPIOHS0 + RST_GPIONUM);
    
    // sysctl_set_spi0_dvp_data(1);
    fpioa_set_function(16, FUNC_GPIOHS0 + KEY_GPIONUM);

    /* init io_mux for UART communication*/
    fpioa_set_function(6, FUNC_UART1_RX + UART_NUM * 2);
    fpioa_set_function(7, FUNC_UART1_TX + UART_NUM * 2);
    // fpioa_set_function(24, FUNC_GPIOHS3); // unknown
#else
    /* SD card */
    fpioa_set_function(29, FUNC_SPI1_SCLK);
    fpioa_set_function(30, FUNC_SPI1_D0);
    fpioa_set_function(31, FUNC_SPI1_D1);
    fpioa_set_function(32, FUNC_GPIOHS7);

    /* Init DVP IO map and function settings */
    fpioa_set_function(11, FUNC_CMOS_RST);
    fpioa_set_function(13, FUNC_CMOS_PWDN);
    fpioa_set_function(14, FUNC_CMOS_XCLK);
    fpioa_set_function(12, FUNC_CMOS_VSYNC);
    fpioa_set_function(17, FUNC_CMOS_HREF);
    fpioa_set_function(15, FUNC_CMOS_PCLK);
    fpioa_set_function(10, FUNC_SCCB_SCLK);
    fpioa_set_function(9, FUNC_SCCB_SDA);

    /* Init SPI IO map and function settings */
    // fpioa_set_function(8, FUNC_GPIOHS0 + DCX_GPIONUM);
    // fpioa_set_function(6, FUNC_SPI0_SS3);
    // fpioa_set_function(7, FUNC_SPI0_SCLK);

    // sysctl_set_spi0_dvp_data(1);

    fpioa_set_function(26, FUNC_GPIOHS0 + KEY_GPIONUM);
#endif
}

static void io_set_power(void)
{
#if BOARD_LICHEEDAN
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V33);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V33);

    /* Set uart pin to 3.3v*/
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V33);

#else
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);
#endif
}

int main(void)
{
    printf("dmac status[0-1] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    rtc_init();
    // RTC INIT --------------------- for test
    rtc_timer_set(2019, 12, 20, 9, 0, 0);
    // UART INIt ----------------------
    gpiohs_set_drive_mode(3, GPIO_DM_OUTPUT);
    gpio_pin_value_t value = GPIO_PV_HIGH;
    gpiohs_set_pin(3, value);

    char recv = 0;

    // flag for byte matching
    int rec_flag = 0;
    int img_flag = 0;

    // public variable
    char length[2];
    char msg_type = '\0';
    // 2-1 Snap
    char format = 0;
    // 2-3 getImage
    uint8_t offset[4];
    char file_len[2];
    char fn_size = '\0';
    char rec_filename[20];

    int i = 0;
    int j = 0;

    char checksum;
    char calcchecksum;

    int snap_size = 0;

    int img_size = 0;
    char *img_data = NULL;
    uint32_t bytesread;

    // 2-5 setRTC
    uint8_t ymdhms[8]; //y(2) m(1) d(1) h(1) m(1) s(1) + checksum
    int rtcoffset = 0;

    // -----------------------------------------
    FATFS sdcard_fs;

    /* Set CPU and dvp clk */
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    printf("dmac status[0-2] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    uarths_init();  

    io_mux_init();
    io_set_power();
    plic_init();

    gpiohs_set_drive_mode(KEY_GPIONUM, GPIO_DM_INPUT);
    gpiohs_set_pin_edge(KEY_GPIONUM, GPIO_PE_FALLING);
    gpiohs_set_irq(KEY_GPIONUM, 2, irq_key);

    /* LCD init */
    // printf("LCD init\r\n");
    // lcd_init();
#if BOARD_LICHEEDAN
#if OV5640
    lcd_set_direction(DIR_YX_LRDU);
#else
    // lcd_set_direction(DIR_YX_LRDU);
#endif
#else
#if OV5640
    lcd_set_direction(DIR_YX_LRUD);
#else
    lcd_set_direction(DIR_YX_LRUD);
#endif
#endif

    // lcd_clear(BLACK);

    g_lcd_gram0 = (uint32_t *)iomem_malloc(320 * 240 * 2);
    g_lcd_gram1 = (uint32_t *)iomem_malloc(320 * 240 * 2);
    /* DVP init */
    printf("dmac status[0-3] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    printf("DVP init\r\n");
#if OV5640
    dvp_init(16);
    dvp_set_xclk_rate(50000000);
    dvp_enable_burst();
    dvp_set_output_enable(0, 0);
    dvp_set_output_enable(1, 1);
    dvp_set_image_format(DVP_CFG_RGB_FORMAT);
    dvp_set_image_size(320, 240);
    ov5640_init();
#else
    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    dvp_set_output_enable(0, 0);
    dvp_set_output_enable(1, 1);
    dvp_set_image_format(DVP_CFG_RGB_FORMAT);
    dvp_set_image_size(320, 240);
    ov2640_init();
#endif

    dvp_set_display_addr((uint32_t)g_lcd_gram0);
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
    dvp_disable_auto();
    printf("dmac status[0-4] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    /* DVP interrupt config */
    printf("DVP interrupt config\r\n");
    printf("dmac status[0-5] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);
    printf("dmac status[0-6] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    /* enable global interrupt */
    sysctl_enable_irq();
    printf("dmac status[0-7] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    /* SD card init */
    if(sd_init())
    {
        printf("Fail to init SD card\r\n");
        return -1;
    }

    /* mount file system to SD card */
    if(f_mount(&sdcard_fs, _T("0:"), 1))
    {
        printf("Fail to mount file system\r\n");
        return -1;
    }

    /* system start */
    printf("system start\r\n");
    g_ram_mux = 0;
    g_dvp_finish_flag = 0;
    
    dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);
    
    printf("dvp interrupt config ok\r\n");

    uart_init(UART_NUM);
    uart_configure(UART_NUM, 115200, 8, UART_STOP_1, UART_PARITY_NONE);

    printf("UART init config ok\r\n");
    printf("dmac status[0] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
    uint16_t jpgnum = 0;
    while(1)
    {
        int counting = 0;
        /* ai cal finish*/
        printf("dmac status[1] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
        while(g_dvp_finish_flag == 0){
            printf("finish flag == 0  counting = %d\r\n",counting++);
        }
        printf("dmac status[2] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
        g_ram_mux ^= 0x01;
        g_dvp_finish_flag = 0;

        // printf("dvp finish flag 1\r\n");
        // printf("dvp finish flag 2\r\n");
        while(uart_receive_data(UART_NUM, &recv, 1))
        {
            printf("receive data anything\r\n");
            switch(rec_flag)
            {
                case 0: //type
                    msg_type = recv;
                    (msg_type == 0x00) || (msg_type == 0x02) || (msg_type == 0x04) ? (rec_flag = 1) : rec_flag;
                    break;
                case 1: //Length
                    length[i++] = recv;
                    if(i >= RECV_LENTH)
                    {
                        i = 0;
                        if(msg_type == 0x00)
                        {
                            rec_flag = 2;
                        } else if(msg_type == 0x02)
                        {
                            rec_flag = 4;
                        } else
                        {
                            rec_flag = 5;
                        }
                    }
                    break;
                case 2: // format [ case : msg_type == 0x00 ]
                    format = recv;
                    rec_flag = 3;
                    break;
                case 3: // checksum [ case : msg_type == 0x00 ]
                    checksum = recv;
                    calcchecksum = length[0] + length[1] + format;
                    if((calcchecksum == checksum) && (length[0] != 0x00))
                    {
                        calcchecksum = 0;
                        printf("request successfully received\r\n");
                        g_save_flag = 1;
                        int width, height, num_components;
                        width = 320;
                        height = 240;
                        num_components = 3;
                        char filename2[15];
                        char fileout[15];
                        int quality = 1;
                        if((format == 0x01) | (format == 0x02) | (format == 0x03))
                        {
                            quality = format;
                        }
                        if(g_save_flag) // take a pic & convert & save transaction
                        {

                            sprintf(filename2, "%04X.BMP", jpgnum++);
                            sprintf(fileout, "%04X.jpg", (jpgnum - 1));
                            //filename , fileout definition

                            //rgb565tobmp [16bit bitmap] rgb888tobmp [24bit bitmap]
                            printf("pass1, g_ram_mux : %d \r\n",g_ram_mux);
                            printf("dmac status[pack] = %d\r\n",dmac_is_idle(DMAC_CHANNEL1));
                            rgb888tobmp((uint8_t *)(g_ram_mux ? g_lcd_gram0 : g_lcd_gram1), 320, 240, _T(filename2));
                            printf("pass2 \r\n");
                            unsigned char *data = stbi_load(filename2, &width, &height, &num_components, 0);
                            //load data from bitmap file
                            if(!data)
                            {
                                puts("Could not find file");
                            }
                            //convert bitmap data to jpeg file
                            int encoderesult = tje_encode_to_file_at_quality(fileout, quality, width, height, num_components, data);
                            
                            if(!encoderesult)
                            {
                                fprintf(stderr, "Could not write JPEG\n");
                            } else
                            {
                                printf("BMP , JPG Success! filename : %s , %s\r\n", filename2, fileout);
                                if(format == 0x00)
                                {
                                    f_unlink(fileout);
                                    snap_size = 230454;
                                } else
                                {
                                    f_unlink(filename2);
                                    snap_size = encoderesult; //save size of pic in snap_size
                                }
                            }
                            free(data); // after data convert, free memory
                            g_save_flag = 0;
                        }

                        uint8_t payload[100]; // buffer for send snap_finished

                        uint8_t sn_type = 0x01; //Type  0x01 fix
                        payload[0] = sn_type;

                        //save LSB first order
                        payload[6] = 0xff & (snap_size >> 24);
                        payload[5] = 0xff & (snap_size >> 16);
                        payload[4] = 0xff & (snap_size >> 8);
                        payload[3] = 0xff & snap_size;

                        uint16_t sn_fnsize = strlen(fileout); //filename size
                        payload[7] = sn_fnsize;
                        if(format == 0x00)
                        {
                            sn_fnsize = strlen(filename2);
                            for(int i = 8; i < (8 + sn_fnsize); i++)
                            {
                                payload[i] = filename2[i - 8];
                            }
                        } else
                        {
                            for(int i = 8; i < (8 + sn_fnsize); i++)
                            {
                                payload[i] = fileout[i - 8];
                            }
                        }

                        uint16_t sn_length = (0x0004 + 0x0001 + sn_fnsize);

                        // Length
                        payload[1] = sn_length & 0xff;
                        payload[2] = (sn_length >> 8) & 0xff;

                        //checksum
                        uint8_t snap_checksum = 0;
                        for(int i = 0; i < (8 + sn_fnsize); i++)
                        {
                            snap_checksum += payload[i];
                        }
                        payload[8 + sn_fnsize] = snap_checksum;
                        uart_send_data(UART_NUM, payload, (8 + sn_fnsize + 1));
                    } else
                    {
                        printf("[0x00] Checksum or Length err recv[0x%x] <-> calc[0x%x]\r\n", checksum, calcchecksum);
                        printf("[0x00] [0x%x] [0x%x] [0x%x] [0x%x] \r\n", msg_type, length[0], length[1], format);
                        if(length[0] == 0x00)
                        {
                            char errmsg[15] = "LENGTH[0]_ERROR";
                            uart_send_data(UART_NUM, errmsg, 15);
                        } else
                        {
                            char errmsg[14] = "CHECKSUM_ERROR";
                            uart_send_data(UART_NUM, errmsg, 14);
                        }
                    }
                    rec_flag = 0;
                    printf("send OK! \r\n");
                    break;
                case 4: //msg_type 0x02
                    if(img_flag == 0)
                    {
                        offset[j++] = recv;
                        if(j >= offset_len)
                        {
                            j = 0;
                            img_flag = 1;
                        }

                    } else if(img_flag == 1)
                    {
                        file_len[j++] = recv;
                        if(j >= length_len)
                        {
                            j = 0;
                            img_flag = 2;
                        }
                    } else if(img_flag == 2)
                    {
                        fn_size = recv;
                        img_flag = 3;
                    } else if(img_flag == 3)
                    {
                        rec_filename[j++] = recv;
                        // printf("[recv] %c\r\n", rec_filename[j - 1]);
                        if(j >= fn_size)
                        {
                            j = 0;  
                            img_flag = 4;
                            // printf("[debug] rec_filename down ok\r\n");
                        }

                    } else
                    {
                        checksum = recv;
                        calcchecksum = msg_type;
                        calcchecksum += length[0] + length[1];
                        calcchecksum += offset[0] + offset[1] + offset[2] + offset[3];
                        calcchecksum += file_len[0] + file_len[1];
                        calcchecksum += fn_size;
                        for(int i = 0; i < fn_size; i++)
                        {
                            calcchecksum += rec_filename[i];
                        }
                        if(checksum == calcchecksum)
                        {
                            uint16_t file_len_2 = (file_len[1] << 8) | file_len[0];
                            // printf("[file len2] = %d\r\n", file_len_2);

                            uint8_t ch_payload[2000];
                            uint8_t ch_type = 0x03;

                            ch_payload[0] = ch_type; //type

                            uint32_t offset_4 = (offset[3] << 24) | (offset[2] << 16) | (offset[1] << 8) | offset[0];

                            FIL tmpfile;
                            FRESULT tmpret = FR_OK;
                            // printf("offset_4 = %d\r\n", offset_4);

                            tmpret = f_open(&tmpfile, rec_filename, FA_READ);
                            f_lseek(&tmpfile, offset_4);
                            img_data = malloc(file_len_2);                                                // memory allocation size = image size
                            FRESULT readret = f_read(&tmpfile, img_data, file_len_2, (void *)&bytesread); //read file and move to img_size
                            // printf("[DEBUG] read RESULT : [%d]\r\n", readret);
                            f_close(&tmpfile); //close file pointer

                            if(tmpret != FR_OK)
                            {
                                printf("open file error len : %d\r\n", fn_size);
                                printf("open file [%s] err[%d]\n", rec_filename, tmpret);
                                char *open_err_msg = "file open Err";
                                uart_send_data(UART_NUM, open_err_msg, strlen(open_err_msg));
                            } else
                            {
                                // printf("[DEBUG] rec filename : [%s] \r\n", rec_filename);
                                for(int i = (file_len_2 - 1), j = 3; i >= 0; i--, j++)
                                {
                                    // printf("0x%x ,",img_data[i]);
                                    ch_payload[j] = img_data[i];
                                    if(i == offset_4)
                                    {
                                        break;
                                    }
                                }
                                uint16_t ch_len = file_len_2;
                                ch_payload[2] = (ch_len >> 8) & 0xff;
                                ch_payload[1] = ch_len & 0xff; //LSB first order

                                //checksum
                                uint8_t khecksum = 0;
                                for(int i = 0; i < (3 + ch_len); i++)
                                {
                                    khecksum += ch_payload[i];
                                }
                                ch_payload[3 + ch_len] = khecksum; // checksum

                                uart_send_data(UART_NUM, ch_payload, (3 + ch_len + 1));
                                // printf("ch_len : %d\r\n", ch_len);
                            }
                            free(img_data);
                        } else
                        {
                            printf("[0x02] err checksum not match recv[0x%x] <-> calc[0x%x]\r\n", checksum, calcchecksum);
                            char errmsg[14] = "CHECKSUM-ERROR";
                            uart_send_data(UART_NUM, errmsg, 14);
                        }
                        rec_flag = 0;
                        img_flag = 0;
                        // printf(" -------------finish line----------\r\n");
                    }
                    break;
                case 5: //msg_type 0x04
                    ymdhms[rtcoffset++] = recv;
                    if(rtcoffset >= 8)
                    {
                        //ymdhms [0~6] : ymdhms [7] : checksum
                        uint8_t rtc_checksum = msg_type + length[0] + length[1];
                        for(int i = 0; i < 7; i++)
                        {
                            rtc_checksum += ymdhms[i];
                        }
                        if(ymdhms[7] == rtc_checksum)
                        {                                                    //if checksum ok, setRTC and msg send
                            uint16_t recv_year = ymdhms[1] << 8 | ymdhms[0]; // uint8_t [year] merge
                            rtc_timer_set(recv_year, ymdhms[2], ymdhms[3], ymdhms[4], ymdhms[5], ymdhms[6]);
                            printf("timer set OK [%d:%d:%d:%d:%d:%d]\r\n", recv_year, ymdhms[2], ymdhms[3], ymdhms[4], ymdhms[5], ymdhms[6]);
                            uint8_t getRTC[4] = {0x05, 0x00, 0x00, 0x05};
                            uart_send_data(UART_NUM, getRTC, 4);
                        } else
                        { //error occur!
                            printf("error! ymdhms[7] = 0x%x #### rtc_checksum = 0x%x\r\n", ymdhms[7], rtc_checksum);
                            char errmsg[14] = "CHECKSUM-ERROR";
                            uart_send_data(UART_NUM, errmsg, 14);
                        }
                        rtcoffset = 0;
                        rec_flag = 0;
                    }
                    break;
            }
        }
        // lcd_draw_picture(0, 0, 320, 240, g_ram_mux ? g_lcd_gram0 : g_lcd_gram1);
    }
    iomem_free(g_lcd_gram0);
    iomem_free(g_lcd_gram1);
    return 0;
}
