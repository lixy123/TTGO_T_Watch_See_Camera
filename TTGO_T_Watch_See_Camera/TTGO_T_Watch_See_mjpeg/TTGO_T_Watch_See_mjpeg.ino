#define SHOW_DEBUG

#include <TTGO.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <JPEGDecoder.h>

String report_address = "192.168.1.20";
String url_head_stream = "/stream"; //mjpg摄像头地址
int url_head_stream_port = 18080;

TTGOClass *ttgo;

//偶尔启动错误：
//E (1755) spiram: SPI SRAM memory test fail. 4/131072 writes failed, first @ 3FAE9020
//出现本错误时，程序重启机率大.
//wifi丢失数据的情况也比较多，平均5%机率.
//平均每切换1张图片约1000ms FPS=1

//程序会重启，原因不明！

//程序中的yield(); delay(5) ; 是为提高数据定性  arduion内置机制，单个函数超过一定时间不返回的话，程序会重启
//2.按钮触发，显示连续视频

//mjpg读取文件流，平均每张图150ma  FPS=6 (每秒6张图)  主要时间消耗在解析JPG并显示(约110ms)
//提升速度的关键点是解析显示JPG,受限于硬件，提升余地不大.
//pc电脑测试 fps:20 每张图 50ms

//电流约110ma

const char* ssid = "CMCC-r3Ff";
const char* password = "9999900000";
WiFiClient wifiClient;
uint8_t buff[1024];

//50K字节 ( 240*240的jpg文件，大小一般 7k-15k)
unsigned int img_buff_max = 50 * 1024;
byte* img_buff;
unsigned int img_buff_p = 0;

bool working = false;

void setup()
{
  Serial.begin(115200);

  //  if (!SPIFFS.begin()) {
  //    Serial.println("SPIFFS initialisation failed!");
  //    while (1) yield(); // Stay here twiddling thumbs waiting
  //  }
  //  Serial.println("\r\nSPIFFS initialised.");

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

  img_buff = (byte*)ps_malloc(img_buff_max);
  Serial.println("start");

  ttgo->button->setPressedHandler(button2_pressed);

}

void button2_pressed()
{
  uint32_t starttime = 0;

  working = true;
  if (WiFi.status() == WL_CONNECTED)
  {
    get_jpg(report_address, url_head_stream_port,  url_head_stream );
  }
  working = false;
}

void connectwifi()
{
  if (WiFi.status() == WL_CONNECTED) return;


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

    //5分钟连接不上，自动重启
    if ( abs(millis() / 1000 - lasttime ) > 300 )
    {
      ets_printf("reboot\n");
      esp_restart();
    }
  }
  Serial.println("Connected");
  Serial.println("My Local IP is : ");
  Serial.println(WiFi.localIP());
  delay(1000);
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




//文字转图片（返回240*120 256色黑白jpg)
String get_jpg(String host, int port, String url)
{
  //img_buff_p = 0;

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
    if (stoptime - starttime >= 10)
    {
#ifdef SHOW_DEBUG
      Serial.println("get_jpg timeout >10s");
#endif
      return "";
    }
  }

  starttime = millis() / 1000;
  //增加一层循环，防止接收的head处理不完全
  while (true)
  {
    yield();
    delay(5);

    stoptime = millis() / 1000;
    //5秒还接收完head算失败！
    if (stoptime - starttime >= 5)
    {
#ifdef SHOW_DEBUG
      Serial.println("get_jpg timeout >5s");
#endif
      return "";
    }

    while (wifiClient.available())
    {
      yield();
      delay(5);

      if (http_ok && jpg_ok && head_ok)
      {
        Serial.println("begin find_jpg" );
        find_jpg();
        break;
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


      //
      if (line.startsWith("Content-Type: multipart/x-mixed-replace; boundary=--aaboundary"))
      {
        jpg_ok = true;
        Serial.println(">Content-Type: multipart/x-mixed-replace; boundary=--aaboundary");
      }

      if (line.length() == 1 && line.startsWith("\r"))
      {
        head_ok = true;
        Serial.println(">end");
        //break;
      }
      //Serial.println(line);
    }
  }
  delay(10);
  Serial.println("wifiClient.stop ");
  wifiClient.stop();


  return ("succ");

}

