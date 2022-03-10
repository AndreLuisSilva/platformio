#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Timestamps.h>
#include <sys/time.h>
#include <time.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ESP32Ping.h>
#include "esp_task_wdt.h"
#include <Vector.h>
#include <Streaming.h>
#define RELAY_PIN 19
#define SENSOR 18
#define RELAY_ON 1
#define RELAY_OFF 0
#define ARRAY_SIZE 60
#define TIME_INTERVAL 1000
#define MAC_ADDRESS "B8:27:EB:B0:21:80"
#define FORMAT_LITTLEFS_IF_FAILED true

const char *ssid = "ForSellEscritorio";
const char *password = "forsell1010";

//const char *ssid = "Andrew";
//const char *password = "teste123";

//const char *ssid = "Andre Wifi";
//const char *password = "090519911327";

// As seguintes variáveis são unsigned long porque o tempo, medido em milissegundos,
// rapidamente se tornará um número maior do que pode ser armazenado em um int.
unsigned long lastTime = 0;
//timer de 1 minuto
unsigned long timerDelay = 60000;

//Armazena o valor (tempo) da ultima vez que o led foi aceso
unsigned long previous_Millis = 0;

int vetor[60] = {0};

int sizeOfRecord = 80;

// Variável que guarda o último registro obtido
String lastRecord = "";

int relay_State = 0;
//variaveis que indicam o núcleo
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

//flags
int relay_Flag = 0;
int flag_ON_OFF = 0;
int flag_Send_Post = 0;
bool flag_Objects = false;

//counts
int count_ON_OFF;
int count_Seconds;
int count_Data_On_SPIFFS_Success;
int count_SPIFFS = 0;
int count_Lines_True = 0;
int count_Lines = 0;
int count_Objects = 0;

String myFilePath = "/portas.txt";
// String que recebe as mensagens de erro
String errorMsg;

String fileName; // Nome do arquivo
File pFile;      // Ponteiro do arquivo

QueueHandle_t buffer;
Timestamps ts(3600); // instantiating object of class Timestamp with an time offset of 0 seconds

long timezone = -3;
byte daysavetime = 0; // Daylight saving time (horario de verão)

uint8_t Get_NTP(void);
void showFile();
void TASK_Check_Relay_Status(void *p);
void TASK_Send_POST(void *p);
void listAllFiles();
int count_Lines_SPIFFS();
void send_POST();
void send_POST_Again();
void check_Wifi_Connection();

// Classe FS_File_Record e suas funções
class FS_File_Record
{
  // Todas as funções desta lib são publicas, mais detalhes em FS_File_Record.cpp
public:
  FS_File_Record(String, int);
  FS_File_Record(String);
  bool init();
  bool readFileLastRecord(String *, String *);
  bool destroyFile();
  String findRecord(int);
  bool rewind();
  bool writeFile(String, String *);
  bool seekFile(int);
  bool readFileNextRecord(String *, String *);
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

FS_File_Record ObjFS(myFilePath, sizeOfRecord);

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, INPUT_PULLUP);
  pinMode(SENSOR, INPUT);

  esp_task_wdt_init(10, true);
  //esp_task_wdt_add(NULL);
  disableCore0WDT();

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Conectando");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("");
  Serial.print("Conectado à rede WiFi: ");
  Serial.println(ssid);
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  Serial.println("");

  if (Get_NTP() == false)
  {
    // Get time from NTP
    Serial.println("Timeout na conexão com servidor NTP");
  }

  // Se não foi possível iniciar o File System, exibimos erro e reiniciamos o ESP
  if (!ObjFS.init())
  {
    Serial.println("Erro no sistema de arquivos");
    delay(1000);
    ESP.restart();
  }

  // Exibimos mensagem
  Serial.println("Sistema de arquivos ok");

  // Se o arquivo não existe, criamos o arquivo
  if (!ObjFS.fileExists())
  {
    Serial.println("Novo arquivo: ");
    ObjFS.newFile(); // Cria o arquivo
  }

  //readFile(myFilePath);
  listAllFiles();
  Serial.println("");
  Serial.println("Iniciando a leitura...");

  //SPIFFS.remove("/esquadrejadeira.bin");

  //buffer = xQueueCreate(10, sizeof(uint32_t));  //Cria a queue *buffer* com 10 slots de 4 Bytes
  buffer = xQueueCreate(10, sizeof(char *)); //Cria a queue *buffer* com 10 slots de char

  xTaskCreatePinnedToCore(
      TASK_Check_Relay_Status,   /*função que implementa a tarefa */
      "TASK_Check_Relay_Status", /*nome da tarefa */
      10000,                     /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                      /*parâmetro de entrada para a tarefa (pode ser NULL) */
      2,                         /*prioridade da tarefa (0 a N) */
      NULL,                      /*referência para a tarefa (pode ser NULL) */
      taskCoreZero);             /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
      TASK_Check_Wifi_Status,   /*função que implementa a tarefa */
      "TASK_Check_Wifi_Status", /*nome da tarefa */
      10000,                    /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                     /*parâmetro de entrada para a tarefa (pode ser NULL) */
      1,                        /*prioridade da tarefa (0 a N) */
      NULL,                     /*referência para a tarefa (pode ser NULL) */
      taskCoreOne);             /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
      TASK_Send_POST,   /*função que implementa a tarefa */
      "TASK_Send_POST", /*nome da tarefa */
      10000,            /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,             /*parâmetro de entrada para a tarefa (pode ser NULL) */
      2,                /*prioridade da tarefa (0 a N) */
      NULL,             /*referência para a tarefa (pode ser NULL) */
      taskCoreOne);     /*Núcleo que executará a tarefa */

  xTaskCreatePinnedToCore(
      TASK_Send_Data_From_SPIFFS,   /*função que implementa a tarefa */
      "TASK_Send_Data_From_SPIFFS", /*nome da tarefa */
      10000,                        /*número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                         /*parâmetro de entrada para a tarefa (pode ser NULL) */
      1,                            /*prioridade da tarefa (0 a N) */
      NULL,                         /*referência para a tarefa (pode ser NULL) */
      taskCoreOne);                 /*Núcleo que executará a tarefa */

  delay(500); //tempo para a tarefa iniciar
}

