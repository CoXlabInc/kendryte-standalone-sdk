#include "ff.h"
#include "rgb2bmp.h"
int rgb888tobmp(uint8_t *buf,int width,int height, const char *filename)
{
	FIL file;
	FRESULT ret = FR_OK;
	uint32_t ret_len = 0;
	uint32_t i;
	uint16_t temp;
	uint16_t *ptr;

	BitMapFileHeader bmfHdr;    /* file header */
	BitMapInfoHeader bmiHdr;    /* information header */
	// 24bit bitmap don't need pallete

	bmiHdr.biSize = sizeof(BitMapInfoHeader);
	bmiHdr.biWidth = width;     
	bmiHdr.biHeight = height;
	bmiHdr.biPlanes = 1;
	bmiHdr.biBitCount = 24; //24bit bitmap
	bmiHdr.biCompression = 0x00; //No compression
	bmiHdr.biSizeImage = (width * height * 3); //24bit need 3 (8x8x8)
	bmiHdr.biXPelsPerMeter = 0;
	bmiHdr.biYPelsPerMeter = 0;
	bmiHdr.biClrUsed = 0;
	bmiHdr.biClrImportant = 0;

	bmfHdr.bfType = 0x4D42; //bitmap 'BM' ASCII code
	// bmfHdr.bfSize = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) + sizeof(RgbQuad)*3 + bmiHdr.biSizeImage;
	bmfHdr.bfSize = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) + bmiHdr.biSizeImage;
	bmfHdr.bfReserved1 = 0;
	bmfHdr.bfReserved2 = 0;
	// bmfHdr.bfOffBits = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader)+ sizeof(RgbQuad)*3;
	bmfHdr.bfOffBits = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) ;


	
	//rgb565 data to rgb888 data
	uint8_t *data = malloc( width * height * 3);
	for (int i = 0, j= 0; i < width* height *3 ; i+= 3, j+=2)
	{
		uint16_t buffer = ( buf[j+1] << 8 ) | ( buf[j] ); // ex) [0xF5][0x12]
		
		uint8_t r = ((buffer & 0xf800) >> 11 );
		uint8_t g = ((buffer & 0x7E0) >> 5);
		uint8_t b = (buffer & 0x1F);
		
		//RGB888 - amplify
  		r <<= 3;
  		g <<= 2;
  		b <<= 3;
		
		//   printf("data[i] : %x\r\n", (b << 16) | (g << 8) | r );
		data[i] = b;
		data[i+1] = g;
		data[i+2] = r;
	}

    if ((ret = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK) 
    {
        printf("create file %s err[%d]\n", filename, ret);
        return ret;
    }

    f_write(&file, &bmfHdr, sizeof(BitMapFileHeader), &ret_len);
    f_write(&file, &bmiHdr, sizeof(BitMapInfoHeader), &ret_len);
	f_write(&file, data, bmiHdr.biSizeImage, &ret_len);
	f_close(&file);
	free(data);

 
	return 0;
}

int rgb565tobmp(uint8_t *buf,int width,int height, const char *filename)
{
	FIL file;
    FRESULT ret = FR_OK;
    uint32_t ret_len = 0;
    uint32_t i;
    uint16_t temp;
    uint16_t *ptr;
    
	BitMapFileHeader bmfHdr;    /* file header */
	BitMapInfoHeader bmiHdr;    /* information header */
	RgbQuad bmiClr[3]; /*color pallete*/
 
	bmiHdr.biSize = sizeof(BitMapInfoHeader);
	bmiHdr.biWidth = width;     
	bmiHdr.biHeight = height;
	bmiHdr.biPlanes = 1;
	bmiHdr.biBitCount = 16;
	bmiHdr.biCompression = BI_BITFIELDS;
	bmiHdr.biSizeImage = (width * height * 2);
	bmiHdr.biXPelsPerMeter = 0;
	bmiHdr.biYPelsPerMeter = 0;
	bmiHdr.biClrUsed = 0;
	bmiHdr.biClrImportant = 0;
 

	/* rgb565 mask */

	bmiClr[0].rgbBlue = 0;
	bmiClr[0].rgbGreen = 0xF8;
	bmiClr[0].rgbRed = 0;
	bmiClr[0].rgbReserved = 0;
 
	bmiClr[1].rgbBlue = 0xE0;
	bmiClr[1].rgbGreen = 0x07;
	bmiClr[1].rgbRed = 0;
	bmiClr[1].rgbReserved = 0;
 
	bmiClr[2].rgbBlue = 0x1F;
	bmiClr[2].rgbGreen = 0;
	bmiClr[2].rgbRed = 0;
	bmiClr[2].rgbReserved = 0;
 



	bmfHdr.bfType = 0x4D42; //bitmap 'BM' ASCII code
	bmfHdr.bfSize = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) + sizeof(RgbQuad)*3 + bmiHdr.biSizeImage;
	bmfHdr.bfReserved1 = 0;
	bmfHdr.bfReserved2 = 0;
	bmfHdr.bfOffBits = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader)+ sizeof(RgbQuad)*3;

    ptr = (uint16_t*)buf;
    
    for (i = 0; i < width * height; i += 2)
    {
        temp = ptr[i];
        ptr[i] = ptr[i + 1];
        ptr[i + 1] = temp;
    }

    if ((ret = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK) 
    {
        printf("create file %s err[%d]\n", filename, ret);
        return ret;
    }

    f_write(&file, &bmfHdr, sizeof(BitMapFileHeader), &ret_len);
    f_write(&file, &bmiHdr, sizeof(BitMapInfoHeader), &ret_len);
    f_write(&file, &bmiClr, 3*sizeof(RgbQuad), &ret_len);
    
	f_write(&file, buf, bmiHdr.biSizeImage, &ret_len);
	f_close(&file);

 
	return 0;
}

