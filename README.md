
用TTGO-T-Watch手表查看远程摄像头

1.服务端：<BR/>
   硬件:树莓派(带摄像头) <BR/>
   目录:raspberry_server <BR/>
   运行: python camera_server.py<BR/>

2.客户端1<BR/> 硬件:TTGO T-WATCH <BR/>
   目录: TTGO_T_Watch_See_Camera  需调整路由器账号，密码，树莓派IP<BR/>
   编译工具 arduino 开发板选择 TTGO T-Watch, 设置端口，编译烧录至TTGO T-WATCH <BR/>
   运行：开机，待连通到路由器上后，按第二个键查看远程摄像头摄像60秒
   通过一次http交互读取远程摄像头一张图片，多次反复。 <BR/>演示功能，学习用 <br/>
   FPS约1，每秒一张，速度慢<br/>
   
 3.客户端2<BR/>
   上面的改进版<BR/>
   硬件:TTGO T-WATCH <BR/>
   目录: TTGO_T_Watch_See_mjpg   需调整路由器账号，密码，树莓派IP<BR/>
   运行：开机，待连通到路由器上后，按第二个键不限时查看远程摄像头<BR/>
   mjpg传输协议，请求一次，无限返回jpg图像，交互效率高<BR/>
   FPS约6, 每秒6张, 时间主要消耗在解析jpg并图形显示,限于硬件水平，不容易提升
   
   硬件效果: <br/>
   <img src= 'https://github.com/lixy123/TTGO_T_Watch_See_Camera/blob/master/TTGO_T_Watch_See_Camera/IMG_20200419_194214.jpg?raw=true' />
<br/>
<br/>
<img src= 'https://github.com/lixy123/TTGO_T_Watch_See_Camera/blob/master/TTGO_T_Watch_See_Camera/IMG_20200419_194835.jpg?raw=true' />
<br/>
