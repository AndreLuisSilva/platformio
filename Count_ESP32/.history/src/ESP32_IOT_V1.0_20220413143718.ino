#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <Timestamps.h>
#include <sys/time.h>
#include <time.h>
#include <SPIFFS.h>
#include <FS.h>
#include "esp_task_wdt.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>  // para o Modulo RTC I2C
#include "RTClib.h"
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "EmonLib.h"             // Include Emon Library
#include "nvs_flash.h"

#define JUMPER_CALIBRAGEM 4
#define SCT013 35
#define LED_CALIBRAGEM 15
#define PIN_LED 2
#define RELAY_ON 1
#define RELAY_OFF 0
#define ARRAY_SIZE 60
#define TIME_INTERVAL 1000
#define MAC_ADDRESS "7C:9E:BD:F4:BC:A4"
#define FORMAT_LITTLEFS_IF_FAILED true

const char *ssid = "ForSellEscritorio";
const char *password = "forsell1010";

//Armazena o valor (tempo) da ultima vez que o led foi aceso
unsigned long previous_Millis = 0;

int vetor[60] = { 0 };
//Vetor usado apenas na calibragem
int vetor_Calibragem[60] = { 0 };

int sizeOfRecord = 80;

const char * remote_host = "www.google.com";

//variaveis que indicam o núcleo
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

//flags

int flag_ON_OFF = 0;
bool flag_Valida_Internet = false;

//counts
int count_ON_OFF;
int count_Seconds;
int count_Seconds_Calibragem = 1;
int count_SPIFFS = 0;
int count_Data_On_SPIFFS_Success;
int count_Lines_True = 0;
int count_Lines = 0;
int counter;

//Criação de variáveis Globais
int tempC = 0;
int tempF = 0;
int direction = 0x48;

EnergyMonitor emon1;             // Create an instance

String myFilePath = "/esquadrejadeira.txt";
String mySDCardPath = "/data.txt";
// String que recebe as mensagens de erro
String errorMsg;

String fileName; // Nome do arquivo
File pFile; // Ponteiro do arquivo

//RTClib MyRTC;
RTC_DS3231 rtc;

Preferences preferences;

QueueHandle_t buffer;

TaskHandle_t HANDLE_pisca_Led = NULL;

Timestamps ts(3600); // instantiating object of class Timestamp with an time offset of 0 seconds 

long timezone = -3;
byte daysavetime = 0; // Daylight saving time (horario de verão)

/*Protótipos */
void grava_dado_nvs(uint32_t dado);
void check_ON_OFF();
void TASK_pisca_Led(void *p);

uint8_t Get_NTP(void);
void showFile();
void TASK_Send_POST(void * p);
void listAllFiles();
int count_Lines_SPIFFS();
void send_POST();
void printDirectory(File dir, int numTabs);

uint16_t ano;
uint8_t mes;
uint8_t dia;
uint8_t hora;
uint8_t minuto;
uint8_t segundo;

/*******************************************************************************
* VARIÁVEIS GLOBAIS
*******************************************************************************/
// Próxima verificação de versão
time_t            vcsNextCheck            = 0;

// Dados do arquivo de versão
struct VCS {
  char    swVersion[14];
  bool    swMandatory;
  time_t  swRelease;
  char    swURL[101];
  char    fsVersion[14];
  bool    fsMandatory;
  time_t  fsRelease;
  char    fsURL[101];
};

VCS vcs;

// Classe FS_File_Record e suas funções
class FS_File_Record {
   // Todas as funções desta lib são publicas, mais detalhes em FS_File_Record.cpp
   public:
   FS_File_Record(String, int);
   FS_File_Record(String);
   bool init();
   bool readFileLastRecord(String * , String * );
   bool destroyFile();
   String findRecord(int);
   bool rewind();
   bool writeFile(String, String * );
   bool seekFile(int);
   bool readFileNextRecord(String * , String * );
   String getFileName();
   void setFileName(String);
   int getSizeRecord();
   void setSizeRecord(int);
   void newFile();
   bool fileExists();
   bool availableSpace();
   int getTotalSpace();
   int getUsedSpace();
};

