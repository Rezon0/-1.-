//===========пины подлючения===================
#define temp_pin 2  //temperature 
#define lock_pin 4  //servo
#define buzzer_pin 15 //пьезоэлемент
#define rgb_pin 13   //ws2812b на чайник 
#define rele_pin 12  //rele d7--GPIO13         
#define liquid_level_pin 5 //!!D1 - МЕНЯТЬ НЕЛЬЗЯ!!

#define BOTtoken "0000000000:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"  // токен бота
#define BOTname "NAME" // Имя бота
#define BOTusername "LOGIN" // Логин бота

#define analog_pin A0 //для генерации семени ГСЧ

#define LED_COUNT 6  // число светодиодов в ленте
#define light_pin 14  //пин ленты
#define bright 50 //яркость ленты

//===============библиотеки=================

#include <Servo.h>    //для сервопривода
Servo servo;  //объявляем переменную servo типа Servo

#include <OneWire.h>  //для температуры
OneWire ds(temp_pin); //пин подключения датчика температуры

#include "FastLED.h"  //для ленты

#include <ESP8266WiFi.h>  //для работы модуля и создания web-сервера
#include <ESP8266WebServer.h>

#include <WiFiClientSecure.h>   
#include <ESP8266TelegramBOT.h>   //для связи с telegram
TelegramBOT bot(BOTtoken, BOTname, BOTusername);  //инициализация бота

#include <NTPClient.h>    //для получения даты и времени в сети интернет
#include <WiFiUdp.h>
const long utcOffsetInSeconds = 10800;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}; //
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


//SSID и пароль для подключения к точке доступа
const char* ssid = "SSID";  // SSID
const char* password = "PASSWORD"; // пароль

ESP8266WebServer server(80);  //порт сервера

String enter_password_word; //строка для хранения входящего собщения
String guest_password;   //код гостя
String admin_password = "admin_tuta";   //код админа

//String chat_id = "000000000";   //id личного чата с ботом
String chat_id = "-000000000000";  //id группового чата с ботом


volatile byte error_schet = 0; //попытки неправильного ввода
byte temp_hysteresis; //значение температуры гестерезиса
int8_t temp;  //значение температуры
uint32_t myTimer1; //таймер millis

boolean flag_anti_recursion_temp = false; //флаг, чтобы избежать рекурсии с нагревом
boolean flag_stop_heating = false;  //флаг для кнопки отключения нагрева
boolean flag_error_schet = false;   //флаг для ограничений отправки ссобщений ботом, когда превышен лимит попыток
boolean flag_temp_hysteresis = false; //флаг для режима нагрева и стабильной работы гестерезиса
boolean flag_time = false;  //флаг для отслеживания установки запланированного времени срабатывания чайника

char arr[36] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};  //массив символов для генерации пароля гостя

struct CRGB leds[LED_COUNT]; //массив ленты

String day_of_week; //день недели
String plan_day_rus;  //переменная для конвертации русского слова на англ
//byte day; 
byte hour; //численное значение часа
byte minute; //численное значение минуты
byte time_temp; //численное значение температуры
boolean time_flag = false;


void setup() {

//==========объявление пинов подключения===============
  pinMode(buzzer_pin, OUTPUT);  //сюда подключается пищалку
  //pinMode(lock_pin, OUTPUT);    //сюда подключается сервопривод
  pinMode(rele_pin, OUTPUT);    //сюда подключается реле чайника
  digitalWrite(rele_pin, 0);    //выключить реле чайника
  pinMode(liquid_level_pin, INPUT_PULLUP);  //сюда подключается датчик уровня жидкости
  servo.attach(lock_pin); //сюда подключается сервопривод

  Serial.begin(115200); //открытие Serial порта 
  delay(100);

 /* Serial.println("Connecting to ");
  Serial.println(ssid);*/

  //подключение к точке доступа
  WiFi.begin(ssid, password);

  //проверяем, подключен ли модуль к точке доступа
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());
  
  timeClient.begin(); //создание клиента времени

  server.on("/lightness", lightness);  //освещение
  server.on("/password", enter_password);  //пароль
  server.on("/teapot", teapot); //чайник

  server.begin();  // запуск сервера
  //Serial.println("HTTP server started");

  for (byte i = 0; i < 15; i++) temperature();    //"простукивание" датчика температуры, чтобы показывал текущую температуру

