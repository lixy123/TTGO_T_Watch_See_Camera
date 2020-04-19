#define SHOW_DEBUG

#include <TTGO.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <JPEGDecoder.h>

String report_address = "192.168.1.20";  //服务端的ip, 需要修改
String url_head_stream = "/jpeg"; 
int url_head_stream_port = 18080;  //服务端端口, 需要修改

TTGOClass *ttgo;


//功能：连接摄像头服务端，将摄像头数据传入并显示到TTGO-WATCH显示屏
//触发：按自定义按钮一次工作1分钟，60次，每秒1张图片(可修改代码做成不限时)
//效果:
//      大部分情况下图像传输稳定性高
//      每秒1张图像, 图片刷新慢.
//      显示效果可行，屏质量不错，基本没偏色现象
//      http交互方式效果不高，可考虑一次交互连续返回的mjpg方式读入视频流能提高速度
//esp32在传输wifi中稳定性一般，有时接收数据中会中断，甚至重启
//编译：
//     开发板选择 TTGO T-Watch 
//     PSRAM Enabled (开机时占用其中的50k内存缓存收到的图像)

//程序中的对容易阻塞的地方使用语句 yield(); delay(5) ; 
//可提高wifi传输稳定性, 可能原因是给wifi通讯释放时间, 
//arduino单个函数超过一定时间不返回程序会重启，用yield()可告诉系统不要重启
//
//偶尔启动报错：
//E (1755) spiram: SPI SRAM memory test fail. 4/131072 writes failed, first @ 3FAE9020
//出现本错误时，程序运行中重启机率大. 重启后本错误消失.

//电流110ma
const char* ssid = "CMCC-r3Ff";   //路由器账号，要调整
const char* password = "9999900000"; //路由器密码，要调整
WiFiClient wifiClient;
uint8_t buff[1024];     //http交互时缓存

//50K字节 ( 240*240的jpg文件，压缩比70, 单张图像大小约 7k-15k)
unsigned int img_buff_max = 50 * 1024;
byte* img_buff;         //缓存收到的图像 50k
unsigned int img_buff_p = 0;  //缓存指针

bool working = false;   //工作状态，工作状态时loop()函数不工作，防止影响图片接收

void setup()
{
  Serial.begin(115200);


  Serial.println("init ttgo");
  ttgo = TTGOClass::getWatch();
  ttgo->begin();
  ttgo->openBL();

  ttgo->eTFT->fillScreen(TFT_BLACK);

  Serial.println("connectwifi");
  //WIFI 连接
  connectwifi();

  //无效！
  //MDNS.begin(host);

  //初始化单张图片接收缓存内存
  img_buff = (byte*)ps_malloc(img_buff_max);
  Serial.println("start");

  //自定义按钮初始化
  ttgo->button->setPressedHandler(button2_pressed);

}

//自定义按钮按下后触发, 60次摄像头读取,1分钟左右
void button2_pressed()
{
  uint32_t starttime = 0;

  working = true;
  if (WiFi.status() == WL_CONNECTED)
  {
    int cnt = 60;
    Serial.println("button2_pressed!");
    for (int loop1 = 0; loop1 < cnt; loop1++)
    {
      starttime = millis() ;
      //url地址
      String ret = get_jpg(report_address, url_head_stream_port,  url_head_stream );
      if (ret.equals("succ"))
      {
        drawArrayJpeg(img_buff, img_buff_p, 0, 0);
        Serial.println("total show imgage " + String(millis() - starttime) + " ms");
      }
    }
  }
  working = false;

}

void connectwifi()
{
  if (WiFi.status() == WL_CONNECTED) return;
  //关闭再开，提高连接成功率
  WiFi.disconnect();
  delay(200);
  //  WiFi.config(IPAddress(192, 168, 1, 101), //设置静态IP位址
  //              IPAddress(192, 168, 1, 1),
  //              IPAddress(255, 255, 255, 0),
  //              IPAddress(192, 168, 1, 1)
  //             );
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to WIFI");
  int lasttime = millis() / 1000;
  WiFi.begin(ssid, password);

  while ((!(WiFi.status() == WL_CONNECTED))) {
    delay(1000);
    Serial.print(".");

    //5分钟连接不上，重启再试，防止无限连接
    //提高连接成功率
    if ( abs(millis() / 1000 - lasttime ) > 300 )
    {
      ets_printf("reboot\n");
      esp_restart();
    }
  }
  Serial.println("Connected");
  Serial.println("My Local IP is : ");
  Serial.println(WiFi.localIP());
  delay(1000);  //增加一些等待时间
}


//####################################################################################################
// Draw a JPEG on the TFT pulled from a program memory array
//####################################################################################################
void drawArrayJpeg(const byte arrayname[], unsigned int array_size, int x, int y) {

  Serial.println("drawArrayJpeg, len=" + String(array_size));
  //内存数组中装载jpg图像
  JpegDec.decodeArray(arrayname, array_size);
  jpegInfo(); // Print information from the JPEG file (could comment this line out)
  jpegRender(x, y);
  Serial.println("#######################################");
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = ttgo->eTFT->getSwapBytes();
  ttgo->eTFT->setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = min(mcu_w, max_x % mcu_w);
  uint32_t min_h = min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    //pushImage 输出图像
    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= ttgo->eTFT->width() && ( mcu_y + win_h ) <= ttgo->eTFT->height())
      ttgo->eTFT->pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= ttgo->eTFT->height())
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  ttgo->eTFT->setSwapBytes(swapBytes);
  showTime(millis() - drawTime); // These lines are for sketch testing only
}


void showTime(uint32_t msTime) {
  //tft.setCursor(0, 0);
  //tft.setTextFont(1);
  //tft.setTextSize(2);
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.print(F(" JPEG drawn in "));
  //tft.print(msTime);
  //tft.println(F(" ms "));
  Serial.println(String(" JPEG drawn in ") + String(msTime) + " ms");
}

