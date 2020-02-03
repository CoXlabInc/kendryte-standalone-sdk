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

# YOU CAN DEMO this program by mark1.py
