import serial
import math
import time

def makeYMDHMS(recvstr):
    ymdhms_byte = bytearray()
    year = int(recvstr[0:4]).to_bytes(2,"little")
    month = bytes([int(recvstr[4:6])])
    day = bytes([int(recvstr[6:8])])
    hour = bytes([int(recvstr[8:10])])
    minute = bytes([int(recvstr[10:12])])
    second = bytes([int(recvstr[12:14])])
    ymdhms_byte.append(0x04)
    ymdhms_byte.append(0x07)
    ymdhms_byte.append(0x00)
    ymdhms_byte = ymdhms_byte + year + month + day + hour + minute + second
    rtc_check = checksumCalc(ymdhms_byte)
    ymdhms_byte.append(rtc_check)
    return ymdhms_byte

def modcalc(filesize):
    mod = filesize % 1024
    modbyte = bytearray()
    modbyte.append(mod & 0xff)
    modbyte.append( (mod >> 8) & 0xff)
    return modbyte

def checksumCalc(bytearr):
    checkres = 0
    for i in bytearr:
        checkres = checkres + i
    # print("checkres = %x" % (checkres & 0xff))
    return (checkres & 0xff)

def int_tobyte(intval):
    result = bytearray()
    result.append(intval & 0xff)
    result.append((intval >> 8) & 0xff)
    result.append((intval >> 16) & 0xff)
    result.append((intval >> 24) & 0xff)
    return result

def bytelen(hexvalue):
    return (int(math.log(hexvalue,256)) + 1)

#Format 0x01 ~ 0x03
def makeSnap(Format):
    Snap = bytearray()
    checksum = 0x00

    msgType = 0x00

    Snap.append(msgType) #type
    checksum = checksum + msgType #add msgtype byte to checksum
    Snap.append(0x01)
    Snap.append(0x00)
    checksum = checksum + 0x01 # add msglength byte to checksum
    Snap.append(Format) #Format
    checksum = checksum + Format # add Format byte to checksum
    Snap.append(checksum) #Checksum
    print(Snap)
    return Snap

ser = serial.Serial(port='COM12',
        baudrate=115200,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=3)
checksum = 0x33

Snap = makeSnap(0x01)

Type = bytearray(b'\x02')
Length = bytearray(b'\x0f\x00')
offset = bytearray(b'\x00\x00\x00\x00') # start 0 offset
ofLength = bytearray(b'\x00\x04') # 1024 byte 0x0400
fnsize = bytearray(b'\x08')
filename = b'001A.jpg'
filesize = 0


payload = bytearray()

flag = 0
offsetplate = 0
payloadlen = 0
startflag = 1
while 1:
    if (flag == 0):
        print (" [ 1 ] select 1 (GetImage)")
        print (" [ 3 ] select 3 (Snap)")
        print (" [ 4 ] select 4 (setRTC)")
        print (" -------> ", end='')
        startflag = 1
        a = input()
        if (a == '1'):
            print (" [filename] : ", end='')
            filename = input()
            print (" [filesize] : ", end='')
            filesize = int(input())
    else:
        a = '1'


    if (a == '1'):
        getImage = Type + Length + int_tobyte(offsetplate) + ofLength + fnsize + filename.encode()
        getImage.append(checksumCalc(getImage))

        ser.write(getImage[:10])
        time.sleep(0.1)
        ser.write(getImage[10:])
        payload = ser.read(1028) # 1024 + 1 + 2 + 1
        flag = 1
    elif (a == "3"):
        print("[Choose number [1 or 2 or 3] 1=row 2=mid 3=high ] -----------> ", end="")
        Snap = makeSnap( int(input()) & 0xff )
        ser.write(Snap)
        payload = ser.read(17)
    elif (a == "4"):
        print("[putdown Year Month day Hour Minute Second]")
        print("[ ex) year [2019] month [3] day [21] time [14:30:15] -> 20190321143015 ] ->>>>>>>>>>>> ",end="")
        ymdhms = makeYMDHMS(input())
        ser.write(ymdhms)
        payload = ser.read(4)

    print("\n-------- read line -----------")


    # for debug
    # print("[payload] : ",end="")
    # print(payload)



    print("[ - Type - ] : ",end="")
    print(payload[0:1])

    if (payload[0:1] == b'\x01') :
        print("----------------- receive Snap Finished ---")
        print("[ - Length - ] : ", end='')
        print(int.from_bytes(payload[1:3] , byteorder='little'))
        print("[ - Size - ] : ", end="")
        print(int.from_bytes(payload[3:7] , byteorder='little'))
        print("[ - filename size - ] : ", end="")
        print(int.from_bytes(payload[7:8] , byteorder='little'))
        print("[ - filename - ] : ", end="")
        print(payload[8:16])
        print("[ - checksum - ] : %x" % int.from_bytes(payload[16:17] , byteorder='little'))
        print("[ - calcchecksum - ] : %x" % checksumCalc(payload[:16]))


    elif (payload[0:1] == b'\x03') :
        payloadlen = payloadlen + ( len(payload) - 4 )
        print("download... : %d/%d ----------- [%d%%]" % (payloadlen , filesize ,(payloadlen/filesize*100)))
        offsetplate = offsetplate + 1024
        if (payloadlen + 1024 >= filesize):
            ofLength = modcalc(filesize)

        if (payloadlen >= filesize):
            flag = 0
            offsetplate = 0
            payloadlen = 0
            ofLength = bytearray(b'\x00\x04')
            # startflag = 1

        if (startflag):
            f = open(filename,"wb")
            startflag = 0
        else:
            f = open(filename,"ab")
        recvlen = len(payload) - 4
        while(recvlen):
            f.write(payload[recvlen+2:recvlen+3])
            recvlen = recvlen - 1

        f.close()

    else:
        print("[ - payload - ] : ", end="")
        print(payload)
    print("----------------------------\n")
