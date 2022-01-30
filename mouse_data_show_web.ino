//#include <BluetoothSerial.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <SPI.h>
#include "RTClib.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include "DHT.h"
#include "Adafruit_Sensor.h"
#define DHTPIN 21   //DHT11 DATA 数据引脚
#define DHTTYPE DHT11  //选择的类型

#define HALLPIN1 19
#define HALLPIN2 18
#define HALLPIN3 5
#define HALLPIN4 17
#define CLOCK_SDA_PIN 4
#define CLOCK_SCL_PIN 16
#define wifi_sta
#define SAVELOG

//#define test
const char* ssid = "web_show";//设置热点名称(自定义)
const char* password = "1234567890";//设置热点密码(自定义)
const char* ssid_sta = "long";//设置热点名称(自定义)
const char* password_sta = "yy201011";//设置热点密码(自定义)
AsyncWebServer server(80);//web服务器端口号
RTC_DS3231 rtc;
DHT dht(DHTPIN , DHTTYPE);
//BluetoothSerial SerialBT;

#ifdef test
int mm_test = 1 , dd_test = 10 , hh_test = 0;
#endif // test


struct Time
{
    uint16_t YY;
    uint8_t MM;
    uint8_t DD;

    uint8_t week;
    String time;
} time_;

double sendRunData_R[4][60] = {};//4个跑轮，每个60个数据
int sendHumiture_R[2][24] = {};//温湿度，24小时数据
//float mouseRunNum1 = 0 , mouseRunNum2 = 0 , mouseRunNum3 = 0 , mouseRunNum4 = 0;


char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
volatile double runData1 = 0 , runData2 = 0 , runData3 = 0 , runData4 = 0;//老鼠跑动的数据

void showData(int data , bool printType = false , bool type = true);
void showData(double data , bool printType = false , bool type = true);
void showData(String data , bool printType = false , bool type = true);

