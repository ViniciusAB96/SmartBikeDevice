/*
Autor: Vinícius de Andrade Barros
Project: SmartBike
The pursuit of a dream that will come true.
*/
/*  
  ESP8266 ESP-12E module -> NEO-6M GY-GPS6MV2 GPS module
  VV (5V)     VCC
  GND         GND
  D1 (GPIO5)  RX
  D2 (GPIO4)  TX
 */
//*************************
#include <TinyGPS++.h>          // include TinyGPS++ library
#include <SoftwareSerial.h>     // include Software serial library
#include <ESP8266WiFi.h>        // include WiFi library
#include <ESP8266HTTPClient.h> 
//*************************

#define RELE 2 //D4 == GPIO02

TinyGPSPlus gps;    // The TinyGPS++ object
SoftwareSerial ss(4, 5);         // configure SoftSerial library (RX pin, TX pin)
WiFiClient espClient;
HTTPClient http;


const char* orionAddressPath = "{Endereço do Orion}";
//Meu Device
const char* deviceID = "{Código da bicicleta}";

//Wi-fi Credentials
//*************************
const char* ssid =   "{ID da rede}"; 
const char* password = "{Senha da Rede}";
//*************************


float lati, longi, vel, batteryLevel;
String Latitude, Longitude, speed1;
boolean locked = true;
/***********************
 *  WarmUp variables
 ***********************/
float latitude , longitude;
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str;
int pm;
//***************************

void setup()
{
    pinMode(RELE, OUTPUT);  
    digitalWrite(RELE, LOW);

    Serial.begin(115200);      // initialise serial communication at 9600 bps
    ss.begin(9600);  // initialize software serial at 9600 bps
    Serial.println();         // print empty line
    Serial.print("Connecting to... "); // print text in Serial Monitor
    Serial.println(ssid);           // print text in Serial Monitor
    WiFi.begin(ssid, password);     // connect to Wi-Fi network with SSID and password
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    
    // print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("Server started");
    Serial.println(WiFi.localIP());
    
    //creando um device no Helix SandBox (plug&play)
    Serial.println("Creating" + String(deviceID) + " entitie...");
    delay(200);
    orionCreateEntitie(deviceID);
    delay(200);
    warmUPGPS();
}

void loop()
{
  batteryLevel = 0.80;
    while (ss.available() > 0)
    {               
       //ReadFromGPSModule();
       if(gps.encode(ss.read()))
       {
          if(gps.location.isValid())
          {
              lati = gps.location.lat();
              Latitude = String(lati, 6);
              longi = gps.location.lng();
              Longitude = String(longi, 6);
              vel = gps.speed.kmph();
              speed1 = String(vel,2);
              Serial.println("DeviceID: "+String(deviceID)+" Latitude: "+Latitude+" Longitude: "+Longitude + "Speed: "+speed1);
          }
          if((Latitude != "" && Longitude != "") || speed1 != "")
          {
              //update 
              Serial.println("Updating data in orion...");
              Serial.println("DeviceID: "+String(deviceID)+" Latitude: "+Latitude+" Longitude: "+Longitude + "Speed: "+speed1 + "BaterryLevel: " + String(batteryLevel));
              orionUpdate(deviceID, Latitude, Longitude, speed1, String(batteryLevel));  
              Serial.println("Finished updating data in orion...");  
              Latitude = "";
              Longitude = "";
              speed1 = "";
          }  
          AvaliaBikeLocked(deviceID);     
          ActiveLocked();       
          delay(5000);                      
       }           
    }
    delay(100);
}

//Criação da entidade no HELIX
void orionCreateEntitie(String entitieName){
    //String bodyRequest = "{\"id\": \"" + entitieName + "\",\"type\": \"iot\",\"location\": {\"value\": { \"type\": \"Point\",\"coordinates\": [2.186447514, 41.3763726]},\"type\": \"geo:json\"},\"speed\": {\"type\": \"float\",\"value\": 0},\"locked\": {\"type\": \"boolean\",\"value\": true}}";
    String bodyRequest = "{\"id\": \"" + entitieName + "\",\"type\": \"iot\",\"location\": {\"value\": { \"type\": \"Point\",\"coordinates\": [2.186447514, 41.3763726]},\"type\": \"geo:json\"},\"speed\": {\"type\": \"float\",\"value\": 0},\"batteryLevel\": {\"type\": \"Property\",\"value\": 0.75},\"locked\": {\"type\": \"boolean\",\"value\": true}}";
    httpRequest("/entities/", bodyRequest, 0);    
}