void watchdog_task(void *pvParameters)
{
  /* kick watchdog every 1 second */
  for (;;)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
}

void TASK_Check_Wifi_Status(void *p)
{
  esp_task_wdt_delete(NULL);
  while (true)
  {
    //esp_task_wdt_reset();
    check_Wifi_Connection();
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI("TASK_Check_Wifi_Status", "OK");
  }
}

void TASK_Send_Data_From_SPIFFS(void *p)
{

  while (true)
  {
    //esp_task_wdt_reset();
    send_POST_Again();
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI("TASK_Send_Data_From_SPIFFS", "OK");
  }
}

void TASK_Send_POST(void *p)
{
  esp_task_wdt_delete(NULL);
  char * myReceivedItem;
  while (true)
  {
    if (buffer == NULL)
      return;

    if (xQueueReceive(buffer, &myReceivedItem, portMAX_DELAY) == true) //Se recebeu o valor dentro de 1seg (timeout), mostrara na tela
    {
      //send_POST(myReceivedItem);
      //free(myReceivedItem);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void TASK_Check_Relay_Status(void *p)
{
  esp_task_wdt_add(NULL);

  while (true)
  {
    esp_task_wdt_reset();

    if (digitalRead(RELAY_PIN))
    {

      flag_Objects = true;
    }

    if (!digitalRead(RELAY_PIN) && flag_Objects == true)
    {

      flag_Objects = false;
      count_Objects++;
      Serial.print("Contagem de objetos detectados: ");
      Serial.println(count_Objects);
      struct tm timeinfo;
      getLocalTime(&timeinfo); // Get local time
      long randNumber = random(999999);
      char myData[150];
      sprintf(myData, "%04d-%02d-%02d %02d:%02d:%02d.%ld", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, randNumber);
      char *myItem = (char *)malloc(strlen(myData) + 1);
      strcpy(myItem, myData);
      Serial.print("Data/Hora da detecção do objeto: ");
      Serial.println(myItem);
      xQueueSend(buffer, &myItem, pdMS_TO_TICKS(0));
    }

  
    vTaskDelay(pdMS_TO_TICKS(100));
  }
 } 


bool hasInternet()
{
  WiFiClient client;
  //Endereço IP do Google 172.217.3.110
  IPAddress adr = IPAddress(8, 8, 4, 4);
  //Tempo limite para conexão
  client.setTimeout(5);
  //Tenta conectar
  bool connected = client.connect(adr, 80);
  //Fecha a conexão
  client.stop();
  //Retorna true se está conectado ou false se está desconectado
  return connected;
}

bool check_Ping()
{
  bool success = Ping.ping("www.google.com", 3);

  if (!success)
  {
    //Serial.println("Ping failed");
    return false;
  }

  return true;
  //Serial.println("Ping successful.");
}

void check_Wifi_Connection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.print("Conectado novamente a rede Wifi: ");
        Serial.println(ssid);
        break;
      }
    }
  }
}

