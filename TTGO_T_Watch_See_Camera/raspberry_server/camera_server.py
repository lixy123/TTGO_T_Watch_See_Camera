# -*- coding: utf-8 -*-
#https://craftor.org/2016/07/03/Python_mjpeg/
#MJPEG Server for the webcam

import string,cgi,time
from os import curdir, sep
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
from SocketServer  import ThreadingMixIn
import cv2
import re
import sys
import imutils
import socket
from picamera.array import PiRGBArray
from picamera import PiCamera
import io
import numpy 
import numpy as np
import datetime,time

#功能:
#mjpeg服务器，可运行于树莓派上网络传图
#
#1.服务端运行方式:
# 执行 python camera_server.python 
#2.客户端访问:
#用chrome等支持html5的浏览器，IE不支持
#地址输入 (改变IP)
#  http://192.168.1.20:18080/stream
#  http://192.168.1.20:18080/jpeg


# initialize the camera and grab a reference to the raw camera capture
camera = PiCamera()
#camera.resolution = (640, 480) #原文建议
camera.resolution = (240, 240)  #输出分辨率
camera.framerate = 30 #原文建议15
camera.vflip = True  #垂直翻转
camera.hflip = True  #左右翻转
rawCapture = io.BytesIO()
exit_now = 0  
cameraQuality = 70  #压缩质量 0-100
port = 18080
print ("Usage : webcamserver <port>")

if len(sys.argv) > 1 :    
    port = int(sys.argv[1])

class MyHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global cameraQuality,exit_now,camera,rawCapture
        
        try:
            print(str(self.path))
            self.path=re.sub('[^.a-zA-Z0-9]', "",str(self.path))
           
            if self.path =="stream":
                print("begin mjpeg...")
                self.send_response(200)
                self.wfile.write("Content-Type: multipart/x-mixed-replace; boundary=--aaboundary")
                self.wfile.write("\r\n\r\n")
                
                frame_rate_calc = 1
                freq = cv2.getTickFrequency()
                last_do = datetime.datetime.now()
                t1 = cv2.getTickCount() 
                cnt_loop=0
                cnt_sum=0
                for f in camera.capture_continuous(rawCapture, format="jpeg", use_video_port=True):
                    if exit_now==0:
                        buff = numpy.fromstring(rawCapture.getvalue(), dtype=numpy.uint8)
                        #frame = cv2.imdecode(buff, cv2.CV_LOAD_IMAGE_UNCHANGED) 
                        frame = cv2.imdecode(buff, -1)
                        t2 = cv2.getTickCount()
                        
                        cnt_loop=cnt_loop+1
                        time1 = (t2-t1+cnt_sum)/freq/cnt_loop
                        cnt_sum=cnt_sum+t2-t1
                        if cnt_loop>=100:
                            cnt_loop=0
                            cnt_sum=0                        
                        frame_rate_calc = 1/time1                           
                        ts =  datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                        
                        cv2.putText(frame, ts, (10, frame.shape[0] - 10), cv2.FONT_HERSHEY_SIMPLEX,0.8, (0, 0, 255), 2) 
                        cv2.putText(frame,"FPS: {0:.2f} RUN:{1:.0f}ms".format(frame_rate_calc,(datetime.datetime.now()- last_do).microseconds/1000),(30,50),cv2.FONT_HERSHEY_SIMPLEX,0.8,(255,255,0),2,2)
                        t1 = cv2.getTickCount()
                        last_do = datetime.datetime.now()
                        cv2mat1 = cv2.imencode(".jpeg", frame,[int(cv2.IMWRITE_JPEG_QUALITY),cameraQuality])[1]
                        data_encode = np.array(cv2mat1)
                        JpegData1 = data_encode.tostring()
                        
                        self.wfile.write("--aaboundary\r\n")
                        self.wfile.write("Content-Type: image/jpeg\r\n")
                        self.wfile.write("Content-length: "+str(len(JpegData1))+"\r\n\r\n" )
                        self.wfile.write(JpegData1)
                        self.wfile.write("\r\n\r\n\r\n")
                        #time.sleep(0.02)
                        rawCapture.truncate()
                        rawCapture.seek(0)                          
                                                 
                return
            if self.path.endswith("jpeg"):
                rawCapture = io.BytesIO()
                camera.start_preview()
                # 摄像头预热
                time.sleep(0.1)
                camera.capture(rawCapture, 'jpeg')
                buff = numpy.fromstring(rawCapture.getvalue(), dtype=numpy.uint8)
                frame = cv2.imdecode(buff, -1)
                cv2mat1 = cv2.imencode(".jpeg", frame,[int(cv2.IMWRITE_JPEG_QUALITY),cameraQuality])[1]
                data_encode = np.array(cv2mat1)
                JpegData1 = data_encode.tostring()
                self.send_response(200)
                self.send_header('Content-Type','application/octet-stream')
                self.send_header('Content-Length',len(JpegData1))
                self.end_headers()
                self.wfile.write(JpegData1)
                return   
               
            return
        except IOError:
            self.send_error(404,'File Not Found: %s' % self.path)


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in a separate thread."""

myname = socket.getfqdn(socket.gethostname())
myaddr = socket.gethostbyname(myname)

def main():
    while 1:
        try:
            server = ThreadedHTTPServer(('0.0.0.0', port), MyHandler)
            print( 'Starting httpServer...')
            print ('See <Local IP>:'+ str(port) + '/stream')
            server.serve_forever()
        except KeyboardInterrupt:
            print( '^C received, shutting down server')
            exit_now=1
            time.sleep(1)
            server.socket.close()            
            break

if __name__ == '__main__':
    main()