void setup()
{
    Serial.begin(115200);//开启串口打印
    //SerialBT.begin("mouse");
    //初始化闪存系统
    Serial.print("正在打开闪存系统...");
    while ( !SPIFFS.begin(true) )
    {
        Serial.print("...");
        delay(1000);
    }
    Serial.println("OK!");
    // 可用空间总和（单位：字节）
    Serial.print("可用空间总和: ");
    Serial.print(SPIFFS.totalBytes());
    Serial.println(" Bytes");

    // 已用空间（单位：字节）
    Serial.print("已用空间: ");
    Serial.print(SPIFFS.usedBytes());
    Serial.println(" Bytes");

    Wire.begin(CLOCK_SDA_PIN , CLOCK_SCL_PIN);

    if ( !rtc.begin() )
    {
        Serial.println("时钟未连接，最终程序");
        Serial.flush();
        while ( 1 ) delay(10);
    }

    if ( rtc.lostPower() )
    {
        Serial.println("时钟时间错误，现在修改，稍后请务必手动校准时间");
        rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
    }
    /*Serial.println(__DATE__);
    Serial.println(__DATE__== "Jan 2 2022");*/
    //rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
    pinMode(HALLPIN1 , INPUT_PULLUP);
    pinMode(HALLPIN2 , INPUT_PULLUP);
    pinMode(HALLPIN3 , INPUT_PULLUP);
    pinMode(HALLPIN4 , INPUT_PULLUP);

    //设置中断触发程序

    dht.begin();

#ifndef wifi_sta
    WiFi.softAP(ssid , password);
#endif // !1

#ifdef wifi_sta
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(ssid , password);
    WiFi.begin(ssid_sta , password_sta);
    Serial.print("wifi连接中，请稍等...");
    int conn_try_num = 0;//尝试连接wifi的时间
    while ( WiFi.status() != WL_CONNECTED )
    { // 尝试进行wifi连接。
        delay(1000);
        conn_try_num++;
        Serial.print(".");
        if ( conn_try_num > 30 )
        {
            Serial.println("wifi连接超时。。。");
            break;
        }
    }
    Serial.print("IP address:\t");            // 以及
    Serial.println(WiFi.localIP());           // NodeMCU的IP地址
#endif // wifi_sta


    Serial.print("Access Point: ");    // 通过串口监视器输出信息
    Serial.println(ssid);              // 告知用户NodeMCU所建立的WiFi名
    Serial.print("IP address: ");      // 以及NodeMCU的IP地址
    Serial.println(WiFi.softAPIP());   // 通过调用WiFi.softAPIP()可以得到NodeMCU的IP地址
    if ( SPIFFS.exists("/index.html") )
    {
        Serial.println("存在控制台文件，开启服务器");
        AsyncStaticWebHandler* handler = &server.serveStatic("/log" , SPIFFS , "/").setDefaultFile("log");

        if ( WiFi.status() == WL_CONNECTED )
        {
            AsyncStaticWebHandler* handler1 = &server.serveStatic("/" , SPIFFS , "/").setDefaultFile("index_conn.html");
            handler1->setAuthentication("admin" , "admin");
            handler1->setLastModified("Fri, 14 Jan 2022 15:43:02 GMT");
        } else
        {
            AsyncStaticWebHandler* handler1 = &server.serveStatic("/" , SPIFFS , "/").setDefaultFile("index.html");
            handler1->setAuthentication("admin" , "admin");
            handler1->setLastModified("Fri, 14 Jan 2022 15:43:02 GMT");
        }
        handler->setAuthentication("log" , "admin");
        handler->setCacheControl("max-age=10");

        server.on("/reqRunData" , HTTP_POST , sendRunData);//给前端反馈跑动信息
        server.on("/reqHumiture" , HTTP_POST , sendHumiture);//给前端反馈温湿度信息
        server.on("/getServerTime" , HTTP_POST , getServerTime);
        server.on("/setServerTime" , HTTP_POST , setServerTime);
        server.begin();//开启web服务器 
        //WiFi.mode(WIFI_MODE_NULL);
    } else
    {
        Serial.println("不存在控制台文件");
    }
    savaRunData();
    saveHumitureData();

    pinMode(0 , INPUT);
    xTaskCreatePinnedToCore(
        runData_tack                                     //创建任务
        , "runData_tack"                                 //创建任务的名称
        , 1024 * 2                                   //分配的运行空间大小
        , NULL                                       //任务句柄
        , 2                                           //任务优先级
        , NULL                                        //回调函数句柄
        , 1);
    xTaskCreatePinnedToCore(
        showWifi                                     //创建任务
        , "showWifi"                                 //创建任务的名称
        , 1024 * 2                                   //分配的运行空间大小
        , NULL                                       //任务句柄
        , 3                                           //任务优先级
        , NULL                                        //回调函数句柄
        , 1);
}

void showWifi(void* pvParameters)
{
    int num = 0;
    while ( true )
    {
        if ( !digitalRead(0) )
        {
            num++;
            Serial.println(num);
        } else
        {
            num = 0;
        }
        if ( num == 3 )
        {
            num = 0;
            if ( WiFi.getMode() == WIFI_MODE_APSTA || WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_STA )
            {
                Serial.print("wifi状态不应该被更改，现在最终");
                continue;
            }
            /* WiFi.mode(WIFI_MODE_NULL);
             vTaskDelay(1000);*/
            num = 0;
            WiFi.mode(WIFI_MODE_APSTA);
            WiFi.softAP(ssid , password);
            WiFi.begin(ssid_sta , password_sta);
            Serial.print("wifi连接中，请稍等...");
            int conn_try_num = 0;//尝试连接wifi的时间
            while ( WiFi.status() != WL_CONNECTED )
            { // 尝试进行wifi连接。
                delay(1000);
                conn_try_num++;
                Serial.print(".");
                if ( conn_try_num > 30 )
                {
                    Serial.println("wifi连接超时。。。");
                    break;
                }
            }
            if ( WiFi.status() == WL_CONNECTED )
            {
                Serial.print("IP address:\t");            // 以及
                Serial.println(WiFi.localIP());           // NodeMCU的IP地址
            }

            //vTaskDelay(10000);
            //WiFi.mode(WIFI_MODE_NULL);
        }
        vTaskDelay(1000);
    }
}

void runData_tack1(void* pvParameters)
{
    while ( true )
    {
        if ( digitalRead(HALLPIN1) == LOW )
        {
            while ( true )
            {
                if ( digitalRead(HALLPIN1) == HIGH )
                {
                    runData1 += 0.25;
                    showData("mouseRun1 " , true , false);
                    showData(runData1 , true);
                    break;
                }
                vTaskDelay(25);
            }
        }
        vTaskDelay(12);
    }
}