void send_POST_Again()
{
  if (ObjFS.fileExists())
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (hasInternet())
      {
        File file = SPIFFS.open(myFilePath, FILE_READ);

        String line = "";

        while (file.available())
        {
          int validate_Break_While = 0;
          line = file.readStringUntil('\n');
          validate_Break_While = re_Send_POST(line);
          if (validate_Break_While == (count_Lines_SPIFFS() + 1))
          {
            count_SPIFFS = 0;
            listAllFiles();
            SPIFFS.remove(myFilePath);
            break;
          }
        }
      }
      else
      {
        Serial.print("");
      }
    }
  }
}

/*--- LEITURA DO ARQUIVO ---*/
String readFile(String pathFile)
{
  Serial.println("- Reading file: " + pathFile);
  SPIFFS.begin(true);
  File rFile = SPIFFS.open(pathFile, "r");
  String values;
  if (!rFile)
  {
    Serial.println("- Failed to open file.");
  }
  else
  {
    while (rFile.available())
    {
      values += rFile.readString();
    }

    Serial.println("- File values: " + values);
  }

  rFile.close();
  return values;
}

void grava_Dados_SPIFFS(String values)
{
  // Se não houver memória disponível, exibe e reinicia o ESP
  if (!ObjFS.availableSpace())
  {
    Serial.println("Memory is full!");
    delay(10000);
    return;
  }

  // Escrevemos no arquivo e exibimos erro ou sucesso na serial para debug
  if (values != "" && !ObjFS.writeFile(values, &errorMsg))
    Serial.println(errorMsg);
  else
    Serial.println("Write ok");

  // Atribui para a variável global a última amostra
  lastRecord = values;
  // Exibe a última amostra no display
  showLastRecord();
  // Exibimos o espaço total, usado e disponível no display, de tempo em tempo
  showAvailableSpace(values);
}

// Construtor que seta somente o nome do arquivo, deixando o tamanho de registro default 3
FS_File_Record::FS_File_Record(String _fileName)
{
  fileName = _fileName;
}

// Construtor que seta nome do arquivo e tamanho de registro +2 (\r\n)
FS_File_Record::FS_File_Record(String _fileName, int _sizeOfRecord)
{
  fileName = _fileName;
  sizeOfRecord = _sizeOfRecord + 2;
}

// Inicializa SPIFFS
bool FS_File_Record::init()
{
  return SPIFFS.begin(true);
}

// Posiciona o ponteiro do arquivo no início
bool FS_File_Record::rewind()
{
  if (pFile)
    return pFile.seek(0);

  return false;
}

