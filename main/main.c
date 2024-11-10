#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "mbedtls/aes.h"
#include "esp_log.h"

#define TAG "MAIN"

// Defines /////////////////////////////////////////////////////////////////////////////////////////
// push button configuration
#define PUSH_BUTTON_PIN  33
#define ESP_INTR_FLAG_DEFAULT 0
// UART configuration
#define UART_NUM UART_NUM_0
#define BUF_SIZE 256
#define RD_BUF_SIZE (BUF_SIZE)

// Global Variables ////////////////////////////////////////////////////////////////////////////////
TaskHandle_t task_1_handle = NULL;
TaskHandle_t task_2_handle = NULL;
TaskHandle_t ISR = NULL;

uint32_t flank_counter = 0;

volatile int32_t shared_counter = 0;

// 128-bit AES key (16 bytes)
const unsigned char aes_key[16] = "1234567890abcdef";

// Function Definitions ///////////////////////////////////////////////////////////////////////////
/**
 * @brief Handler function for the rising edge gpio isr. Increments the detected flanks counter.
 * @param arg Pointer to ISR arguments.
*/
void IRAM_ATTR button_isr_handler(void *arg){
    flank_counter++;
    xTaskResumeFromISR(ISR);
}

/**
 * @brief Task to handle the ISR triggered workload.
 * @param arg Pointer to task arguments.
*/
void interrupt_task(void *arg){
    while(1){
        vTaskSuspend(NULL);
        flank_counter--;
        ESP_LOGI(TAG,"Missed %d flanks since last isr handling call.\n", (int)flank_counter);
    }
}

/**
 * @brief Task to decrement the shared counter.
 * @param arg Pointer to task arguments.
*/
void task_1(void *arg)
{
    while(1){
        int32_t temp = shared_counter;
        temp++;
        vTaskDelay(pdMS_TO_TICKS(100));
        shared_counter = temp;
        ESP_LOGI(TAG, "Incremented (Task 1): Counter = %d", (int)shared_counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
    }
}

/**
 * @brief Task to decrement the shared counter.
 * @param arg Pointer to task arguments.
*/
void task_2(void *arg)
{
    while(1){
        int32_t temp = shared_counter;
        temp--;
        vTaskDelay(pdMS_TO_TICKS(100));
        shared_counter = temp;
        ESP_LOGI(TAG, "Decremented (Task 2): Counter = %d", (int)shared_counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Task to receive encrypted data via uart and decrypt the data.
 * @param arg Pointer to task arguments.
*/
void task_3(void *arg) {
    int num_rx_bytes = 0;
    unsigned char encrypted_data[RD_BUF_SIZE];
    mbedtls_aes_context aes;

    while (1) {
        // Read data from UART
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM, (size_t*)&num_rx_bytes));
        // If we receive 16 bytes of data, we assume it's the encrypted block
        if (num_rx_bytes >= 16) {
            uart_read_bytes(UART_NUM, encrypted_data, RD_BUF_SIZE, 20 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Encrypted Data Received: ");
            for (uint8_t i = 0; i < 16; i++) {
                ESP_LOGI(TAG, "0x%02x ", encrypted_data[i]);
            }
            unsigned char decrypted_data[16]; // Buffer for decrypted data

            // Initialize the AES context
            mbedtls_aes_init(&aes);
            mbedtls_aes_setkey_dec(&aes, aes_key, 128); // 128-bit key size

            // Decrypt the block
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, encrypted_data, decrypted_data);

            // Null-terminate the decrypted string to safely print as text
            decrypted_data[10] = '\0';

            // Print the decrypted data as a string
            ESP_LOGI(TAG, "Decrypted String: %s", decrypted_data);
        }
    }

    // Free the AES context
    mbedtls_aes_free(&aes);
}

void app_main(void)
{   
    BaseType_t xRet;

    uint32_t array[300];
    // initialize the array with ones
    for (uint8_t i = 0; i < 400; i++) {
        array[i] = -1;
    }

    // setup push button gpio
    ESP_ERROR_CHECK(gpio_set_direction(PUSH_BUTTON_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(PUSH_BUTTON_PIN, GPIO_PULLUP_ONLY));
    ESP_ERROR_CHECK(gpio_set_intr_type(PUSH_BUTTON_PIN, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PUSH_BUTTON_PIN, button_isr_handler, NULL));

    // setup uart driver
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // Install UART driver
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);

    // create the tasks
    xRet = xTaskCreate(task_1, "task_1", 4096, NULL, 10, &task_1_handle);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Task 1 creation failed!");
    } else {
        ESP_LOGI(TAG, "Task 1 created successfully.");
    }
    xRet = xTaskCreate(task_2, "task_2", 4096, NULL,10, &task_2_handle);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "Task 2 creation failed!");
    } else {
        ESP_LOGI(TAG, "Task 2 created successfully.");
    }
    xRet = xTaskCreate(interrupt_task, "interrupt_task", 4096, NULL, 10, &ISR);
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "ISR task creation failed!");
    } else {
        ESP_LOGI(TAG, "ISR task created successfully.");
    }

    // start main app
    ESP_LOGI(TAG, "Starting main app.");
    while (1) {
    }
    
 }