void jpegInfo() {
  return;
  Serial.println(F("==============="));
  Serial.println(F("JPEG image info"));
  Serial.println(F("==============="));
  Serial.print(F(  "Width      :"));
  Serial.println(JpegDec.width);
  Serial.print(F(  "Height     :")); Serial.println(JpegDec.height);
  Serial.print(F(  "Components :")); Serial.println(JpegDec.comps);
  Serial.print(F(  "MCU / row  :")); Serial.println(JpegDec.MCUSPerRow);
  Serial.print(F(  "MCU / col  :")); Serial.println(JpegDec.MCUSPerCol);
  Serial.print(F(  "Scan type  :")); Serial.println(JpegDec.scanType);
  Serial.print(F(  "MCU width  :")); Serial.println(JpegDec.MCUWidth);
  Serial.print(F(  "MCU height :")); Serial.println(JpegDec.MCUHeight);
  Serial.println(F("==============="));
}




void loop()
{
  //处于工作状态就不loop了，防止wifi转向
  if (!working)
  {
    ttgo->button->loop();
    connectwifi();
    lv_task_handler();
    delay(5);
  }
}



//请求读取服务端图片（返回摄像头服务器上的240*240的 jpg图像, 显示至显示屏)
String get_jpg(String host, int port, String url)
{
  img_buff_p = 0;

  if (!wifiClient.connect(host.c_str(), port)) {
#ifdef SHOW_DEBUG
    Serial.println("get_jpg connection failed");
#endif
    return ("get_jpg connection failed");
  }

  String  HttpHeader = String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: " + host + ":" + String(port) + "\r\n" +
                       "Connection: keep-alive\r\n\r\n" ;

#ifdef SHOW_DEBUG
  Serial.println("===> " + HttpHeader);
#endif
  //Serial.println(body);
  wifiClient.print(HttpHeader);

  String retstr = "";
  String line = "";
  bool jpg_ok = false;
  bool http_ok = false;
  bool head_ok = false;
  uint32_t filesize = 0;

  String tmpstr = "";
  uint32_t starttime = 0;
  uint32_t stoptime = 0;


  starttime = millis() / 1000;
  while (!wifiClient.available())
  {
    yield();
    delay(5);
    stoptime = millis() / 1000;
    //10秒无响应，跳出
    if (stoptime - starttime >= 10)
    {
#ifdef SHOW_DEBUG
      Serial.println("get_jpg timeout >10s");
#endif
      return "";
    }

  }

  bool savejpg_ok = false;
  while (wifiClient.available())
  {
    //下两句很重要，所有wifiClient.available 的下一步都加上此句
    //可提高接收数据成功率, 同时减少重启概率
    yield();
    delay(5);
    //稳定性一般，wifi传输数据接收不完整概率大，约5%
    //本次图像接收不成功就跳过
    //以下语句与服务器返回值有很大关系，如果服务端协议变化要相应调整
    //编程时有用的二个工具软件:
    //1. Fiddler 监控跟踪http交互信息.
    //2. Postman http测试
    if (head_ok == true)
    {
      Serial.println("head_ok find");
      if (http_ok && jpg_ok && filesize > 0)
      {
        Serial.println("begin savejpg filesize=" + String(filesize));
        savejpg_ok = savejpg(filesize);
        break;
      }

    }
    line = wifiClient.readStringUntil('\n');
    Serial.println("line:" + line);
    if (line.startsWith("HTTP/1.1 200 OK") or line.startsWith("HTTP/1.0 200 OK"))
    {
      http_ok = true;
      Serial.println(">HTTP 200 OK");

      //line不含'\n'
      //Serial.println(String("HTTP 200 OK").length());
      //Serial.println(line.length());
    }
    if (line.startsWith("Content-Length: "))
    {
      tmpstr = line.substring(String("Content-Length: ").length(), line.length());
      Serial.println(">Content-Length: " + tmpstr );
      //Serial.println();
      //如果超过65535可能会有问题? 
      filesize = tmpstr.toInt();
    }

    if (line.startsWith("Content-Type: application/octet-stream"))
    {
      jpg_ok = true;
      Serial.println(">Content-Type: application/octet-stream");
    }

    if (line.length() == 1 && line.startsWith("\r"))
    {
      head_ok = true;
      Serial.println(">end");
      //break;
    }
    //Serial.println(line);
  }
  delay(10);
  Serial.println("wifiClient.stop ");
  wifiClient.stop();

  if (img_buff_p > 0 and savejpg_ok)
    return ("succ");
  else
    return ("fail");
}


//偶尔出现丢失数据现象，可能是wifi信号原因
bool savejpg(long file_size)
{
  long writenum = 0;
  uint32_t starttime = 0;
  uint32_t stoptime = 0;
  //最多等3秒
  starttime = millis() / 1000;
  while (  writenum < file_size)
  {
    while (wifiClient.available())
    {
      yield();
      delay(5);
      int readnum = wifiClient.read(buff, 1024);

      memcpy(img_buff + img_buff_p, buff, readnum);
      img_buff_p = img_buff_p + readnum;
      writenum = writenum + readnum;
      starttime = millis() / 1000;
    }
    if ( millis() / 1000 - starttime > 3)
    {
      Serial.println("savejpg timeout >3s" );
      break;
    }
    yield();
    delay(5);
  }


#ifdef SHOW_DEBUG
  Serial.println("savejpg success FileSize=" + String(writenum));
#endif

  //成功收到图像条件
  //图像接收字节>0, 接收字节数等于预期文件长度
  if (img_buff_p > 0 and (file_size == writenum))
    return (true);
  else
    return (false);
}