//update --> Aqui irei realizar o updade tanto da velocidade como da longitude / latitude.
void orionUpdate(String entitieID, String latitude, String longitude, String velocidade, String bateryCharge)
{
    //Primeiramente vou atualizar a localização.    
    String bodyRequest = "{\"type\": \"Point\", \"coordinates\": ["+latitude+","+longitude+"]}";
    String pathRequest = "/entities/" + entitieID + "/attrs/location/value";     
    httpRequest(pathRequest, bodyRequest, 1); 
    
    //Posteriormente vou atualizar a velocidade.
    bodyRequest = velocidade;
    pathRequest = "/entities/" + entitieID + "/attrs/speed/value";     
    httpRequest(pathRequest, bodyRequest, 2);   
    
    //Posteriormente vou atualizar o nível da bateria.
    bodyRequest = bateryCharge;
    pathRequest = "/entities/" + entitieID + "/attrs/batteryLevel/value";     
    httpRequest(pathRequest, bodyRequest, 2); 
    
}

//avalia se a bicicleta está bloqueada ou não;
void AvaliaBikeLocked(String entitieID)
{
    String bodyRequest = "";
    String pathRequest = "/entities/" + entitieID + "/attrs/locked/value";     
    httpRequest(pathRequest, bodyRequest, 3);   
}

//request
void httpRequest(String path, String body, int type)
{   
    String payload = makeRequest(String(orionAddressPath), path, body, type);      
    delay(1000);
    if(!payload)
    {    
        return;
    }
    else if(body == "")
    {
       // Serial.println(payload);
        if(payload == "true" || payload == "True")
        {
          locked = true;
        }
        else
        {
           locked = false; 
        }    
    }
}

String makeRequest(String orionAddress, String path, String body, int type)
{
    int httpCode;
    String fullAddress = "http://" + orionAddress + path;  
    http.begin(fullAddress);
    Serial.println("Orion URI request: " + fullAddress);
    
    prepareHeader(type);
    if(type == 0)
    {
        Serial.println(body);
        httpCode = http.POST(body);
    }
    else if (type == 1 || type == 2)
    {
        Serial.println(body);
        httpCode = http.PUT(body);
    }
    else
    {
        httpCode = http.GET();
    }  
    String response =  http.getString();  
    Serial.println("HTTP CODE");
    Serial.println(httpCode);
        
    if (httpCode < 0) {
        Serial.println("request error - " + httpCode);
        return "";
    }
    if (httpCode != HTTP_CODE_OK) {
        return "";
    }  
    http.end();
    return response;
}

void prepareHeader(int type)
{
    http.addHeader("fiware-service", "helixiot"); 
    http.addHeader("fiware-servicepath", "/");
    
    //Post
    if(type == 0)
    {    
        http.addHeader("Content-Type", "application/json"); 
    }  
    
    //Put JSON
    if (type == 1)
    {
        http.addHeader("Content-Type", "application/json");    
        http.addHeader("Accept", "application/json"); 
    }  
    //Put plain Text
    if (type == 2)
    {
        http.addHeader("Content-Type", "text/plain");
    }  
    //GET
    if(type != 0 && type != 1 && type != 2)
    {
        http.addHeader("Accept", "text/plain"); 
    }
}


void ActiveLocked()
{
    if(locked)
    { 
        Serial.println("Locked: " + String(locked));
        digitalWrite(RELE, LOW);
    }
    else
    {
        Serial.println("Else - Locked: " + String(locked));
        digitalWrite(RELE, HIGH);
    }            
    delay(1000);  
}



void warmUPGPS()
{
  while(true)
  {
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read()))
    {
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        lat_str = String(latitude , 6);
        longitude = gps.location.lng();
        lng_str = String(longitude , 6);
      }

      if (gps.date.isValid())
      {
        date_str = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();

        if (date < 10)
          date_str = '0';
        date_str += String(date);

        date_str += " / ";

        if (month < 10)
          date_str += '0';
        date_str += String(month);

        date_str += " / ";

        if (year < 10)
          date_str += '0';
        date_str += String(year);
      }

      if (gps.time.isValid())
      {
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();

        minute = (minute + 30);
        if (minute > 59)
        {
          minute = minute - 60;
          hour = hour + 1;
        }
        hour = (hour + 5) ;
        if (hour > 23)
          hour = hour - 24;

        if (hour >= 12)
          pm = 1;
        else
          pm = 0;

        hour = hour % 12;

        if (hour < 10)
          time_str = '0';
        time_str += String(hour);

        time_str += " : ";

        if (minute < 10)
          time_str += '0';
        time_str += String(minute);

        time_str += " : ";

        if (second < 10)
          time_str += '0';
        time_str += String(second);

        if (pm == 1)
          time_str += " PM ";
        else
          time_str += " AM ";

      }
       Serial.println("Latitude: " + lat_str);
       Serial.println("Longitude: " + lng_str);
       if(lat_str != "" && lng_str != "" )
       {
          return;
       }
    }
  }
  delay(100);
  }
}
