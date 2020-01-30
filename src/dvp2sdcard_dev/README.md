DVP2SDCARD_DEV
=======================================
## 1. Take a pic and save as jpg file format
## 2. send pic by UART communication
1. Use  
    this directory, mark1.py
---------------------------------------
2. How to use  
    1-1) using kflash.py build this project and run  
    1-2) connect board with your Computer containing the mark1.py  
    1-3) GPIO 6 rx GPIO 7 TX (you can change by main.c function name [io_mux_init] )  
    
    2) run mark1.py (python mark1.py)  
    
    3) mark1.py option  
    
    | Name | Param | Usage |
    |------|---|---|
    | Get Image | image name , image size| get image from board by UART ( *mark1.py is Serial communication by UART /USB connector )|
    | Snap | quality| take a picture and save to board's sd card|
----------------------------------------