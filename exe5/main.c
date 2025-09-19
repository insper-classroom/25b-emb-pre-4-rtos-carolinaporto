#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdbool.h>
#include <stdint.h>

#define BTN_PIN_R   28
#define BTN_PIN_Y   21
#define LED_PIN_R    5
#define LED_PIN_Y   10

static QueueHandle_t     xQueueBtn;        // carrega o GPIO do evento
static SemaphoreHandle_t xSemaphoreLedR;   // toggle pedido p/ LED R
static SemaphoreHandle_t xSemaphoreLedY;   // toggle pedido p/ LED Y

#define DEBOUNCE_MS 120

static void btn_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        BaseType_t hpw = pdFALSE;
        xQueueSendFromISR(xQueueBtn, &gpio, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}

static void btn_task(void* p) {
    gpio_init(BTN_PIN_R); gpio_set_dir(BTN_PIN_R, GPIO_IN); gpio_pull_up(BTN_PIN_R);
    gpio_init(BTN_PIN_Y); gpio_set_dir(BTN_PIN_Y, GPIO_IN); gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    TickType_t lastEvtR = 0, lastEvtY = 0;

    for (;;) {
        uint32_t gpio;
        if (xQueueReceive(xQueueBtn, &gpio, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();

            if (gpio == BTN_PIN_R) {
                if (now - lastEvtR >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
                    xSemaphoreGive(xSemaphoreLedR);
                    lastEvtR = now;
                }
            } else if (gpio == BTN_PIN_Y) {
                if (now - lastEvtY >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
                    xSemaphoreGive(xSemaphoreLedY);
                    lastEvtY = now;
                }
            }
        }
    }
}

typedef struct { uint led_pin; SemaphoreHandle_t sem; } LedArgs;

static void led_task(void *p) {
    LedArgs *args = (LedArgs*)p;
    const uint led = args->led_pin;
    SemaphoreHandle_t sem = args->sem;

    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, 0);

    bool blinking = false;
    bool level = false;

    for (;;) {
        if (xSemaphoreTake(sem, 0) == pdTRUE) {
            blinking = !blinking;
            if (!blinking) { 
                level = false;
                gpio_put(led, 0);
            }
        }

        if (blinking) {
            level = !level;
            gpio_put(led, level);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

int main() {
    stdio_init_all();

    xQueueBtn     = xQueueCreate(16, sizeof(uint32_t));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    static LedArgs argR, argY;
    argR.led_pin = LED_PIN_R; argR.sem = xSemaphoreLedR;
    argY.led_pin = LED_PIN_Y; argY.sem = xSemaphoreLedY;

    xTaskCreate(btn_task,   "btn_task",   256, NULL, 2, NULL);
    xTaskCreate(led_task,   "led_r_task", 256, &argR, 1, NULL);
    xTaskCreate(led_task,   "led_y_task", 256, &argY, 1, NULL);

    vTaskStartScheduler();
    while (true) {}
    return 0;
}
