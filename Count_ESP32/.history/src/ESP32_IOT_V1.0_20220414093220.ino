#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_task_wdt.h"

#define PIN_LED 2
#define MAC_ADDRESS "7C:9E:BD:F4:BC:A4"

const char *ssid = "ForSellEscritorio";
const char *password = "forsell1010";

//variaveis que indicam o núcleo do ESP32 a ser usado
//static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

//flags
int flag_ON_OFF = 0;

//counts
int count_SPIFFS = 0;

QueueHandle_t buffer;

/*Protótipos */
void TASK_Send_POST(void * p);
void send_POST();

/*Objetos */
WiFiClient client;
HTTPClient http;

void setup() 
{
   Serial.begin(9600);
   Serial.println("");
   Serial.println("");

   esp_task_wdt_init(80, true);
   esp_task_wdt_add(NULL);  

   Serial.println(""); 
   Serial.println(""); 
   WiFi.begin(ssid, password);
   Serial.println("---------------------------------------"); 
   Serial.print("Conectando a rede ");
   Serial.println(ssid);

   while (WiFi.status() != WL_CONNECTED) 
   {
      vTaskDelay(pdMS_TO_TICKS(500));
      Serial.print(".");
   }

   Serial.println("");
   Serial.println("---------------------------------------"); 
   Serial.print("Conectado à rede WiFi: ");
   Serial.println(ssid);
   Serial.print("Endereço IP: ");
   Serial.println(WiFi.localIP());
   Serial.print("MAC ADDRESS: ");
   Serial.println(WiFi.macAddress());     

   Serial.println("---------------------------------------");    

   buffer = xQueueCreate(10, sizeof(uint32_t)); //Cria a queue *buffer* com 10 slots de 4 Bytes    
         
   xTaskCreatePinnedToCore(
      TASK_Check_Wifi_Status, 
      "TASK_Check_Wifi_Status", 
      10000,
      NULL, 
      1, 
      NULL, 
      taskCoreOne); 

   xTaskCreatePinnedToCore(
      TASK_Send_POST, 
      "TASK_Send_POST", 
      10000, 
      NULL, 
      2, 
      NULL, 
      taskCoreOne);    

   delay(500); //tempo para a tarefa iniciar 
   
}

void TASK_Check_Wifi_Status(void * p) 
{
   esp_task_wdt_delete(NULL);
   
   while (true) 
   {
      check_Wifi_Connection();
      //ESP_LOGI("TASK_Check_Wifi_Status", "OK");      
      vTaskDelay(pdMS_TO_TICKS(1000)); //Verifica a conexão com o WiFi a cada 1 segundo
   }
}

void TASK_Send_POST(void * p) 
{   
   esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa 
   uint32_t rcv = 0;       //Variavel para controle da queue 

   while (true) 
   {
      if (buffer == NULL) 
      {
         return;
      }

      if (xQueueReceive(buffer, & rcv, portMAX_DELAY) == true) //Se recebeu o valor dentro de 1seg (timeout), mostrara na tela
      {        
         send_POST();
         esp_task_wdt_reset();
         vTaskDelay(pdMS_TO_TICKS(10));  
      }
   }
}

void check_Wifi_Connection() 
{
   if (WiFi.status() == WL_CONNECTED) 
   {

     //Se o WiFi estiver conectado, continua normal

   } else {
      
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) 
      {
         if (WiFi.status() == WL_CONNECTED) 
         {

            break;

         } 

         else 
         {
            Serial.print("Tentando se reconectar a rede Wifi: ");
            Serial.println(ssid);            
            vTaskDelay(pdMS_TO_TICKS(250));            
         }
      }
   }
}

void send_POST() 
{
   Serial.println("Iniciando o envio do POST."); 
   
   //char logdata[80];
   
   //Verifique o status da conexão WiFi
   if (WiFi.status() == WL_CONNECTED) 
   {     
                 
         // Especifique o destino para a solicitação HTTP
         //http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");       
         //http.addHeader("Content-Type", "application/x-www-form-urlencoded");
         //sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
        
         //Serial.print("String enviada: ");       

         //int httpResponseCode = http.POST(logdata); //publica o post

         //verifica se foi possível fazer o insert com post
         //if (httpResponseCode > 0) {
            //String response = http.getString(); //Obtém a resposta do request
            //Serial.println(httpResponseCode); //Printa o código do retorno
            //Serial.println("");
            //Serial.println(response); //Printa a resposta do request
            //Serial.println("");
            
         }   

      Serial.println("--------------------------------------------------------------"); 
      vTaskDelay(pdMS_TO_TICKS(100));
      //timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog) 
} 

void loop() 
{
   vTaskDelay(pdMS_TO_TICKS(10));
   esp_task_wdt_delete(NULL);
}