void runData_tack2(void* pvParameters)
{
    while ( true )
    {
        if ( digitalRead(HALLPIN2) == LOW )
        {
            while ( true )
            {
                if ( digitalRead(HALLPIN2) == HIGH )
                {
                    runData2 += 0.25;
                    showData("mouseRun2 " , true , false);
                    showData(runData2 , true);
                    break;
                }
                vTaskDelay(25);
            }
        }
        vTaskDelay(10);
    }
}

void runData_tack3(void* pvParameters)
{
    while ( true )
    {
        if ( digitalRead(HALLPIN3) == LOW )
        {
            while ( true )
            {
                if ( digitalRead(HALLPIN3) == HIGH )
                {
                    runData3 += 0.25;
                    showData("mouseRun3 " , true , false);
                    showData(runData3 , true);
                    break;
                }
                vTaskDelay(25);
            }
        }
        vTaskDelay(8);
    }
}

void runData_tack4(void* pvParameters)
{
    while ( true )
    {
        if ( digitalRead(HALLPIN4) == LOW )
        {
            while ( true )
            {
                if ( digitalRead(HALLPIN4) == HIGH )
                {
                    runData4 += 0.25;
                    showData("mouseRun4 " , true , false);
                    showData(runData4 , true);
                    break;
                }
                vTaskDelay(25);
            }
        }
        vTaskDelay(9);
    }
}