//查找出现连续b1,b2的位置
unsigned int  findbyte_index(const byte arrayname[],  int array_size, byte b1, byte b2)
{
  //byte * array_p= arrayname;
  int  find_index = -1;
  for ( int loop1 = 0; loop1 < array_size - 1; loop1++)
  {
    if (arrayname[loop1] == b1 && arrayname[loop1 + 1] == b2)
    {
      find_index = loop1;
      break;
    }
  }
  return find_index;
}


//通过寻找jpg文件头，文件尾，实现无限显示jpg图像
bool find_jpg()
{
  uint32_t starttime = 0;
  uint32_t stoptime = 0;

  int jpg_begin_index = -1;
  int jpg_end_index = -1;
  bool file_bad = false;
  img_buff_p = 0;

  //此函数无限循环
  while (true)
  {
    yield();
    delay(5);
    while (wifiClient.available())
    {
      yield();
      delay(5);  //不加fps:6.5 120ms >> 加上延时后  每张图7k, 增加时间35ms fps=6 影响不大！
      int readnum = wifiClient.read(buff, 1024);
      //检查jpeg头
      jpg_begin_index = findbyte_index(buff, readnum, 0xFF, 0xd8);
      //检查jpeg尾
      jpg_end_index = findbyte_index(buff, readnum, 0xFF, 0xd9);

      //Serial.println("readnum=" + String(readnum) + ",jpg_begin_index=" + String(jpg_begin_index) +
      //    ",jpg_end_index=" + String(jpg_end_index)+ ",file_bad=" + String(file_bad));

      //1.发现jpg尾
      if  (jpg_end_index > -1)
      {
        Serial.println("find jpg_end_index,file_bad=" + String(file_bad));
        if (file_bad == false)  //判断本次文件丢弃标志
        {
          if  (img_buff_p + jpg_end_index + 2 <= img_buff_max)
          {
            memcpy(img_buff + img_buff_p, buff, jpg_end_index + 2);
            img_buff_p = img_buff_p + jpg_end_index + 2;

            if (file_bad == false)
            {
              //初步判断文件大小，并显示图像
              if (img_buff_p > 3000)
              {
                //执行drawArrayJpeg
                drawArrayJpeg(img_buff, img_buff_p, 0, 0);
                //Serial.println("drawArrayJpeg filesize=" + String(img_buff_p));
                //Serial.println("##################################################");
                //file_bad = true;
                img_buff_p = 0;
                Serial.println("用时:" + String(millis() - starttime) + " ms");
                starttime = millis() ;
              }
              else
                Serial.println("filesize <3k");

              //处理连续数据中的下一张jpg数据
              if (jpg_begin_index > -1 &&  jpg_begin_index > jpg_end_index)
              {
                Serial.println("本次数据 find jpg_begin_index,file_bad=" + String(file_bad));
                starttime = millis();
                img_buff_p = 0;
                //后半截数据追加缓存
                //1k数据不可能越界，不需检查越界
                memcpy(img_buff + img_buff_p, buff + jpg_begin_index, readnum - jpg_begin_index);
                img_buff_p = img_buff_p + readnum - jpg_begin_index;
                file_bad = false;  //清空上次文件丢弃标志

              }
            }
          }
          else
          {
            Serial.println("skip jpg尾");
            file_bad = true;
          }
        }
      }
      //2.发现jpg头 (重新开始计缓存)
      else if (jpg_begin_index > -1)
      {
        Serial.println("find jpg_begin_index,file_bad=" + String(file_bad));
        starttime = millis();
        img_buff_p = 0;
        //后半截数据追加缓存
        //1k数据不可能越界，不需检查越界
        memcpy(img_buff + img_buff_p, buff + jpg_begin_index, readnum - jpg_begin_index);
        img_buff_p = img_buff_p + readnum - jpg_begin_index;
        file_bad = false;  //清空上次文件丢弃标志
      }

      //追加数据
      else
      {
        //Serial.println("append,file_bad=" + String(file_bad));
        if (file_bad == false) //判断本次文件丢弃标志
        {
          //img_buff_p=0;
          //指针越界才追加缓存
          //如果越界，设置文件越界标志，全部接收数据丢弃
          if (img_buff_p + readnum <= img_buff_max)
          {
            memcpy(img_buff + img_buff_p, buff, readnum);
            img_buff_p = img_buff_p + readnum;
          }
          else
            file_bad = true;
        }
      }
    }

  }

}