//===========генерация кода гостя===============
    guest_password = "";
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    randomSeed(analogRead(analog_pin)); //зерно для функции random

  
  servo.write(0); //установить серво в начальное положение

LEDS.setBrightness(bright);  //максимальная яркость ленты
LEDS.addLeds<WS2812B, light_pin, GRB>(leds, LED_COUNT);  // настрйоки для ленты

set_color_to_light(5,5,5); //вкключить ленту
delay(100);
set_color_to_light(0,0,0); //выключить ленту
delay(100);

//===========отправка сооьщений ботом================
    bot.sendMessage(chat_id, "Система запущена. Хорошего использования!", "");  
    bot.sendMessage(chat_id, "Сгенерировн код Гостя. Актуальный код: '" + guest_password + "'", "");

}

void loop() {


temperature();  //измеряем температуру

//===========генератор кода гостя каждые 2 часа==============
  if (millis() - myTimer1 >= 7200000) {   // ищем разницу (2 часа)
    myTimer1 = millis();              // сброс таймера
    guest_password = "";
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    guest_password += (String)arr[random(0, 36)];
    bot.sendMessage(chat_id, "Сгенерирован код Гостя. Актуальный код: '" + guest_password + "'", "");
  }

  timeClient.update();  //получить значения времени

if((flag_time == true)){  //если есть запланированное включение

String dd = daysOfTheWeek[timeClient.getDay()]; //текущий день недели
byte hh = timeClient.getHours();   //текущий час
byte mm = timeClient.getMinutes();  //текущая минута

if((dd == day_of_week) && ((String)hh == (String)hour) && ((String)mm == (String)minute)) { //если запланированная дата и время совпали
  
flag_time = false; //опускаем флаг, чтобы не включился повторно 


    if (flag_anti_recursion_temp == false) {  //проверка на рекурсию (чтобы чайник не был уже включен)

  if(liquid_level() == true){   //проверка на уровень жидкости

        if ((time_temp <= 100) && (time_temp >= 25) && (temp < time_temp))  //проверка на температуру
        {
          buzzer_on();  //включить пищалку
          bot.sendMessage(chat_id, "Запланированный чайник включен. Режим Нагрева. Подогрев выключен. Конечная температура: " + (String)time_temp, "");
          teapot_heating(time_temp, true);  //процедура нагрева, true отключает функцию подогрева
        } else bot.sendMessage(chat_id, "Запланированный запуск чайника отклонен. Текущая температура воды выше заданной", "");
      } else {
          bot.sendMessage(chat_id, "Запланированный запуск чайника отклонен. Недостаточно воды", "");
}
  } else bot.sendMessage(chat_id, "Запланированный запуск чайника отклонен. Чайник был включен", "");
}
}


  server.handleClient();  //проверить входные данные на сервере
}


