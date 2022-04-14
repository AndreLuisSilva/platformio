#include <WiFi.h>
#include <HTTPClient.h>

#include <WebServer.h>

#include "esp_task_wdt.h"

#define PIN_LED 2
#define MAC_ADDRESS "7C:9E:BD:F4:BC:A4"

const char *ssid = "ForSellEscritorio";
const char *password = "forsell1010";

//variaveis que indicam o núcleo do ESP32 a ser usado
static uint8_t taskCoreZero = 0;
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

   xTaskCreatePinnedToCore(
      TASK_Send_Data_From_SPIFFS, 
      "TASK_Send_Data_From_SPIFFS", 
      10000, 
      NULL, 
      1, 
      NULL, 
      taskCoreOne); 

   delay(500); //tempo para a tarefa iniciar 
   
}

void TASK_Check_Wifi_Status(void * p) {
   esp_task_wdt_delete(NULL);
   //esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa
   while (true) {
      check_Wifi_Connection();
      ESP_LOGI("TASK_Check_Wifi_Status", "OK");
      //esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(1000));
   }
}

void TASK_Send_Data_From_SPIFFS(void * p) {
   esp_task_wdt_delete(NULL);
   //esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa
   while (true) {
      send_POST_Again();
      ESP_LOGI("TASK_Send_Data_From_SPIFFS", "OK");
      //esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(10));
   }
}

void TASK_Send_POST(void * p) {
   //esp_task_wdt_delete(NULL);
   esp_task_wdt_add(NULL); //Habilita o monitoramento do Task WDT nesta tarefa 
   uint32_t rcv = 0;
   while (true) {
      if (buffer == NULL) {
         return;
      }

      if (xQueueReceive(buffer, & rcv, portMAX_DELAY) == true) //Se recebeu o valor dentro de 1seg (timeout), mostrara na tela
      {
         digitalWrite(PIN_LED, HIGH);
         send_POST();
         esp_task_wdt_reset();
         vTaskDelay(pdMS_TO_TICKS(10));
         digitalWrite(PIN_LED, LOW);

      }
   }
}

void check_Wifi_Connection() {

   if (WiFi.status() == WL_CONNECTED) {
      // WiFi is UP,  do what ever
   } else {
      // wifi down, reconnect here
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
         if (WiFi.status() == WL_CONNECTED) {
            break;
         } else {
            Serial.print("Tentando se reconectar a rede Wifi: ");
            Serial.println(ssid);
            //digitalWrite(GREEN, HIGH);
            //digitalWrite(BLUE, HIGH);
            vTaskDelay(pdMS_TO_TICKS(250));
            //digitalWrite(GREEN, LOW);
            //digitalWrite(BLUE, LOW);
            //vTaskDelay(pdMS_TO_TICKS(250));
         }
      }
   }
}

void send_POST_Again() {
   //verifica se existe algum arquivo criado na memoria flash
  
      //verifica se existe conexão com o wifi
      if (WiFi.status() == WL_CONNECTED) {
         //verifica se existe conexão com a internet pingando o servidor do google
         
            //abre o arquivo dentro da SPIFFS para leitura
           

            String line = "";            

            
         
      }
   
}

void wifi_Reconnect() {
   // Caso retorno da função WiFi.status() seja diferente de WL_CONNECTED
   // entrara na condição de desconexão
   if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Não foi possível conectar ao servidor WEB ou banco de dados");
      // Função que reconectará ao WIFI caso esteja disponível novamente.
      WiFi.reconnect();
      // Se o status de WiFi.status() continuar diferente de WL_CONNECTED
      // vai aguardar 1 segundo até a proxima verificação.
   }
}


