#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"

static const char *TAG = "TWAI";
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#define BITRATE "Bitrate is 500 Kbit/s"

#define MAX_MESSAGE_HISTORY 50

// Define a structure to keep track of received messages and their counts
typedef struct {
    twai_message_t message;  // The CAN message
    int count;               // Number of times this message has been received
} message_history_t;

// Initialize an array to store the message history
static message_history_t message_history[MAX_MESSAGE_HISTORY];

// Function to initialize message history
void init_message_history() {
    for (int i = 0; i < MAX_MESSAGE_HISTORY; i++) {
        message_history[i].count = 0;
    }
}

// Function to compare two twai_message_t structures
bool compare_messages(twai_message_t *msg1, twai_message_t *msg2) {
    // Compare the data part of the message (you can expand to compare more fields if necessary)
    return (msg1->identifier == msg2->identifier) && (msg1->data_length_code == msg2->data_length_code) &&
           (memcmp(msg1->data, msg2->data, msg1->data_length_code) == 0);
}

// Function to check if a message already exists in history
int find_existing_message(twai_message_t *msg) {
    for (int i = 0; i < MAX_MESSAGE_HISTORY; i++) {
        if (message_history[i].count > 0 && compare_messages(&message_history[i].message, msg)) {
            return i;  // Return index of the existing message
        }
    }
    return -1;  // Return -1 if no matching message is found
}

// Function to print message data in a readable format (you can modify it as needed)
void print_message(twai_message_t *msg, int msgCounts) {
    // ESP_LOGI("CAN", "New message received: ID=0x%03"PRIx32", DLC=%d, Data=", msg->identifier, msg->data_length_code);
    // for (int i = 0; i < msg->data_length_code; i++) {
    //     ESP_LOGI("CAN", "0x%02X ", msg->data[i]);
    // }
    // ESP_LOGI("CAN", "\n");

    int ext = msg->flags & 0x01;
    int rtr = msg->flags & 0x02;
    if(ext == 0)
        printf("Standard ID: 0x%03"PRIx32"     ", msg->identifier);
    else 
        printf("Extended ID: 0x%08"PRIx32, msg->identifier);

    printf(" DLC: %d  Data: ", msg->data_length_code);

    if(rtr == 0)
    {
        for(int i = 0; i < msg->data_length_code; i++)
        {
            printf("0x%02x ", msg->data[i]);
        }
    }
    else 
    {
        printf("REMOTE REQUEST FRAME");
    }

    if(msgCounts > 0)
    {
        printf(" || Repeated Msg: %d \n", msgCounts);
    }
    else 
        printf("\n");
    
}

// Function to process the incoming CAN message
void process_can_message(twai_message_t *msg) {
    int existing_msg_index = find_existing_message(msg);

    if (existing_msg_index != -1) {
        // Message already exists, increment the count
        message_history[existing_msg_index].count++;
       // ESP_LOGI("CAN", "Repeated message (ID: 0x%03"PRIx32"): Received %d times", msg->identifier, message_history[existing_msg_index].count);
        //print_message(msg, message_history[existing_msg_index].count);
    } else {
        // New message, store it in history and print
        for (int i = 0; i < MAX_MESSAGE_HISTORY; i++) {
            if (message_history[i].count == 0) {
                message_history[i].message = *msg;  // Store the message
                message_history[i].count = 1;       // Set the count to 1
                print_message(msg, 0);  // Print the new message
                break;
            }
        }
    }
}

void twai_task(void *pvParameters)
{
  ESP_LOGI(TAG,"task start");
  

  // Install and start TWAI driver
  ESP_LOGI(TAG, "%s",BITRATE);
  ESP_LOGI(TAG, "CTX_GPIO=%d",17);
  ESP_LOGI(TAG, "CRX_GPIO=%d",16);

  static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(17, 16, TWAI_MODE_NORMAL);
  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
  ESP_LOGI(TAG, "Driver installed");
  ESP_ERROR_CHECK(twai_start());
  ESP_LOGI(TAG, "Driver started");

  twai_message_t rx_msg;

  while (1) {
    //esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(1));
    esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(10));
    if (ret == ESP_OK) {
      // ESP_LOGD(TAG,"twai_receive identifier=0x%"PRIu32" flags=0x%"PRIu32" extd=0x%x rtr=0x%x data_length_code=%d",
      //   rx_msg.identifier, rx_msg.flags, rx_msg.extd, rx_msg.rtr, rx_msg.data_length_code);

      // int ext = rx_msg.flags & 0x01;
      // int rtr = rx_msg.flags & 0x02;
      // ESP_LOGD(TAG, "ext=%x rtr=%x", ext, rtr);


      // if (ext == 0) {
      //   printf("Standard ID: 0x%03"PRIx32"     ", rx_msg.identifier);
      // } else {
      //   printf("Extended ID: 0x%08"PRIx32, rx_msg.identifier);
      // }
      // printf(" DLC: %d  Data: ", rx_msg.data_length_code);

      // if (rtr == 0) {
      //   for (int i = 0; i < rx_msg.data_length_code; i++) {
      //     printf("0x%02x ", rx_msg.data[i]);
      //   }
      // } else {
      //   printf("REMOTE REQUEST FRAME");

      // }
      // printf("\n");

            process_can_message(&rx_msg);


    } else if (ret == ESP_ERR_TIMEOUT) {

    } else {
      ESP_LOGE(TAG, "twai_receive Fail %s", esp_err_to_name(ret));
    }
  }

  // never reach
  vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG,"task start");
    xTaskCreate(&twai_task, "twai_task", 1024*6, NULL, 2, NULL);
}