void runData_tack(void* pvParameters)
{
    volatile bool state[4] = {};
    while ( true )
    {
        if ( digitalRead(HALLPIN1) == LOW )
        {
            state[0] = true;
        }
        if ( digitalRead(HALLPIN2) == LOW )
        {
            state[1] = true;
        }
        if ( digitalRead(HALLPIN3) == LOW )
        {
            state[2] = true;
        }
        if ( digitalRead(HALLPIN4) == LOW )
        {
            state[3] = true;
        }
        if ( state[0] && digitalRead(HALLPIN1) == HIGH )
        {
            runData1 += 0.25;
            showData("mouseRun1 " , true , false);
            showData(runData1 , true);
            state[0] = false;
        }
        if ( state[1] && digitalRead(HALLPIN2) == HIGH )
        {
            runData2 += 0.25;
            showData("mouseRun2 " , true , false);
            showData(runData2 , true);
            state[1] = false;
        }
        if ( state[2] && digitalRead(HALLPIN3) == HIGH )
        {
            runData3 += 0.25;
            showData("mouseRun3 " , true , false);
            showData(runData3 , true);
            state[2] = false;
        }
        if ( state[3] && digitalRead(HALLPIN4) == HIGH )
        {
            runData4 += 0.25;
            showData("mouseRun4 " , true , false);
            showData(runData4 , true);
            state[3] = false;
        }
        delay(10);
    }
}
void getServerTime(AsyncWebServerRequest* request)
{
    DateTime now = rtc.now();

    String fileName = "";
    fileName += now.year();
    fileName += "-";
    fileName += now.month();
    fileName += "-";
    fileName += now.day();
    fileName += " ";
    fileName += now.hour();
    fileName += ":";
    fileName += now.minute();
    fileName += ":";
    fileName += now.second();
    fileName += " ";
    /*String MM_[12] = { "Jan","Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
    fileName += MM_[now.dayOfTheWeek()];*/
    Serial.println(fileName);
    request->send(200 , "text/plain" , fileName);
}
void setServerTime(AsyncWebServerRequest* request)
{
    String reqDate;
    time_.YY = request->arg("yy").toInt();
    time_.MM = request->arg("mm").toInt();
    time_.DD = request->arg("dd").toInt();
    time_.week = request->arg("week").toInt();
    time_.time = request->arg("time");
    String MM_[12] = { "Jan","Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
    String DATE = "";
    DATE += MM_[time_.MM - 1];
    DATE += " ";
    DATE += time_.DD;
    DATE += " ";
    DATE += time_.YY;
    Serial.print("写入数据DATE:");
    Serial.println(DATE);
    Serial.print("写入数据TIME:");
    Serial.println(time_.time);
    rtc.adjust(DateTime(DATE.c_str() , time_.time.c_str()));
    Serial.print("设置时间:成功");

    request->send(200 , "text/plain" , "setTime_OK");
}
void sendRunData(AsyncWebServerRequest* request)
{
    String reqDate;
    reqDate += "/";
    if ( request->hasArg("date") )
    {
        reqDate += request->arg("date");
        Serial.print("请求老鼠跑动数据:");

        Serial.println(reqDate);

        request->send(200 , "text/plain" , sendData_mouseRun(reqDate));
    }
}
void sendHumiture(AsyncWebServerRequest* request)
{
    String reqDate;
    reqDate += "/";
    if ( request->hasArg("date") )
    {
        reqDate += request->arg("date");
        Serial.print("请求温湿度数据");
        Serial.println(reqDate);
        request->send(200 , "text/plain" , sendData_humiture(reqDate));
    }
}

//向客户端发送老鼠跑动的数据
String sendData_mouseRun(String date)
{
    //if ( date.length()!=11 )//hhhhmmdd@hh
    //{
    //    Serial.println("请求日期有误");
    //    return "err:Incorrect request date";
    //}
    if ( SPIFFS.exists(date) )
    {
        File dataFile = SPIFFS.open(date , "r");
        long dataSize = dataFile.size();
        String fsData;
        for ( int i = 0; i < dataSize; i++ )
        {
            fsData += (char) dataFile.read();
        }
        dataFile.close();
        return fsData;
    } else
    {
        showData("err:The file was not found" , true);
        return "err:The file was not found";
    }
}

//向客户端发送温湿度数据
String sendData_humiture(String date)
{
    //if ( date.length() != 8 )//hhhhmmdd
    //{
    //    Serial.println("请求日期有误");
    //    return "err:Incorrect request date";
    //}
    if ( SPIFFS.exists(date) )
    {
        File dataFile = SPIFFS.open(date , "r");
        long dataSize = dataFile.size();
        String fsData;
        for ( int i = 0; i < dataSize; i++ )
        {
            fsData += (char) dataFile.read();
        }
        dataFile.close();
        return fsData;
    } else
    {
        return "err:The file was not found";
    }
}

//保存老鼠跑动的数据
void savaRunData()
{
    //必须结束中断，否则容易被狗复位

    Serial.print("闪存剩余空间: ");
    int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    Serial.print(freeSpace);
    Serial.println(" Bytes");
    if ( freeSpace < 3000 )
    {
        Serial.println("存储空间告急！！！");
    } else if ( freeSpace < 1500 )
    {
        Serial.println("存储空间已经用尽，请手动清理！！！");
        return;
    } else
    {
        //Serial.println("存储空间充足！！！");
        showData("存储空间充足！！！" , true);
    }
    String runData;

    DynamicJsonDocument doc(4096);
    DateTime now = rtc.now();

    String fileName = "/";
    fileName += now.year();
    if ( now.month() < 10 )
    {
        fileName += "0";
    }
    fileName += now.month();
    if ( now.day() < 10 )
    {
        fileName += "0";
    }
    fileName += now.day();
    fileName += "@";
    fileName += now.hour();
#ifdef test
    if ( mm_test > 12 )
    {
        mm_test = 1;
    }
    if ( dd_test > 28 )
    {
        dd_test = 1;
    }
    if ( hh_test > 24 )
    {
        hh_test = 1;
    }
    fileName = "/2022";
    if ( mm_test < 10 )
    {
        fileName += "0";
    }
    fileName += mm_test;
    if ( dd_test < 10 )
    {
        fileName += "0";
    }
    fileName += dd_test;
    fileName += "@";
    fileName += hh_test;
#endif // test

    /* Serial.print("当前老鼠跑动的数据存储文件名：");
     Serial.println(fileName);*/

    showData("当前老鼠跑动的数据存储文件名：" , true , false);
    showData(fileName , true);
    doc["dataType"] = "runData";
    doc["date"] = fileName;//hhhhyydd@hh

    JsonArray data1 = doc.createNestedArray("data1");
    //data1.add(10);

    for ( size_t j = 0; j < 60; j++ )
    {
        data1.add(sendRunData_R[0][j]);
    }


    JsonArray data2 = doc.createNestedArray("data2");
    for ( size_t j = 0; j < 60; j++ )
    {
        data2.add(sendRunData_R[1][j]);
    }


    JsonArray data3 = doc.createNestedArray("data3");

    for ( size_t j = 0; j < 60; j++ )
    {
        data3.add(sendRunData_R[2][j]);
    }

    JsonArray data4 = doc.createNestedArray("data4");

    for ( size_t j = 0; j < 60; j++ )
    {
        data4.add(sendRunData_R[3][j]);
    }

    serializeJson(doc , runData);

    //Serial.println(fileName);
    //Serial.println(runData);
    showData(runData , true);
    File dataFile = SPIFFS.open(fileName , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
    dataFile.println(runData);       // 向dataFile写入字符串信息
    dataFile.close();

    saveLog("savaRunData");
}

//保存温湿度
void saveHumitureData()
{
    //必须结束中断，否则容易被狗复位

    Serial.print("闪存剩余空间: ");
    int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    Serial.print(freeSpace);
    Serial.println(" Bytes");
    if ( freeSpace < 3000 )
    {
        Serial.println("存储空间告急！！！");
    } else if ( freeSpace < 1500 )
    {
        Serial.println("存储空间已经用尽，请手动清理！！！");
        return;
    } else
    {
        Serial.println("存储空间充足！！！");
    }

    String runData;

    StaticJsonDocument<1024> doc;

    DateTime now = rtc.now();
    String fileName = "/";
    fileName += now.year();
    if ( now.month() < 10 )
    {
        fileName += "0";
    }
    fileName += now.month();
    if ( now.day() < 10 )
    {
        fileName += "0";
    }
    fileName += now.day();

#ifdef test
    if ( mm_test > 12 )
    {
        mm_test = 1;
    }
    if ( dd_test > 28 )
    {
        dd_test = 1;
    }
    fileName = "/2022";
    if ( mm_test < 10 )
    {
        fileName += "0";
    }
    fileName += mm_test;
    if ( dd_test < 10 )
    {
        fileName += "0";
    }
    fileName += dd_test;

#endif // test
    Serial.print("当前温湿度的数据存储文件名：");
    Serial.println(fileName);
    doc["dataType"] = "humitureData";
    doc["date"] = fileName;//hhhhmmdd

    JsonArray temperature = doc.createNestedArray("temperature");
    for ( size_t i = 0; i < 24; i++ )
    {
        temperature.add(sendHumiture_R[0][i]);
    }

    JsonArray humidity = doc.createNestedArray("humidity");
    for ( size_t i = 0; i < 24; i++ )
    {
        humidity.add(sendHumiture_R[1][i]);
    }
    serializeJson(doc , runData);

    Serial.println(fileName);
    Serial.println(runData);
    File dataFile = SPIFFS.open(fileName , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
    dataFile.println(runData);       // 向dataFile写入字符串信息
    dataFile.close();


    saveLog("saveHumitureData");
}

int time_num_mm = 0;//用于记录跑动数据存储数组的索引值
int time_num_hh = 0;//用于记录温湿度存储数组的索引值
int wifiConnNum = 0;
void loop()
{
#ifndef test
    delay(1000 * 60);//等待一分钟
    wifiConnNum++;
    if ( wifiConnNum > 5 )
    {
        WiFi.mode(WIFI_MODE_NULL);//wifi连接5分钟自动关闭
        wifiConnNum = 0;
    }

    //delay(2000);
    sendRunData_R[0][time_num_mm - 1] = runData1;
    sendRunData_R[1][time_num_mm - 1] = runData2;
    sendRunData_R[2][time_num_mm - 1] = runData3;
    sendRunData_R[3][time_num_mm - 1] = runData4;
    String sData_t = (String) time_num_mm;
    sData_t += " minute:runData1@";
    sData_t += sendRunData_R[0][time_num_mm - 1];
    sData_t += "\trunData2@";
    sData_t += sendRunData_R[1][time_num_mm - 1];
    sData_t += "\trunData3@";
    sData_t += sendRunData_R[2][time_num_mm - 1];
    sData_t += "\trunData4@";
    sData_t += sendRunData_R[3][time_num_mm - 1];
    showData(sData_t , true);
#endif // !test

#ifdef test
    delay(100);
#endif // test


    time_num_mm++;

#ifdef test
    /*sendRunData_R[0][time_num_mm - 1] = random(0 , 100);
    sendRunData_R[1][time_num_mm - 1] = random(0 , 100);
    sendRunData_R[2][time_num_mm - 1] = random(0 , 100);
    sendRunData_R[3][time_num_mm - 1] = random(0 , 100);*/

    sendRunData_R[0][time_num_mm - 1] = runData1;
    sendRunData_R[1][time_num_mm - 1] = runData2;
    sendRunData_R[2][time_num_mm - 1] = runData3;
    sendRunData_R[3][time_num_mm - 1] = runData4;
#endif // test

    runData1 = 0 , runData2 = 0 , runData3 = 0 , runData4 = 0;

    if ( time_num_mm >= 59 )//数据量达到一小时
    {
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        if ( isnan(h) || isnan(t) )
        {
            Serial.println("温湿度读取异常！");
            sendHumiture_R[0][time_num_hh] = 0;
            sendHumiture_R[1][time_num_hh] = 0;
        #ifdef test
            hh_test++;
            sendHumiture_R[0][time_num_hh] = random(0 , 100);
            sendHumiture_R[1][time_num_hh] = random(0 , 100);
        #endif // test
        } else
        {
            Serial.print("Humidity: ");
            Serial.print(h);
            Serial.print(" %t");
            Serial.print("Temperature: ");
            Serial.print(t);
            Serial.println(" *C ");
            sendHumiture_R[0][time_num_hh] = t;
            sendHumiture_R[1][time_num_hh] = h;
        #ifdef test
            hh_test++;
            sendHumiture_R[0][time_num_hh] = random(0 , 100);
            sendHumiture_R[1][time_num_hh] = random(0 , 100);
        #endif // test

        }
        time_num_hh++;
        //向闪存中写入这一小时老鼠跑动的数据
        savaRunData();
        time_num_mm = 0;

        if ( time_num_hh >= 23 )
        {
            time_num_hh = 0;
            saveHumitureData();
        #ifdef test
            dd_test++;
        #endif // test

        }
    }
}

//中断服务程序
//void mouseRun1()
//{
//    runData1 += 0.25;
//    /* Serial.print("mouseRun1 ");
//     Serial.println(runData1);*/
//    showData("mouseRun1 " , true , false);
//    showData(runData1 , true);
//}
//void mouseRun2()
//{
//    runData2 += 0.25;
//    Serial.print("mouseRun2 ");
//    Serial.println(runData2);
//}
//void mouseRun3()
//{
//    runData3 += 0.25;
//    Serial.print("mouseRun3 ");
//    Serial.println(runData3);
//}
//void mouseRun4()
//{
//    runData4 += 0.25;
//    Serial.print("mouseRun4 ");
//    Serial.println(runData4);
//}

void showData(String data , bool printType , bool type)
{
    if ( type )
    {
        Serial.println(data);
        if ( printType )
        {
            //SerialBT.println(data);
        }
    } else
    {
        Serial.print(data);
        if ( printType )
        {
            //SerialBT.print(data);
        }
    }
}

void showData(double data , bool printType , bool type)
{
    if ( type )
    {
        Serial.println(data);
        if ( printType )
        {
            //SerialBT.println(data);
        }
    } else
    {
        Serial.print(data);
        if ( printType )
        {
            //SerialBT.print(data);
        }
    }
}

void showData(int data , bool printType , bool type)
{
    if ( type )
    {
        Serial.println(data);
        if ( printType )
        {
            //SerialBT.println(data);
        }
    } else
    {
        Serial.print(data);
        if ( printType )
        {
            //SerialBT.print(data);
        }
    }
}

void saveLog(String data)
{
#ifndef SAVELOG
    return;
#endif // savaLog

    DateTime now = rtc.now();
    String date = "";
    date += now.year();
    date += "-";
    date += now.month();
    date += "-";
    date += now.day();
    date += " ";
    date += now.hour();
    date += ":";
    date += now.minute();
    date += ":";
    date += now.second();
    Serial.println(date);



    if ( SPIFFS.exists("/log") )
    {
        String wData = date;
        wData += "  \t";
        wData += data;
        wData += "\n";

        File dataFile = SPIFFS.open("/log" , "a");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
        dataFile.println(wData);       // 向dataFile写入字符串信息
        dataFile.close();
        showData("日志保存成功" , true);
    } else
    {
        showData("不存在日志文件" , true);
    }


}