//Envia POST novamente
int re_Send_POST(String post) { 
   
      // Especifique o destino para a solicitação HTTP
      http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");      
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      int httpResponseCode = http.POST(post); //publica o post

      //verifica se foi possível fazer o insert com post
      if (httpResponseCode > 0) {

         String response = http.getString(); //Obtém a resposta do request         

         //Afim de garantir que todos os inserts no banco de dados ocorram de forma correta, é iniciado a primeira vez o loop de inserção
         //nesse primeiro momento, as inserções serão somente realizadas e não serão contabilizadas
         //isso significa que desse modo o sistema vai passar pelo loop ao menos duas vezes para garantir a integridade da inserção de todos os dados
         //terminando esse loop, ele sai do while e não deleta o arquivo que contém os dados coletados de forma offline 
         //em seguida ele irá entrar novamente no while, mas dessa vez irá começar a contabilizar as linhas afetadas no banco de dados
         //cada linha que retornar um 0 significa que já foi inserida no banco e assim incrementasse o contador "count_SPIFFS"
         //validando que esse dado realmente já foi inserido no banco de dados ao menos uma vez
         //a SQL de inserção não permite a inserção de dados duplicados e nos retorna o numero de linhas afetadas
         //se existir alguma linha que ainda não tenha sido inserida, ele irá inserir novamente no banco de dados         
         //se isso ocorrer ao finalizar o loop ele irá realizar todo o processo novamente, não excluindo o arquivo e entrando no loop novamente
         //caso o contador de linhas armazenadas na memória flash seja igual ao count_SPIFFS
         //todas as linhas da SPIFFS já foram inseridas ao menos uma vez no banco e assim validando a exclusão do arquivo
         
         if (response.toInt() == 0) {         
            
            count_SPIFFS++;

         } 

      } else {
         //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
         Serial.println("");
         Serial.print("Erro ao Reenviar POST: ");
         Serial.println(httpResponseCode);
         Serial.println("");
      }

      http.end();
   
   //Serial.println("");
   return count_SPIFFS;
}

void send_POST() {
   Serial.println("Iniciando o envio do POST.");   
   //struct tm timeinfo;
   //getLocalTime( & timeinfo); // Get local time
   //aumentar o tamanho do char conforme necessário para o POST   
    
    long randNumber = random(999999);
   
    
    
   
   char logdata[80];
   
   //Verifique o status da conexão WiFi
   if (WiFi.status() == WL_CONNECTED) {     
                 
         // Especifique o destino para a solicitação HTTP
         http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");
         //http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira.php");
         //http.begin("http://54.207.230.239/site/query_insert_CMDados_paradas.php");
         http.addHeader("Content-Type", "application/x-www-form-urlencoded");
         //sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         //sprintf(csv,"%d,%s,%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         //sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         
         Serial.print("String enviada: ");       

         int httpResponseCode = http.POST(logdata); //publica o post

         //verifica se foi possível fazer o insert com post
         if (httpResponseCode > 0) {
            String response = http.getString(); //Obtém a resposta do request
            //Serial.println(httpResponseCode); //Printa o código do retorno
            //Serial.println("");
            //Serial.println(response); //Printa a resposta do request
            //Serial.println("");

            //Se o INSERT no banco não retornar um "OK", salva na memória flash.
            if (response.toInt() != 1) {
               Serial.println("Insert não inserido no banco com sucesso =(");
               Serial.println("Salvando dados na memória flash...");   
            } else {               
               Serial.println("Insert realizado com sucesso!");
            }
         } else {
            //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
            Serial.print("Erro ao enviar POST: ");
            Serial.println(httpResponseCode);
            Serial.println("Salvando dados na memória flash...");        
         }
    
      
      
   } else {
      
         //Se não tiver conexão com o WiFi salva na memória flash
         Serial.println("Sem conexão com a rede!");
         Serial.println("Salvando dados na memória flash...");
         Serial.println("");
         //sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         
      }    
      
      Serial.println("--------------------------------------------------------------"); 
      vTaskDelay(pdMS_TO_TICKS(100));
      //timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog) 
   }



void loop() {
   vTaskDelay(pdMS_TO_TICKS(10));
   esp_task_wdt_delete(NULL);
}