// Lê a próxima linha do arquivo
bool FS_File_Record::readFileNextRecord(String *line, String *errorMsg)
{
  *errorMsg = "";
  *line = "";
  // Se o ponteiro estiver nulo
  if (!pFile)
  {
    // Abre arquivo para leitura
    pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

    // Se aconteceu algum erro
    if (!pFile)
    {
      // Guarda msg de erro *errorMsg = "Failed to open the file";
      // Retorna falso
      return false;
    }
  }

  // Se for possível ler o arquivo
  if (pFile.available())
  {
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
bool FS_File_Record::seekFile(int pos)
{
  // Se o ponteiro estiver nulo
  if (pFile)
    pFile.close();

  pFile = SPIFFS.open(fileName.c_str(), FILE_READ); // Abre o arquivo para leitura

  // Posiciona o ponteiro na posição multiplicando pelo tamanho do registro
  return pFile.seek(sizeOfRecord * pos);
}

// Escreve no arquivo
bool FS_File_Record::writeFile(String line, String *errorMsg)
{
  if (pFile)
    pFile.close();

  pFile = SPIFFS.open(myFilePath, FILE_APPEND);

  // Se foi possível abrir
  if (pFile)
  {
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
bool FS_File_Record::readFileLastRecord(String *line, String *errorMsg)
{
  // Variável que guardará o tamanho do arquivo
  int sizeArq;

  // Limpa string *errorMsg = "";

  // Se o arquivo está aberto, fecha
  if (pFile)
    pFile.close();

  // Abre o arquivo para leitura
  pFile = SPIFFS.open(fileName.c_str(), FILE_READ);

  // Se não foi possível abrir o arquivo
  if (!pFile)
  {
    // Guarda mensagem de erro e retorna false *errorMsg = "Failed to open the file: " + String(fileName);
    return false;
  }

  // Obtém o tamanho do arquivo
  sizeArq = pFile.size();
  Serial.println("Size: " + String(sizeArq));

  // Se existe ao menos um registro
  if (sizeArq >= sizeOfRecord)
  {
    pFile.seek(sizeArq - sizeOfRecord); // Posiciona o ponteiro no final do arquivo menos o tamanho de um registro (sizeOfRecord)
    *line = pFile.readStringUntil('\n');
    pFile.close();
  }

  return true;
}

// Exclui arquivo
bool FS_File_Record::destroyFile()
{
  // Se o arquivo estiver aberto, fecha
  if (pFile)
    pFile.close();

  // Exclui arquivo e retorna o resultado da função "remove"
  return SPIFFS.remove((char *)fileName.c_str());
}

// Função que busca um registro
// "pos" é a posição referente ao registro buscado
String FS_File_Record::findRecord(int pos)
{
  // Linha que receberá o valor do registro buscado
  String line = "", errorMsg = "";

  // Posiciona na posição desejada
  // Obs. A posição se inicia com zero "0"
  if (!seekFile(pos))
    return "Seek error";

  // Lê o registro
  if (!readFileNextRecord(&line, &errorMsg))
    return errorMsg;

  return line;
}

// Verifica se o arquivo existe
bool FS_File_Record::fileExists()
{
  return SPIFFS.exists((char *)fileName.c_str());
}

// Cria um novo arquivo, se já existir um arquivo de mesmo nome, ele será removido antes
void FS_File_Record::newFile()
{
  if (pFile)
    pFile.close();

  SPIFFS.remove((char *)fileName.c_str());
  pFile = SPIFFS.open(fileName.c_str(), FILE_WRITE);
  pFile.close();
}

// Obtém o nome do arquivo
String FS_File_Record::getFileName()
{
  return fileName;
}

// Seta o nome do arquivo
void FS_File_Record::setFileName(String _fileName)
{
  fileName = _fileName;
}

// Obtém o tamanho do registro
int FS_File_Record::getSizeRecord()
{
  return sizeOfRecord - 2;
}

// Seta o tamanho do registro
void FS_File_Record::setSizeRecord(int _sizeOfRecord)
{
  sizeOfRecord = _sizeOfRecord + 2;
}

// Verifica se existe espaço suficiente na memória flash
bool FS_File_Record::availableSpace()
{
  return getUsedSpace() + sizeOfRecord <= getTotalSpace();
  // ou também pode ser:
  //return getUsedSpace()+(sizeof(char)*sizeOfRecord) <= getTotalSpace();   // sizeof(char) = 1
}

// Retorna o tamanho em bytes total da memória flash
int FS_File_Record::getTotalSpace()
{
  return SPIFFS.totalBytes();
}

// Retorna a quantidade usada de memória flash
int FS_File_Record::getUsedSpace()
{
  return SPIFFS.usedBytes();
}

void wifi_Reconnect()
{
  // Caso retorno da função WiFi.status() seja diferente de WL_CONNECTED
  // entrara na condição de desconexão
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Não foi possível conectar ao servidor WEB ou banco de dados");
    // Função que reconectará ao WIFI caso esteja disponível novamente.
    WiFi.reconnect();
    // Se o status de WiFi.status() continuar diferente de WL_CONNECTED
    // vai aguardar 1 segundo até a proxima verificação.
  }
}

uint8_t Get_NTP(void)
{
  struct tm timeinfo;

  timeval epoch = {946684800, 0}; // timeval is a struct: {tv_sec, tv_usec}. Old data for detect no replay from NTP. 1/1/2000
  settimeofday(&epoch, NULL);     // Set internal ESP32 RTC

  Serial.println("Entrando em contato com o servidor NTP");
  configTime(3600 * timezone, daysavetime * 3600, "a.st1.ntp.br", "a.ntp.br", "gps.ntp.br"); // initialize the NTP client
  // using configTime() function to get date and time from an NTP server.

  if (getLocalTime(&timeinfo, 1000) == 0) // transmits a request packet to a NTP server and parse the received
  {
    // time stamp packet into to a readable format. It takes time structure
    // as a parameter. Second parameter is server replay timeout
    Serial.println("Sem conexão com servidor NTP");
    return false; // Something went wrong
    ESP.restart();
  }

  Serial.println("Resposta do servidor NTP");
  Serial.printf("Agora: %02d-%02d-%04d %02d:%02d:%02d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.println("-------------------------");
  return true; // All OK and go away
}

int count_Lines_SPIFFS()
{
  int count = 0;
  String line = "";
  pFile = SPIFFS.open(myFilePath, FILE_READ);
  while (pFile.available())
  {
    // we could open the file, so loop through it to find the record we require
    count++;
    //Serial.println(count);  // show line number of SPIFFS file
    line = pFile.readStringUntil('\n'); // Read line by line from the file
  }

  return count;
}

//Envia POST novamente
int re_Send_POST(String post)
{
  Serial.println("Iniciando o reenvio do POST.");

  WiFiClient client;
  HTTPClient http;

  // Especifique o destino para a solicitação HTTP
  http.begin("http://54.207.230.239/site/query_insert_postgres_count_doors.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpResponseCode = http.POST(post); //publica o post

  //verifica se foi possível fazer o insert com post
  if (httpResponseCode > 0)
  {
    String response = http.getString(); //Obtém a resposta do request
    //Serial.println(httpResponseCode); //Printa o código do retorno
    Serial.println(response); //Printa a resposta do request
    //Serial.println("");

    //Se o INSERT no banco não retornar um "OK", salva na memória flash.
    if (response != "OK")
    {
      Serial.println("");
      Serial.println("Insert não inserido no banco com sucesso =(");
    }
    else
    {
      Serial.println("");
      Serial.println("Insert realizado com sucesso!");
      count_SPIFFS++;
    }
  }
  else
  {
    //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
    Serial.println("");
    Serial.print("Erro ao enviar POST: ");
    Serial.println(httpResponseCode);
  }

  http.end();

  Serial.println("");
  return count_SPIFFS;
}

void send_POST(char *data_hora)
{
  Serial.println("Iniciando o envio do POST.");

  char logdata[150];
  //Verifique o status da conexão WiFi
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    // Especifique o destino para a solicitação HTTP
    http.begin("http://54.207.230.239/site/query_insert_postgres_count_doors.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    sprintf(logdata, "mac_address=%s&data_hora=%s\n", MAC_ADDRESS, data_hora);
    Serial.print("String a ser enviada no POST: ");
    Serial.print(logdata);
    int httpResponseCode = http.POST(logdata); //publica o post

    //verifica se foi possível fazer o insert com post
    if (httpResponseCode > 0)
    {
      String response = http.getString(); //Obtém a resposta do request

      //Se o INSERT no banco não retornar um "OK", salva na memória flash.
      if (response != "OK")
      {
        Serial.println("Insert não inserido no banco com sucesso =(");
        Serial.println("Salvando dados na memória flash...");
        pFile = SPIFFS.open(myFilePath, FILE_APPEND);
        if (pFile.print(logdata))
        {
          Serial.println("");
          Serial.println("Mensagem anexada com sucesso!!!");
        }
        else
        {
          Serial.println("");
          Serial.print("Falha ao anexar!");
        }

        pFile.close();
      }
      else
      {
        Serial.println("");
        Serial.println("Insert realizado com sucesso!");
        Serial.println(response);
      }
    }
    else
    {
      //Se acontecer algum outro tipo de erro ao enviar o POST, salva na memória flash.
      Serial.println("");
      Serial.print("Erro ao enviar POST: ");
      Serial.println(httpResponseCode);
      Serial.println("Salvando dados na memória flash...");
      pFile = SPIFFS.open(myFilePath, FILE_APPEND);
      if (pFile.print(logdata))
      {
        Serial.println("Mensagem anexada com sucesso!!!");
      }
      else
      {
        Serial.print("Falha ao anexar!");
      }

      pFile.close();
    }
  }
  else
  {
    //Se não tiver conexão com o WiFi salva na memória flash
    Serial.println("Sem conexão com a rede Wireless!");
    Serial.println("Salvando dados na memória flash...");
    Serial.println("");
    sprintf(logdata, "mac_address=%s&data_hora=%s\n", MAC_ADDRESS, data_hora);
    pFile = SPIFFS.open(myFilePath, FILE_APPEND);
    if (pFile.print(logdata))
    {
      Serial.println("Mensagem anexada com sucesso!!!");
    }
    else
    {
      Serial.print("Falha ao anexar!");
    }

    pFile.close();
  }
  Serial.println("");
}

void listAllFiles()
{
  String linhas = "";
  int count = 0;
  // Read file content
  File file = SPIFFS.open(myFilePath, FILE_READ);
  Serial.println("");
  Serial.println("              *********Conteúdo armazenado*********");
  while (file.available())
  {
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

// Exibe última amostra de temperatura e umidade obtida
void showLastRecord()
{
  Serial.println("Last record:");
  Serial.println(lastRecord);
}

// Exibe o espaço total, usado e disponível no display
void showAvailableSpace(String values)
{
  Serial.println("Space: " + String(ObjFS.getTotalSpace()) + " Bytes");
  Serial.println("Used: " + String(ObjFS.getUsedSpace()) + " Bytes");
} //fim da função

void loop() {}
