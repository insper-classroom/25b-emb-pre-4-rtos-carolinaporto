#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#define BTN_PIN_R   28
#define BTN_PIN_Y   21
#define LED_PIN_R    5
#define LED_PIN_Y   10

static QueueHandle_t     xQueueBtn; 
static SemaphoreHandle_t xSemaphoreLedR;
static SemaphoreHandle_t xSemaphoreLedY;

#define DEBOUNCE_MS 120

static void btn_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        BaseType_t hpw = pdFALSE;
        xQueueSendFromISR(xQueueBtn, &gpio, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}

static void led_r_task(void *p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);

    bool blinking = false;
    bool level = false;

    for (;;) {
        if (!blinking) {
            xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY);
            blinking = true;
            level = false;
            gpio_put(LED_PIN_R, 0);
        } else {
            if (xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(100)) == pdTRUE) {
                blinking = false;
                gpio_put(LED_PIN_R, 0); 
            } else {
                level = !level;
                gpio_put(LED_PIN_R, level);
            }
        }
    }
}

static void led_y_task(void *p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);

    bool blinking = false;
    bool level = false;

    for (;;) {
        if (!blinking) {
            xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY);
            blinking = true;
            level = false;
            gpio_put(LED_PIN_Y, 0);
        } else {
            if (xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(100)) == pdTRUE) {
                blinking = false;
                gpio_put(LED_PIN_Y, 0);
            } else {
                level = !level;
                gpio_put(LED_PIN_Y, level);
            }
        }
    }
}


static void btn_task(void *p) {
    gpio_init(BTN_PIN_R); gpio_set_dir(BTN_PIN_R, GPIO_IN); gpio_pull_up(BTN_PIN_R);
    gpio_init(BTN_PIN_Y); gpio_set_dir(BTN_PIN_Y, GPIO_IN); gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    absolute_time_t last_r = 0, last_y = 0;

    for (;;) {
        uint gpio;
        if (xQueueReceive(xQueueBtn, &gpio, portMAX_DELAY) == pdTRUE) {
            absolute_time_t now = get_absolute_time();
            if (gpio == BTN_PIN_R) {
                if (absolute_time_diff_us(last_r, now) / 1000 >= DEBOUNCE_MS) {
                    while (!gpio_get(BTN_PIN_R)) vTaskDelay(pdMS_TO_TICKS(1));
                    xSemaphoreGive(xSemaphoreLedR);
                    last_r = now;
                }
            } else if (gpio == BTN_PIN_Y) {
                if (absolute_time_diff_us(last_y, now) / 1000 >= DEBOUNCE_MS) {
                    while (!gpio_get(BTN_PIN_Y)) vTaskDelay(pdMS_TO_TICKS(1));
                    xSemaphoreGive(xSemaphoreLedY); 
                    last_y = now;
                }
            }
        }
    }
}

int main() {
    stdio_init_all();

    xQueueBtn     = xQueueCreate(16, sizeof(uint));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task,   "btn",  512, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(led_r_task, "ledR", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(led_y_task, "ledY", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    while (true)
    return 0;
}