void teapot() { //процедура чайника

  server.send(200, "", "ok");    // ответить на HTTP запрос

if ((server.argName(0) == "day") && (server.argName(1) == "hour") && (server.argName(2) == "minute"))   //нажата кнопка для запланированное включение
{

if(server.arg(0) == "Понедельник")  //перевод с русского на англ
{
  day_of_week = "Monday";
} else if (server.arg(0) == "Вторник")
{
  day_of_week = "Tuesday";
} else if((server.arg(0) == "Среда"))
{
  day_of_week = "Wednesday";
} else if((server.arg(0) == "Четверг"))
{
  day_of_week = "Thursday";
} else if((server.arg(0) == "Пятница"))
{
  day_of_week = "Friday";
} else if((server.arg(0) == "Суббота"))
{
  day_of_week = "Saturday";
} else if((server.arg(0) == "Воскресенье"))
{
  day_of_week = "Sunday";
}

plan_day_rus = server.arg(0); //присвоение переменной значение на русском 

/*Serial.print(server.arg(0));
Serial.print("\t");
Serial.println(day_of_week);*/
  
hour = server.arg(1).toInt();   //запланированный час
minute = server.arg(2).toInt(); //запланированный минута
time_temp = server.arg(3).toInt(); //запланированная температура
flag_time = true; //включить режим запланированного включения

bot.sendMessage(chat_id, "Включение чайника запланировано на " + plan_day_rus + ", " + (String)hour + ":" + (String)minute + ". Конечная температура: " + (String)time_temp, "");
time_flag = true; //проверка, что не включен запланированный запуск??? зачем второй флаг для этого???
}

if (server.argName(0) == "stop_plan_heating") //нажата кнопка для отмены запланированного включения
      {
        
       if(flag_time == true)  bot.sendMessage(chat_id, "Запланированное включение чайника на " + plan_day_rus + ", " + (String)hour + ":" + (String)minute + " отменено", "");
        flag_time = false; 
      }

if (server.argName(0) == "actual_temp") //нажата кнопка для показа текущей температуры
      {
       bot.sendMessage(chat_id, "Текущая температура воды: " + (String)temp, "");
      }

if (server.argName(0) == "stop_heating")  //нажата кнопка для остановки нагрева
      {
        flag_stop_heating = true;
      }
      
if(time_flag == false){ //проверка, что не включен запланированный запуск??? зачем второй флаг для этого???
  if (flag_anti_recursion_temp == false) {  //проверка, что чайник не включен

  if(liquid_level() == true){   //проверка на уровень жидкости
    
      if (server.argName(0) == "boiling") //нажата кнопка "кипячение"
      {
        if (temp < 100) //проверка текущей температуры
        {
          buzzer_on();  //включение пищалки
          bot.sendMessage(chat_id, "Чайник включен. Режим кипячения. Подогрев выключен. Конечная температура: 100", "");
          teapot_heating(100, true);  //нагрев до 100 градусов, подогрев выключен
        } else bot.sendMessage(chat_id, "Запуск чайника отклонен. Текущая температура воды выше 100", "");
      }

      if (server.argName(0) == "heat"){ //нажата кнопка "нагрев"    
        if ((server.arg(0).toInt() <= 100) && (server.arg(0).toInt() >= 25))  //проверка на корректность ввода и температуры
        {
          if (temp < server.arg(0).toInt())
          {
          buzzer_on(); //включение пищалки
          bot.sendMessage(chat_id, "Чайник включен. Режим Нагрева. Подогрев включен. Конечная температура: " + (String)server.arg(0).toInt(), "");
          teapot_heating(server.arg(0).toInt(), false); //нагрев до определенной температуры, подогрев включен
          }else bot.sendMessage(chat_id, "Запуск чайника отклонен. Текущая температура воды выше заданной", "");
        } else bot.sendMessage(chat_id, "Запуск чайника отклонен. Введено некорректное значение температуры. Вводите температуру в диапазоне от 25 до 100!", "");
      }
      } else {
         if (flag_stop_heating == false) bot.sendMessage(chat_id, "Запуск чайника отклонен. Недостаточно воды", "");
}
  }
}
}


void enter_password()
{

  if (server.argName(0) == "password") {
    //enter_password_word = server.arg(0);

    if ((server.arg(0) == guest_password) && (error_schet < 3))
    {
      error_schet = 0;
      bot.sendMessage(chat_id, "Использован код гостя. Доступ разрешен", "");
      flag_error_schet = false;
      servo.write(180);
      delay(1500);
      servo.write(0);

    } else {

      error_schet++;
      if (error_schet > 3) error_schet = 3;
      servo.write(0);
      if (flag_error_schet == false) {
        bot.sendMessage(chat_id, "Неверный ввод. Дверь закрыта. Попыток осталось: " + (String)(3 - error_schet), "");
        if (error_schet == 3) flag_error_schet = true;
      }
    }
  }

  if (server.argName(0) == "admin") {
    //enter_password_word = server.arg(0);

    if (server.arg(0) == admin_password)
    {
      bot.sendMessage(chat_id, "Использован код Администратора. Добро пожаловать, мой Господин", "");
      error_schet = 0;
      flag_error_schet = false;
      servo.write(180);
      delay(1500);
      servo.write(0);
    }
  }

  if (server.argName(0) == "repeat_send_code") {
    bot.sendMessage(chat_id, "Актуальный код Гостя: '" + guest_password + "'", "");
  }

  server.send(200, "", "ok");    // ответить на HTTP запрос
}


