DVP2SDCARD_DEV
---------------------------------------
# 
```
1. take a pic format ( BMP16, BMP24, JPG )
2. image transfer
3. Real-Time clock set 
```
#### by Uart Communication
---------------------------------------
| format| description|
|:--------|:--------:|
| BMP16 | use rgb565tobmp() |
| BMP24 | use rgb888tobmp() |
| JPEG(default) | use rgb888tobmp , stbi_load and tje_encode_to_file_at_quality|
----------------------------------------
### return message according to message type
__*all message is LSB first order & have Checksum in endline__
1. snap ( sender )

| type | length| format|
|:---:|:--:|:---:|
|0x00|0x0100|0x03|

2. snap Finished ( this program )

| type | length|size |filename size| filename|
|:---:|:--:|:--:|:---:|:---:|
|0x01|0x0D00|0x45230100|0x08|1234.jpg|

3. Get Image (sender)

| type | length| offset |chunk length| filename size| filename|
|:---:|:--:|:--:|:---:|:---:|:---:|
|0x02|0x0f00|0x00000400|0x0A00|0x08|1234.jpg|

4. Image Chunk ( this program )

| type | length| Chunk |
|:---:|:--:|:---:|
|0x03|0x6400|100 bytes stream|

5. Set RTC ( sender ex) 2020.02.03 15:30:01 )

| type | length| year | month | day | hour | minute | second |
|:---:|:--:|:--:|:---:|:---:|:---:|:---:|:---:|
|0x04|0x0700|0xe407|0x02|0x03|0x0f|0x1e|0x01|

6. get RTC ( this program )

| type | Length |
|:--:|:--:|
|0x05|0x0000|
----------------------------------------
## mark1.py (demo Program)
간단한 byte Message 전송 프로그램
```
> 기능 1 : Snap ( 사진찍기 요청 )
> 기능 2 : Get Image ( 이미지 전송 요청 )
> 기능 3 : Set RTC ( RTC 설정 요청 )
```
#### **Uart to USB 케이블을 사용해 시리얼 통신으로 메시지 전송.**
----------------------------------------
----------------------------------------
## + 함수 설명
#### **1. main.c**
> [1] io_mux_init()
> ```
> GPIO 설정용 함수
> ```
> [2] io_set_power()
> ```
> 보드 전압 설정용 함수
> ```  
> [3] rtc_init() , rtc_timer_set (날짜)
> ```
> RTC 설정 초기화 및 시간설정용 함수
> ```  
> [4] uart_init(UART_NUM)
> ```
> uart 통신 초기화 함수
> * 가능한 가장 늦게 초기화하기를 권장
> 다른 초기화함수보다 일찍 초기화하면, 장비 리셋 후 receive 레지스터에 알수없는 바이트가 남아있음.
> ```  
> [5] while문 이후
> ```
> 반복행위 = uart_receive_data (UART_NUM, &recv, 1)  
> 1 Byte 데이터를 UART_NUM 장비에서 받아 recv변수에 저장한다.
>  
> 레지스터에 저장된 바이트메세지를 1 byte씩 순차적으로 분석한다.  
> 
> 예) 중계기에서 0x00\0x01\0x00\0x03 을 카메라 장비에 보낸다.
> 1. 레지스터에 0x00\0x01\0x00\0x03이 쌓인다.  
> 2. recv에 0x00이 들어오고, msg_type에 저장한 뒤 rec_flag를 1로 설정후 break한다.
> 3. recv에 0x01이 들어오고, length[0] 에 저장 후 break한다.
> 4. recv에 0x00이 들어오고 length[1]에 저장 후 break한다.
> 5. length를 다 받았으므 앞에서 받은 msg_type인 0x00[Snap]에 따라 rec_flag를 2로 설정한 후 break한다.  
> 6. recv에 0x03[가장 높은 압축]이 들어오고, format에 값을 저장한 뒤 rec_flag를 3으로 설정한 후 break한다. 
>7. 앞에서받은 format에 따라 snap을 수행하고 결과에 따른 byte message를 만든다.
>8. byte message를 uart_send_data를 이용해 중계기에 전송한다.  
> 
> 다른 바이트 메세지들도 같은 방식으로 분석해서 처리한다.
> ```  

#### **2. stb_image.h**  
>  [>> Original File Link <<](https://github.com/nothings/stb, "githublink")  
>  ```
>  24BIT bmp파일을 읽어서 RAW한 데이터로 만들어준다.
>  [원본과의 차이점]
>  1. 원본파일에서 fwrite, fopen같은 파일관련 함수를 *FATFS 파일시스템*에 맞게 f_write, f_open등으로 바꿔주었다.
>  ```

#### **3. tiny_jpeg.h**

>  [>> Original File Link <<](https://github.com/nothings/stb, "githublink")  
>  ```
>  stbi_load 함수로 변환한 데이터를 jpeg파일로 만들어 저장해준다.
>  [원본과의 차이점]
>  1. 원본파일에서 fwrite, fopen같은 파일관련 함수를 *FATFS 파일시스템*에 맞게 변경
> 2. 기존에 리턴값이 1또는 0이였는데, 파일크기를 반환하게 수정하였다.
>  ```
