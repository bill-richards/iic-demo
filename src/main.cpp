//#define SLAVE
#define ALTERNATE_SLAVE

#include <Arduino.h>
#include <Wire.h>

#ifdef SLAVE
#include <WireSlave.h>
#else
#include <WireSlaveRequest.h>
#endif

#ifdef SLAVE
#ifdef ALTERNATE_SLAVE
static const uint8_t SLAVE_ADDRESS    = 0x09;
#else
static const uint8_t SLAVE_ADDRESS    = 0x4C;
#endif
#else
static const int _iicAddresses[2]     = { 0x09, 0x4C };
#endif

static const uint8_t SDA_PIN          = 21;
static const uint8_t SCL_PIN          = 22;
static const size_t  PACKET_LENGTH    = 250;

static const uint32_t KILOBYTE_1      = 1024;
static const uint32_t KILOBYTE_2      = 2048;
static const uint32_t KILOBYTE_3      = 3072;

static const unsigned long BAUD_RATE  = 115200;

#ifdef SLAVE
const TickType_t xSleepPeriod = pdMS_TO_TICKS(1000);
const QueueHandle_t xMessageQueue = xQueueCreate(1000, sizeof(char));
#else
const TickType_t xSleepPeriod = pdMS_TO_TICKS(1000);
const QueueHandle_t xMessageQueue = xQueueCreate(2000, sizeof(char));
#endif

/////////////////////////////////////////////
void Message_Task(void * pvParameters);
void I2C_Task(void * params);
void RequestMade();
const char message[] = "1234567 10 234567 20 234567 30 234567 40 234567 50 234567 60 234567 70 234567 80 234567 90 23456 100 23456 110 23456 120 23456 130 23456 140 23456 150 23456 160 23456 170 23456 180 23456 190 23456 200 23456 210 23456 220 23456 230 23456 240 23456 250";
/////////////////////////////////////////////

void setup() 
{
  Serial.begin(BAUD_RATE);

#ifdef SLAVE
  WireSlave.begin(SDA_PIN, SCL_PIN, SLAVE_ADDRESS);
  WireSlave.onRequest(RequestMade);
  xTaskCreate(Message_Task, "Message-Task", KILOBYTE_1, NULL, 5, NULL);
#else    
  Wire.begin(SDA_PIN, SCL_PIN);
  xTaskCreate(I2C_Task, "I2C-Task", KILOBYTE_2, NULL, 5, NULL);
  vTaskDelay(pdMS_TO_TICKS(150));
  xTaskCreate(Message_Task, "Message-Task", KILOBYTE_3, NULL, 5, NULL);
#endif

}

#ifndef SLAVE

void I2C_Task(void * pvParameters)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  int counter = 1;

  while(true)
  {
    for (const int& address : _iicAddresses)
    {
      WireSlaveRequest request(Wire, address, PACKET_LENGTH);
      request.setRetryDelay(5);
      counter = 1;

      do
      {
        if(request.request())
        {
          int index = 0;
          while(request.available())
          {
            char currentCharacter = (char)request.read();
            xQueueSendToBack(xMessageQueue, &currentCharacter, 0);
            ++index;
          }
        } 

        ++counter;
      } while (counter <= 4);
    }

    vTaskDelayUntil(&xLastWakeTime, xSleepPeriod);
  }
}

void Message_Task(void * pvParameters)
{
  if(xMessageQueue == NULL) return;

  TickType_t xLastWakeTime;
  const int messageBufferLength = 2001;
  char message[messageBufferLength];

  xLastWakeTime = xTaskGetTickCount();
  while(true)
  {
    int index = 0;
    memset(&message, 0x00, messageBufferLength);
    
    while(xQueueReceive(xMessageQueue, &message[index], 0) == pdPASS) { ++index; }
    if(strlen(message) > 0) { Serial.println(message); }

    vTaskDelayUntil(&xLastWakeTime, xSleepPeriod);
  }
}

#endif

void loop() 
{ 
  static const TickType_t ONE_MILLISECOND = pdMS_TO_TICKS(1);

#ifdef SLAVE
  WireSlave.update();
#endif
  vTaskDelay(ONE_MILLISECOND);
}

#ifdef SLAVE

void Message_Task(void * pvParameters)
{
  static const size_t length = strlen(message);
  static const TickType_t TEN_MILLISECONDS = pdMS_TO_TICKS(10);

  while(true)
  {
    if(uxQueueSpacesAvailable(xMessageQueue) < length) 
    { 
      vTaskDelay(TEN_MILLISECONDS);
      continue; 
    }
    
    // Add the string to the Queue
    for(int index = 0; index < length; index++)
    {       
      if(xQueueSendToBack(xMessageQueue, &message[index], 0) != pdPASS) 
        index = length;
    }
   
  }
}

void RequestMade() 
{ 
  static char message[PACKET_LENGTH+1];
  static int index = 0;

  if(xMessageQueue == NULL) return;

  Serial.println("IIC Request received");
  index = 0;
  memset(&message, 0x00, PACKET_LENGTH+1);

  while(index < PACKET_LENGTH)
  {
    xQueueReceive(xMessageQueue, &message[index], 0);
    ++index;
  }

  if(strlen(message) > 0)
  {
    Serial.println("sending data");
    WireSlave.write(message);
  }
}

#endif
