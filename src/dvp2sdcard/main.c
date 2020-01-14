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
#include "dvp.h"
#include "fpioa.h"
#include "lcd.h"
#include "ov5640.h"
#include "ov2640.h"
#include "plic.h"
#include "sysctl.h"
#include "uarths.h"
#include "nt35310.h"
#include "board_config.h"
#include "sdcard.h"
#include "ff.h"
#include "rgb2bmp.h"
#include "gpiohs.h"
#include "iomem.h"
#include "tiny_jpeg.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

void irq_key(void* gp)
{
   g_save_flag = 1;
}

static int on_irq_dvp(void* ctx)
{
    if (dvp_get_interrupt(DVP_STS_FRAME_FINISH))
    {
        /* switch gram */
        dvp_set_display_addr(g_ram_mux ? (uint32_t)g_lcd_gram0 : (uint32_t)g_lcd_gram1);

        dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
        g_dvp_finish_flag = 1;
    }
    else
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
    fpioa_set_function(38, FUNC_GPIOHS0 + DCX_GPIONUM);
    fpioa_set_function(36, FUNC_SPI0_SS3);
    fpioa_set_function(39, FUNC_SPI0_SCLK);
    fpioa_set_function(37, FUNC_GPIOHS0 + RST_GPIONUM);

    sysctl_set_spi0_dvp_data(1);

    fpioa_set_function(16, FUNC_GPIOHS0 + KEY_GPIONUM);
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
    fpioa_set_function(8, FUNC_GPIOHS0 + DCX_GPIONUM);
    fpioa_set_function(6, FUNC_SPI0_SS3);
    fpioa_set_function(7, FUNC_SPI0_SCLK);

    sysctl_set_spi0_dvp_data(1);

    fpioa_set_function(26, FUNC_GPIOHS0 + KEY_GPIONUM);

#endif
}

static void io_set_power(void)
{
#if BOARD_LICHEEDAN
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);

#else
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);
#endif
}

int main(void)
{
    FATFS sdcard_fs;

    /* Set CPU and dvp clk */
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    
    uarths_init();

    io_mux_init();
    io_set_power();
    plic_init();

    gpiohs_set_drive_mode(KEY_GPIONUM, GPIO_DM_INPUT);
    gpiohs_set_pin_edge(KEY_GPIONUM, GPIO_PE_FALLING);
    gpiohs_set_irq(KEY_GPIONUM, 2, irq_key);

    /* LCD init */
    printf("LCD init\r\n");
    lcd_init();
#if BOARD_LICHEEDAN
    #if OV5640
    lcd_set_direction(DIR_YX_LRDU);
    #else
    lcd_set_direction(DIR_YX_LRDU);
    #endif
#else
    #if OV5640
    lcd_set_direction(DIR_YX_LRUD);
    #else
    lcd_set_direction(DIR_YX_LRUD);
    #endif
#endif

    lcd_clear(BLACK);

    g_lcd_gram0 = (uint32_t *)iomem_malloc(320*240*2);
    g_lcd_gram1 = (uint32_t *)iomem_malloc(320*240*2);
    /* DVP init */
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

    /* DVP interrupt config */
    printf("DVP interrupt config\r\n");
    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);

    /* enable global interrupt */
    sysctl_enable_irq();

    /* SD card init */
    if (sd_init())
    {
        printf("Fail to init SD card\r\n");
        return -1;
    }

    /* mount file system to SD card */
    if (f_mount(&sdcard_fs, _T("0:"), 1))
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
    uint16_t i = 0;
    while (1)
    {
        /* ai cal finish*/
        while (g_dvp_finish_flag == 0)
            ;
        g_ram_mux ^= 0x01;
        g_dvp_finish_flag = 0;

        if (g_save_flag)
        {
            int width, height, num_components;
            width = 320;
            height = 240;
            num_components = 3;
            char filename2[15];
            char fileout[15];
            sprintf(filename2, "0:%04X.BMP", i++);
            sprintf(fileout,"0:%04x.jpg",(i-1));
            //filename , fileout definition

            // rgb565tobmp((uint8_t*)(g_ram_mux ? g_lcd_gram0 : g_lcd_gram1), 320, 240, _T(filename2));
            rgb888tobmp((uint8_t*)(g_ram_mux ? g_lcd_gram0 : g_lcd_gram1), 320, 240, _T(filename2));
            // choose 16bit bmp or 24bit bmp

            unsigned char* data = stbi_load(filename2, &width, &height, &num_components, 0);
            //load data from bitmap file
            if ( !data ) {
            puts("Could not find file");
            }

            //convert bitmap data to jpeg file
            if ( !tje_encode_to_file(fileout, width, height, num_components, data) ) {
            fprintf(stderr, "Could not write JPEG\n");
            }
            else{
                printf("BMP -> JPG Success! filename : %s\r\n", fileout);
            }
            
            g_save_flag = 0;
        }
        /* display pic*/        
        lcd_draw_picture(0, 0, 320, 240, g_ram_mux ? g_lcd_gram0 : g_lcd_gram1);
    }
    iomem_free(g_lcd_gram0);
    iomem_free(g_lcd_gram1);
    return 0;


    
}