void temperature() {
  byte data[2]; // Место для значения температуры

  ds.reset(); // Начинаем взаимодействие со сброса всех предыдущих команд и параметров
  ds.write(0xCC); // Даем датчику DS18b20 команду пропустить поиск по адресу. В нашем случае только одно устрйоство
  ds.write(0x44); // Даем датчику DS18b20 команду измерить температуру. Само значение температуры мы еще не получаем - датчик его положит во внутреннюю память

  delay(20); // Микросхема измеряет температуру, а мы ждем.

  ds.reset(); // Теперь готовимся получить значение измеренной температуры
  ds.write(0xCC);
  ds.write(0xBE); // Просим передать нам значение регистров со значением температуры

  // Получаем и считываем ответ
  data[0] = ds.read(); // Читаем младший байт значения температуры
  data[1] = ds.read(); // А теперь старший

  // Формируем итоговое значение:
  //    - сперва "склеиваем" значение,
  //    - затем умножаем его на коэффициент, соответсвующий разрешающей способности (для 12 бит по умолчанию - это 0,0625)
  temp =  ceil (((data[1] << 8) | data[0]) * 0.0625); //ceil - округление вверх
  
}


void teapot_heating(byte temp_value, boolean mode_teapot) {

  temp_hysteresis = temp_value - 5;
  flag_temp_hysteresis = false;

  flag_anti_recursion_temp = true;
  flag_stop_heating = false;

  temp = temp_value;

  digitalWrite(rele_pin, 1);
  do {
    
  if(liquid_level() == true)
{  
    temperature();
    Serial.println(temp);
    if (((temp_value < temp) || (temp >= 100)) && (mode_teapot == true)) break; //если нажали кипятить

    
    if(flag_stop_heating == true) break;


  if(mode_teapot == false) //если нажали нагрев
  { 
    if(temp >= temp_value) flag_temp_hysteresis = true;

    if((temp > temp_hysteresis) && (flag_temp_hysteresis == true))
    {
      digitalWrite(rele_pin, 0);
      continue;
    } else{
      digitalWrite(rele_pin, 1);
      flag_temp_hysteresis = false;
      continue;
    }
  }

} else {
  bot.sendMessage(chat_id, "Вода закончилась!", "");
  break;
}
server.handleClient();  //проверяем, не пришли ли иные команды
  } while (true);

  
  bot.sendMessage(chat_id, "Чайник выключен. Температура воды: " + (String)temp, "");
  //bot.sendMessage(chat_id, (String)temp, "");

  digitalWrite(rele_pin, 0);
  buzzer_off();
  flag_anti_recursion_temp = false;
}


void lightness() {
server.send(200, "", "ok");    // ответить на HTTP запрос
if ((server.argName(0) == "light_color_red") && (server.argName(1) == "light_color_green") && (server.argName(2) == "light_color_blue") && (server.argName(3) == "brightness")) {
       
LEDS.setBrightness(server.arg(3).toInt());  // ограничить максимальную яркость
LEDS.show();
delay(20);
set_color_to_light(server.arg(0).toInt(), server.arg(1).toInt(), server.arg(2).toInt());
delay(20);
bot.sendMessage(chat_id, "Параметры освещения изменены", "");
}
}


void set_color_to_light(byte red, byte green, byte blue) //процедура раскраски ленты
{  
  
  for (int i = 0 ; i < LED_COUNT; i++ ) leds[i].setRGB( red, green, blue);
  
  LEDS.show();
  
}




void buzzer_on(){ //звуковое сопровождение при включении чайника
  tone(buzzer_pin, 1350, 300);
}


void buzzer_off(){ //звуковое сопровождение при выключении чайника
  tone(buzzer_pin, 650, 50);
  delay(300);
  tone(buzzer_pin, 650, 50);
  delay(300);
  tone(buzzer_pin, 650, 50);
  delay(300);
  tone(buzzer_pin, 800, 50);
  delay(300);
  tone(buzzer_pin, 900, 50);
}


boolean liquid_level() {

boolean flag = digitalRead(liquid_level_pin);
 
  if (flag == false)
  {
    return true;
  } else {
    return false;
  }
}