WiFiClient client;
HTTPClient http;
FS_File_Record ObjFS(myFilePath, sizeOfRecord);


//função que o temporizador irá chamar, para reiniciar o ESP32
void IRAM_ATTR resetModule() 
{
   ets_printf("(watchdog) reiniciar\n"); //imprime no log
   esp_restart(); //reinicia o chip
}

void setup() 
{
   Serial.begin(9600);
   Serial.println("");
   Serial.println("");

  // Open Preferences with my-app namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("amperes", false);
  counter = preferences.getUInt("counter", 0);
  Serial.print(counter);
  Serial.println(" A");

  pinMode(JUMPER_CALIBRAGEM, INPUT_PULLUP); //   

  emon1.current(SCT013, 9); // Current: input pin, calibration.  

   esp_task_wdt_init(80, true);
   esp_task_wdt_add(NULL);
   //disableCore0WDT();   

   Serial.println(""); 
   Serial.println(""); 
   WiFi.begin(ssid, password);
   Serial.println("---------------------------------------"); 
   Serial.print("Conectando a rede ");
   Serial.println(ssid);
   while (WiFi.status() != WL_CONNECTED) {
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

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
   while (1);
  }

  Serial.println("---------------------------------------"); 

   if (Get_NTP() == false) {
      // Get time from NTP
      Serial.println("Timeout na conexão com servidor NTP");      
      //ESP.restart();
      Serial.println("Utilizando horario armazenado no RTC: ");      

      //rtc.setClockMode(false);
      DateTime now = rtc.now();      

      dia = now.day();
      mes = now.month();
      ano = now.year();
      hora = now.hour();
      minuto = now.minute();
      segundo = now.second();
      
      Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", dia, mes, ano, hora, minuto, segundo);
      Serial.println("-------------------------");   
      
   } else {
      struct tm timeinfo;
      getLocalTime( & timeinfo);  

      dia = timeinfo.tm_mday;
      mes = timeinfo.tm_mon + 1;
      ano = timeinfo.tm_year + 1900;
      hora = timeinfo.tm_hour;
      minuto = timeinfo.tm_min;
      segundo = timeinfo.tm_sec;     
    
      Serial.println("RTC atualizado pelo servidor NTP.");     
       
      rtc.adjust(DateTime(ano, mes, dia, hora, minuto, segundo));      
      
      Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", dia, mes, ano, hora, minuto, segundo);
      Serial.println("---------------------------------------");  
   }     

    return_Temp();

    Serial.println("---------------------------------------");
    
   init_SD_Card();

   // Se não foi possível iniciar o File System, exibimos erro e reiniciamos o ESP
   if (!ObjFS.init()) 
   {
      Serial.println("Erro no sistema de arquivos");
      delay(500);     
      ESP.restart();
   }   

   // Se o arquivo não existe, criamos o arquivo
   if (!ObjFS.fileExists()) 
   {
      
      delay(500);
      Serial.println("Novo arquivo: ");
      ObjFS.newFile(); // Cria o arquivo  

   }

   // Exibimos mensagem
   Serial.println("---------------------------------------"); 
   Serial.println("SPIFFS Ok!");
   Serial.println("---------------------------------------"); 

   //readFile(myFilePath);
   //digitalWrite(BLUE, HIGH);
   delay(500);
   listAllFiles();
   Serial.println("");
   Serial.println("Iniciando a leitura...");
   Serial.println("");
   //digitalWrite(BLUE, LOW);

   //SPIFFS.remove("/esquadrejadeira.txt");  
   //SD.remove("/data.txt");  

   buffer = xQueueCreate(10, sizeof(uint32_t)); //Cria a queue *buffer* com 10 slots de 4 Bytes    

   //cria a task pisca led para indicar que esta no modo de calibragem
   xTaskCreatePinnedToCore(
      TASK_Check_SCT013_Status, //Função que será executada
      "TASK_Check_SCT013_Status", //Nome da tarefa
      10000,  //Tamanho da pilha
      NULL, //Parâmetro da tarefa (no caso não usamos)
      2,  //Prioridade da tarefa
      NULL, //Caso queria manter uma referência para a tarefa que vai ser criada (no caso não precisamos)
      taskCoreZero); //Número do core que será executada a tarefa (usamos o core 0 para o loop ficar 
      
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

void TASK_pisca_Led(void *p)
{
  esp_task_wdt_delete(NULL);
  while (true)
  {
    digitalWrite(LED_CALIBRAGEM, HIGH);
    vTaskDelay(pdMS_TO_TICKS(250));
    digitalWrite(LED_CALIBRAGEM, LOW);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void TASK_Check_SCT013_Status(void *p)
{
  esp_task_wdt_delete(NULL);

  while (true)
  {
    unsigned long current_Millis = millis();
    //Verifica se o intervalo já foi atingido
    if (current_Millis - previous_Millis >= TIME_INTERVAL)
    {
      previous_Millis = current_Millis;
      count_Seconds++;
      check_ON_OFF();
    }

    vTaskDelay(pdMS_TO_TICKS(10));

  }
}

int return_SCT013_State()
{
  int valor_Calibragem = counter;
  double Irms = emon1.calcIrms(1480); // Calculate Irms only

  if (Irms > valor_Calibragem)
  {
    return 1;
  }
  else if (Irms <= valor_Calibragem)
  {
    return 0;
  }
}

boolean return_Array_SCT013_State()
{
  for (int i = 0; i < ARRAY_SIZE; i++)
  {
    if (vetor[i] == 1)
    {
      count_ON_OFF++;
    }
  }

  if (count_ON_OFF > 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void return_Temp() {
   float tempC = rtc.getTemp1();
   float temp = rtc.getTemperature();
   Serial.print("Temperatura atual: ");    
   Serial.print(tempC, 1);
   Serial.println("ºC");  

   Serial.print("Temperatura: ");    
   Serial.print(temp);
   Serial.println("ºC");  
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

bool validate_Send_POST() {
   //Se tem internet
   IPAddress ip(8, 8, 8, 8);
   bool connected = Ping.ping(ip, 3);
   if (connected) {
      return true;
   } else {
      return false;
   }
   vTaskDelay(pdMS_TO_TICKS(1000));
}

void send_POST_Again() {
   //verifica se existe algum arquivo criado na memoria flash
   if (ObjFS.fileExists()) {
      //verifica se existe conexão com o wifi
      if (WiFi.status() == WL_CONNECTED) {
         //verifica se existe conexão com a internet pingando o servidor do google
         if (validate_Send_POST()) {
            //abre o arquivo dentro da SPIFFS para leitura
            File file = SPIFFS.open(myFilePath, FILE_READ);

            String line = "";            

            while (file.available()) {

               int validate_Break_While = 0;
               line = file.readStringUntil('\n');
               validate_Break_While = re_Send_POST(line);

               if (validate_Break_While == (count_Lines_SPIFFS() + 1)) {
                  count_SPIFFS = 0;
                  listAllFiles();
                  SPIFFS.remove(myFilePath);
                  break;
               }
            }
         }
      }
   }
}

void init_SD_Card() {

    Serial.println("Inicializando o cartão SD...");
 
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(SS)) {
    Serial.println("Falha na inicialização. Verifique se...");
    Serial.println("* tem um cartão inserido no slot?");
    Serial.println("* sua fiação está correta?");
    Serial.println("* você mudou o pino do chip? Selecione o pino correto para funcionar corretamente com o seu shield ou módulo!");  
    while (1);
    
  } else {
    Serial.println("Cartão inserido no slot corretamente.");
  }

   File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("Arquivo ainda não existe");
    Serial.println("Criando arquivo...");   

   file = SD.open("/data.txt", FILE_APPEND);

   // Se foi possível abrir
   if (file) {
      // Escreve registro
      file.println("on_off, mac_address, data_hora \r\n");           
   }
  }
  else {
    Serial.println("Arquivo já existe.");  
  }
  
 
  // print the type of card
  Serial.println();
  Serial.print("Modelo de card: ");
  switch (SD.cardType()) {
    case CARD_NONE:
      Serial.println("Nenhum");
      break;
    case CARD_MMC:
      Serial.println("MMC");
      break;
    case CARD_SD:
      Serial.println("SD");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Desconhecido");
  }  
 
  Serial.print("Tamanho do cartão:  ");
  Serial.println((float)SD.cardSize()/1000);
 
  Serial.print("Total em bytes: ");
  Serial.println(SD.totalBytes());
 
  Serial.print("Bytes utilizados: ");
  Serial.println(SD.usedBytes()); 

  file.close();
}

/*--- LEITURA DO ARQUIVO ---*/
String readFile(String pathFile) {
   Serial.println("- Reading file: " + pathFile);
   SPIFFS.begin(true);
   File rFile = SPIFFS.open(pathFile, "r");
   String values;
   if (!rFile) {
      Serial.println("- Failed to open file.");
   } else {
      while (rFile.available()) {
         values += rFile.readString();
      }

      Serial.println("- File values: " + values);
   }

   rFile.close();
   return values;
}

void grava_Dados_SPIFFS(String values) {
   // Se não houver memória disponível, exibe e reinicia o ESP
   if (!ObjFS.availableSpace()) {
      Serial.println("Memory is full!");
      vTaskDelay(pdMS_TO_TICKS(10000));
      return;
   }

   // Escrevemos no arquivo e exibimos erro ou sucesso na serial para debug
   if (values != "" && !ObjFS.writeFile(values, & errorMsg))
      Serial.println(errorMsg);
   else
      Serial.println("Write ok");
   
   // Exibimos o espaço total, usado e disponível no display, de tempo em tempo
   showAvailableSpace(values);
}

// Construtor que seta somente o nome do arquivo, deixando o tamanho de registro default 3
FS_File_Record::FS_File_Record(String _fileName) {
   fileName = _fileName;
}

// Construtor que seta nome do arquivo e tamanho de registro +2 (\r\n)
FS_File_Record::FS_File_Record(String _fileName, int _sizeOfRecord) {
   fileName = _fileName;
   sizeOfRecord = _sizeOfRecord + 2;
}

// Inicializa SPIFFS
bool FS_File_Record::init() {
   return SPIFFS.begin(true);
}

// Posiciona o ponteiro do arquivo no início
bool FS_File_Record::rewind() {
   if (pFile)
      return pFile.seek(0);

   return false;
}

// Lê a próxima linha do arquivo
bool FS_File_Record::readFileNextRecord(String * line, String * errorMsg) {
   * errorMsg = "";
   * line = "";
   // Se o ponteiro estiver nulo  
   if (!pFile) {
      // Abre arquivo para leitura
      pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

      // Se aconteceu algum erro 
      if (!pFile) {
         // Guarda msg de erro *errorMsg = "Failed to open the file";
         // Retorna falso
         return false;
      }
   }

   // Se for possível ler o arquivo
   if (pFile.available()) {
      // Lê arquivo *line = pFile.readStringUntil('\n');
      // Retorna true
      return true;
   }

   // Se o arquivo estiver vazio retorna true
   if (pFile.size() <= 0)
      return true;

   // Se não for possível ler o arquivo retorna falso    
   return false;
}

//Posiciona ponteiro do arquivo na posição "pos"
bool FS_File_Record::seekFile(int pos) {
   // Se o ponteiro estiver nulo  
   if (pFile)
      pFile.close();

   pFile = SPIFFS.open(fileName.c_str(), FILE_READ); // Abre o arquivo para leitura

   // Posiciona o ponteiro na posição multiplicando pelo tamanho do registro
   return pFile.seek(sizeOfRecord * pos);
}

// Escreve no arquivo
bool FS_File_Record::writeFile(String line, String * errorMsg) {
   if (pFile)
      pFile.close();

   pFile = SPIFFS.open(myFilePath, FILE_APPEND);

   // Se foi possível abrir
   if (pFile) {
      // Escreve registro
      pFile.println(line);
      // Fecha arquivo
      pFile.close();
      // Retorna true
      return true;
   }

   // Se não foi possível abrir guarda mensagem de erro e retorna false *errorMsg = "Failed to open the file: " + String(fileName);
   return false;
}

// Lê o último registro do arquivo
bool FS_File_Record::readFileLastRecord(String * line, String * errorMsg) {
   // Variável que guardará o tamanho do arquivo
   int sizeArq;

   // Limpa string *errorMsg = "";

   // Se o arquivo está aberto, fecha
   if (pFile)
      pFile.close();

   // Abre o arquivo para leitura
   pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

   // Se não foi possível abrir o arquivo
   if (!pFile) {
      // Guarda mensagem de erro e retorna false *errorMsg = "Failed to open the file: " + String(fileName);
      return false;
   }

   // Obtém o tamanho do arquivo
   sizeArq = pFile.size();
   Serial.println("Size: " + String(sizeArq));

   // Se existe ao menos um registro
   if (sizeArq >= sizeOfRecord) {
      pFile.seek(sizeArq - sizeOfRecord); // Posiciona o ponteiro no final do arquivo menos o tamanho de um registro (sizeOfRecord)
      * line = pFile.readStringUntil('\n');
      pFile.close();
   }

   return true;
}

// Exclui arquivo
bool FS_File_Record::destroyFile() {
   // Se o arquivo estiver aberto, fecha
   if (pFile)
      pFile.close();

   // Exclui arquivo e retorna o resultado da função "remove"  
   return SPIFFS.remove((char * ) fileName.c_str());
}

// Função que busca um registro
// "pos" é a posição referente ao registro buscado
String FS_File_Record::findRecord(int pos) {
   // Linha que receberá o valor do registro buscado
   String line = "", errorMsg = "";

   // Posiciona na posição desejada
   // Obs. A posição se inicia com zero "0" 
   if (!seekFile(pos))
      return "Seek error";

   // Lê o registro
   if (!readFileNextRecord( & line, & errorMsg))
      return errorMsg;

   return line;
}

// Verifica se o arquivo existe
bool FS_File_Record::fileExists() {
   return SPIFFS.exists((char * ) fileName.c_str());
}

// Cria um novo arquivo, se já existir um arquivo de mesmo nome, ele será removido antes
void FS_File_Record::newFile() {
   if (pFile)
      pFile.close();

   SPIFFS.remove((char * ) fileName.c_str());
   pFile = SPIFFS.open(fileName.c_str(), FILE_WRITE);
   pFile.close();
}

// Obtém o nome do arquivo
String FS_File_Record::getFileName() {
   return fileName;
}

// Seta o nome do arquivo
void FS_File_Record::setFileName(String _fileName) {
   fileName = _fileName;
}

// Obtém o tamanho do registro
int FS_File_Record::getSizeRecord() {
   return sizeOfRecord - 2;
}

// Seta o tamanho do registro
void FS_File_Record::setSizeRecord(int _sizeOfRecord) {
   sizeOfRecord = _sizeOfRecord + 2;
}

// Verifica se existe espaço suficiente na memória flash
bool FS_File_Record::availableSpace() {
   return getUsedSpace() + sizeOfRecord <= getTotalSpace();
   // ou também pode ser:
   //return getUsedSpace()+(sizeof(char)*sizeOfRecord) <= getTotalSpace();   // sizeof(char) = 1
}

// Retorna o tamanho em bytes total da memória flash
int FS_File_Record::getTotalSpace() {
   return SPIFFS.totalBytes();
}

// Retorna a quantidade usada de memória flash
int FS_File_Record::getUsedSpace() {
   return SPIFFS.usedBytes();
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

uint8_t Get_NTP(void) {
   struct tm timeinfo;

   timeval epoch = {946684800, 0}; // timeval is a struct: {tv_sec, tv_usec}. Old data for detect no replay from NTP. 1/1/2000    
   settimeofday( & epoch, NULL); // Set internal ESP32 RTC

   Serial.println("Entrando em contato com o servidor NTP");
   configTime(3600 * timezone, daysavetime * 3600, "a.st1.ntp.br", "a.ntp.br", "gps.ntp.br"); // initialize the NTP client 
   // using configTime() function to get date and time from an NTP server.

   if (getLocalTime( & timeinfo, 1000) == 0) // transmits a request packet to a NTP server and parse the received 
   {
      // time stamp packet into to a readable format. It takes time structure
      // as a parameter. Second parameter is server replay timeout
      Serial.println("Sem conexão com servidor NTP");
      return false; // Something went wrong
      ESP.restart();
   }

   //Serial.println("Resposta do servidor NTP");
   //Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
   //Serial.println("-------------------------");
   return true; // All OK and go away
}

int count_Lines_SPIFFS() {
   int count = 0;
   String line = "";
   pFile = SPIFFS.open(myFilePath, FILE_READ);
   while (pFile.available()) {
      // we could open the file, so loop through it to find the record we require
      count++;
      //Serial.println(count);  // show line number of SPIFFS file
      line = pFile.readStringUntil('\n'); // Read line by line from the file        
   }

   return count;
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

    //rtc.setClockMode(false);
    long randNumber = random(999999);
    struct tm timeinfo;
    getLocalTime( & timeinfo);  
    
    char data[15];
    char hora[15];
    sprintf(data, "%02d-%02d-%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    sprintf(hora, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    //Serial.print(data);
    //Serial.print(" ");
    //Serial.println(hora);
    
    send_Data_To_IHM(data, hora, flag_ON_OFF);
   
   char logdata[80];
   char csv[80];
   //Verifique o status da conexão WiFi
   if (WiFi.status() == WL_CONNECTED) {

      if (flag_Valida_Internet) { 
                 
         // Especifique o destino para a solicitação HTTP
         http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira_teste.php");
         //http.begin("http://54.207.230.239/site/query_insert_postgres_conexao_madeira.php");
         //http.begin("http://54.207.230.239/site/query_insert_CMDados_paradas.php");
         http.addHeader("Content-Type", "application/x-www-form-urlencoded");
         //sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         //sprintf(csv,"%d,%s,%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         sprintf(csv,"%d,%s,%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         Serial.print("String enviada: ");
         Serial.print(csv);
         Serial.println("--------------------------------------------------------------"); 
         File file = SD.open(mySDCardPath, FILE_APPEND);
         if (file.print(csv)) 
               {                  
                  Serial.println("Mensagem anexada com sucesso no cartão SD!!!");
               } else {                  
                 Serial.print("Falha ao anexar no cartão SD!");
               }
         file.close();  

         Serial.println("--------------------------------------------------------------"); 

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
               pFile = SPIFFS.open(myFilePath, FILE_APPEND);
               if (pFile.print(logdata)) {
                  Serial.println("");
                  Serial.println("Mensagem anexada com sucesso!!!");
               } else {
                  Serial.println("");
                  Serial.print("Falha ao anexar!");
               }

               pFile.close();
            } else {               
               Serial.println("Insert realizado com sucesso!");
            }
         } else {
            //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
            Serial.print("Erro ao enviar POST: ");
            Serial.println(httpResponseCode);
            Serial.println("Salvando dados na memória flash...");
            pFile = SPIFFS.open(myFilePath, FILE_APPEND);
            if (pFile.print(logdata)) {
               Serial.println("Mensagem anexada com sucesso!!!");

            } else {
               Serial.print("Falha ao anexar!");
            }

            pFile.close();
         }
         //flag_Valida_Internet = false;
      } else {
         
         //Se não tiver conexão com o WiFi salva na memória flash
         Serial.println("Sem conexão com a internet!");
         Serial.println("Salvando dados na memória flash...");
         Serial.println("");
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         pFile = SPIFFS.open(myFilePath, FILE_APPEND);
         if (pFile.print(logdata)) {
            Serial.println("Mensagem anexada com sucesso!!!");

         } else {
            Serial.print("Falha ao anexar!");
         }

         pFile.close();
      }
      
   } else {
      
         //Se não tiver conexão com o WiFi salva na memória flash
         Serial.println("Sem conexão com a rede!");
         Serial.println("Salvando dados na memória flash...");
         Serial.println("");
         sprintf(logdata, "on_off=%d&mac_address=%s&data_hora=%04d-%02d-%02d %02d:%02d:%02d.%ld\n", flag_ON_OFF, MAC_ADDRESS, ano, mes, dia, hora, minuto, segundo, randNumber);
         pFile = SPIFFS.open(myFilePath, FILE_APPEND);
         if (pFile.print(logdata)) {
            Serial.println("Mensagem anexada com sucesso!!!");

         } else {
            Serial.print("Falha ao anexar!");
         }

         pFile.close();
      }

      Serial.print("Linhas armazenadas: ");
      Serial.println(count_Lines_SPIFFS());
      Serial.println("--------------------------------------------------------------"); 
      vTaskDelay(pdMS_TO_TICKS(100));
      //timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog) 
   }

boolean return_Array_State() {
   for (int i = 0; i < ARRAY_SIZE; i++) {
      if (vetor[i] == 1) {
         count_ON_OFF++;
      }
   }

   if (count_ON_OFF > 0) {
      return true;
   } else {
      return false;
   }
}

void listAllFiles() {
   String linhas = "";
   int count = 0;
   // Read file content
   File file = SPIFFS.open(myFilePath, FILE_READ);
   Serial.println("");
   Serial.println("                     *********Conteúdo armazenado*********");
   while (file.available()) {
      linhas = file.readStringUntil('\n');
      Serial.println(linhas);
      count++;
      //Serial.write(file.read());
   }

   Serial.println("");
   Serial.print("Quantidade de linhas: ");
   Serial.println(count);
   // Check file size
   Serial.print("Tamanho do arquivo: ");
   Serial.println(file.size());
   file.close();
   Serial.println("");
}

void check_ON_OFF() {  

  if (count_Seconds <= ARRAY_SIZE)
  {
    Serial.print(count_Seconds);
    Serial.print("ª leitura = ");
    Serial.println(return_SCT013_State());
    vetor[count_Seconds] = return_SCT013_State();

  }
  else if (count_Seconds > ARRAY_SIZE)
  {
    count_Seconds = 0;

    Serial.println("--------------------------------------------------------------");
    Serial.print("Estado do rele ");
    if (return_Array_SCT013_State() == 1)
    {
      Serial.println("Ligado");
      Serial.print("Tempo: ");
      Serial.print(count_ON_OFF + 1);
      Serial.println(" seg.");

    }
    else if (return_Array_SCT013_State() == 0)
    {
      Serial.println("Desligado");
      Serial.print("Tempo: ");
      Serial.print(ARRAY_SIZE - count_ON_OFF);
      Serial.println(" seg.");

    }
  }
}

// Exibe o espaço total, usado e disponível no display
void showAvailableSpace(String values) {
   Serial.println("Space: " + String(ObjFS.getTotalSpace()) + " Bytes");
   Serial.println("Used: " + String(ObjFS.getUsedSpace()) + " Bytes");
} //fim da função

void send_Data_To_IHM(char * data, char *  hora, int maquina) {

   for(int i = 0; i < 5; i++) {
   Serial.write(0xff);                            
   Serial.write(0xff);
   Serial.write(0xff);  
   Serial.print("txt_data.txt=");
   Serial.print("\"");
   Serial.print(data);
   Serial.print("\""); 
   Serial.write(0xff);                            
   Serial.write(0xff);
   Serial.write(0xff);  
   Serial.print("txt_hora.txt=");
   Serial.print("\"");
   Serial.print(hora);
   Serial.print("\"");   
   Serial.write(0xff);                            
   Serial.write(0xff);
   Serial.write(0xff);  
   Serial.print("txt_maquina.txt=");
   Serial.print("\"");   
   if(maquina == 1) 
   {   
      Serial.print("LIGADA");
   }
   else 
   {
      Serial.print("DESLIGADA");
   }   
   Serial.print("\""); 
   Serial.write(0xff);                            
   Serial.write(0xff);
   Serial.write(0xff);  
   }
   
}

void loop() {
   vTaskDelay(pdMS_TO_TICKS(10));
   esp_task_wdt_delete(NULL);
}