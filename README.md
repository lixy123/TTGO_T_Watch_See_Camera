
TTGO_T_Watch 查看远程摄像头摄像

1.服务端：<BR/>
   硬件:树莓派 <BR/>
   目录:raspberry_server <BR/>
   运行: python camera_server.py<BR/>
<BR/>
2.客户端<BR/>
   硬件:TTGO T-WATCH <BR/>
   目录: TTGO_T_Watch_See_Camera  需调整路由器账号，密码，树莓派IP<BR/>
   编译工具 arduino 开发板选择 TTGO T-Watch, 设置端口，编译烧录至TTGO T-WATCH <BR/>
   运行：开机，待连通到路由器上后，按第二个键查看远程摄像头摄像60秒
   
   演示功能，学习用 <br/>
   目前刷新率很低，每秒一张，适合对速度要求的不高的监视,<br/>
   上传本程序的目的是给大家一个框架和学习demo,可以在此基础上进行功能改进
   
