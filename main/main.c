#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "MAIN"

// Defines /////////////////////////////////////////////////////////////////////////////////////////
#define ESP_INTR_FLAG_DEFAULT 0
#define PUSH_BUTTON_PIN  33


// Global Variables ////////////////////////////////////////////////////////////////////////////////
TaskHandle_t task_1_handle = NULL;
TaskHandle_t task_2_handle = NULL;
TaskHandle_t ISR = NULL;

volatile uint32_t flank_counter = 0; // TODO: Remove the volatile Keyword

int32_t shared_counter = 0;

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
    bool led_status = false;
    while(1){
        vTaskSuspend(NULL);
        flank_counter--;
        printf("Missed %d flanks since last isr handling call.\n", flank_counter);
    }
}

/**
 * @brief Task to decrement the shared counter.
 * @param arg Pointer to task arguments.
*/
void task_1(void *arg)
{
    while(1){
        uint32_t temp = shared_counter;
        temp++;
        vTaskDelay(pdMS_TO_TICKS(100));
        shared_counter = temp;
        ESP_LOGI(TAG, "Incremented (Task 1): Counter = %d", shared_counter);
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
        uint32_t temp = shared_counter;
        temp--;
        vTaskDelay(pdMS_TO_TICKS(100));
        shared_counter = temp;
        ESP_LOGI(TAG, "Decremented (Task 2): Counter = %d", shared_counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{   
    BaseType_t xRet;

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

    // setup of push button gpio
    ESP_ERROR_CHECK(gpio_set_direction(PUSH_BUTTON_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(PUSH_BUTTON_PIN, GPIO_PULLUP_ONLY));
    ESP_ERROR_CHECK(gpio_set_intr_type(PUSH_BUTTON_PIN, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PUSH_BUTTON_PIN, button_isr_handler, NULL));

    // start main app
    ESP_LOGI(TAG, "Starting main app.")
    while (1) {
        /* code */
    }
    
 }
