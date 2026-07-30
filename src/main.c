/*              == REAL-TIME SYSTEMS CAPSTONE ==
 *
 * ============================================================
 *  LOCK MODE  (priority-inversion demo)
 * ============================================================
 *
 * USE_PI_MUTEX selects the lock type shared by tasks H and L. Both modes run the
 * SAME H/M/L scenario and log the SAME fields (H's block->acquire wait, plus the
 * L and M timeline); only the lock primitive differs.
 *
 *   USE_PI_MUTEX = 1  -> H and L share a FreeRTOS MUTEX (priority inheritance
 *                        ON). When H blocks on the lock L holds, L inherits H's
 *                        priority, so M cannot preempt L. H waits about L's
 *                        remaining critical section — bounded.
 *   USE_PI_MUTEX = 0  -> H and L share a BINARY SEMAPHORE used as a lock (no
 *                        ownership, no inheritance). M preempts L while L still
 *                        holds the lock, so H waits for M to finish too — the
 *                        classic unbounded priority inversion.
 *
 * The separation only appears because L's critical section is CPU-bound (a fixed
 * iteration burn). If L merely slept, the CPU would be free, M would run in both
 * modes, and the two numbers would converge — which is why the demo burns cycles
 * instead of calling vTaskDelay inside the lock.
 *
 * ============================================================
 * Theme: Aerospace
 * ============================================================
 */

#ifndef USE_PI_MUTEX
#define USE_PI_MUTEX 1 // set to 1 to use Mutex (priority inheritance enabled), 0 for binary semaphore (no ownership- no priority inheritance)
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_task_wdt.h"

/* ---------- Configuration ---------- */
#define LED_GPIO          GPIO_NUM_2
#define SIGNALING_PERIOD_MS   1000          /* 1 Hz signaling rate */

#define BUTTON_GPIO GPIO_NUM_18

#define CONFIG_LOG_DEFAULT_LEVEL_INFO 1
#define CONFIG_LOG_MAXIMUM_LEVEL  5

static const char *TAG = "PI_Demo";

/* ---------- Synchronization primitives ---------- */
static SemaphoreHandle_t sig_sem;               /* binary — ISR → responder */
static SemaphoreHandle_t control_loop_sem;      /* binary - wakes the flight control loop every SIGNALING_PERIOD_MS */

/* ---------- ISR: signal the Terrain Mapper ---------- */
static volatile int64_t last_edge_us;
static void IRAM_ATTR button_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if (now - last_edge_us < 100000) return;       /* debounce */
    last_edge_us = now;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sig_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ============================================================
 *  Priority-inversion demo  (tasks H / M / L)
 * ============================================================
 *
 * Classic three-task inversion on one core:
 *   - Telemetry Logger (low,  prio 5, known as "L" in the defines below)  grabs the lock and runs a CPU-bound section for a short amount of time, but frequently.
 *   - Flight Controller (high, prio 15, known as "H") tries the lock every second, usually blocked momentarily by the Telemetry Logger.
 *   - Terrain Mapper (mid,  prio 10, known as "M") burns CPU. It does NOT touch the lock — it is pure interference, and it can interfere heavily.
 *
 * MUTEX mode  (USE_PI_MUTEX=1): when the flight controller task blocks, the telemetry logger inherits prio 15, so the terrain mapper cannot
 *   preempt the telemetry logger; telemetry logger finishes its section and hands the lock to the flight controller, whose wait is
 *   bounded by the logger's remaining critical section.
 * BINARY-SEM mode (USE_PI_MUTEX=0): telemetry logger stays at prio 5; terrain mapper preempts the logger while the logger holds
 *   the lock, so the flight controller waits for the terrain mapper to complete too. The wait inflates according to the mapper's run time.
 *
 * Read the yellow log line for a time measurement of how long the flight controller waited each cycle.
 * Or, notice how long the red light stays on when using a semaphore instead of a mutex.
 */
#if USE_PI_MUTEX
#define PI_LOCK_CREATE() xSemaphoreCreateMutex()
#define PI_LOCK_NAME     "MUTEX (priority inheritance ON)"
#else
#define PI_LOCK_CREATE() xSemaphoreCreateBinary()
#define PI_LOCK_NAME     "BINARY SEM (no inheritance)"
#endif

static SemaphoreHandle_t pi_lock;

/* Stagger knobs — control the ordering, not the durations. */
#define PI_H_DELAY_MS  50      /* H tries the lock 50 ms after start (after L holds it) */
#define PI_M_DELAY_MS  100     /* M becomes ready 100 ms after start */

/* Work knobs — fixed-iteration CPU burns. Tunable */
#define PI_L_ITERS  500000UL
#define PI_M_ITERS  4000000UL

static volatile uint32_t pi_sink;       /* defeats dead-code elimination */
static void pi_burn(uint32_t iters)
{
    uint32_t x = pi_sink ? pi_sink : 1u;
    for (uint32_t i = 0; i < iters; i++) { x ^= (x << 5); x += i; }
    pi_sink = x;
}

static void telemetry_logger_task(void *arg)
{
    for (;;) {
        /* This task is created last in app_main, and it grabs the lock immediately. */
        xSemaphoreTake(pi_lock, portMAX_DELAY);
        int64_t t_acq = esp_timer_get_time();
        ESP_LOGI(TAG, "Telemetry logger took lock @ %lld us — entering CPU-bound section",
                (long long)t_acq);
        pi_burn(PI_L_ITERS);
        int64_t t_rel = esp_timer_get_time();
        xSemaphoreGive(pi_lock);
        ESP_LOGI(TAG, "Telemetry Logger: Released lock @ %lld us (held %lld us wall-clock)",
                (long long)t_rel, (long long)(t_rel - t_acq));
        vTaskDelay(pdMS_TO_TICKS(SIGNALING_PERIOD_MS/8)); // restarts 8x per signaling period for the high priority task
    }
}

static void terrain_mapper_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(PI_M_DELAY_MS)); // This task should start after the others even if the button is being held on startup
    for (;;) {
        if (xSemaphoreTake(sig_sem, portMAX_DELAY) == pdTRUE) { // wait for button press
            int64_t t0 = esp_timer_get_time();
            ESP_LOGI(TAG, "Terrain mapper ready @ %lld us — burning CPU (takes no lock)",
                    (long long)t0);
            pi_burn(PI_M_ITERS);
            int64_t t1 = esp_timer_get_time();
            ESP_LOGI(TAG, "Terrain Mapper: Done  @ %lld us (ran %lld us wall-clock)",
                    (long long)t1, (long long)(t1 - t0));
        }
    }
}

static void flight_control_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(PI_H_DELAY_MS)); // This task should start after the logger gets the lock, for demonstration purposes
    for (;;) {
        // block on receiving the binary semaphore every 1s here
        xSemaphoreTake(control_loop_sem, portMAX_DELAY);

        gpio_set_level(LED_GPIO, 1);
        int64_t t_block = esp_timer_get_time();
        ESP_LOGI(TAG, "Flight controller wants lock @ %lld us — blocking", (long long)t_block);
        xSemaphoreTake(pi_lock, portMAX_DELAY);

        int64_t t_acq = esp_timer_get_time();
        gpio_set_level(LED_GPIO, 0);
        int64_t wait = t_acq - t_block;
        ESP_LOGW(TAG, "Flight Controller: ACQUIRED @ %lld us — waited %lld us (~%lld ms)  [lock=%s]",
                (long long)t_acq, (long long)wait, (long long)(wait / 1000), PI_LOCK_NAME);
        xSemaphoreGive(pi_lock);
    }
}

static void control_loop_task(void *arg)
{

     // vTaskDelayUntil is drift-free: we wake on a fixed schedule
     // even if our work took a variable amount of time. 
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SIGNALING_PERIOD_MS);

    for (;;) {
        xSemaphoreGive(control_loop_sem);
        ESP_LOGI(TAG, "Locking mechanism released!");
        vTaskDelayUntil(&last_wake, period);
    }
} 


static void start_inversion_demo(void)
{
    pi_lock = PI_LOCK_CREATE();
#if !USE_PI_MUTEX
    /* A binary semaphore is created empty; prime it once so it starts "unlocked". */
    xSemaphoreGive(pi_lock);
#endif
    ESP_LOGI(TAG, "[PI] inversion demo lock = %s", PI_LOCK_NAME);

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    /* Create flight control and terrain mapper tasks first (they delay before acting), then logger LAST so it wins the
     * lock the instant it is created instead of starving app_main while it burns. */
    xTaskCreatePinnedToCore(flight_control_task, "H", 4096, NULL, 15, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(terrain_mapper_task,  "M", 4096, NULL, 10, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(telemetry_logger_task,  "L", 4096, NULL,  5, NULL, APP_CPU_NUM);

    // start high priority task signaling task
    xTaskCreatePinnedToCore(
        control_loop_task,        /* function */
        "timer",           /* name (max 16 chars) */
        2048,              /* stack — 2048 words = 8 KB */
        NULL,              /* parameters */
        20,                 /* priority */ // 20 is higher than the H task (15)
        NULL,              /* task handle (we don't need it) */
        APP_CPU_NUM        /* Core 1 */
    );
}

/* ---------- app_main ---------- */
void app_main(void)
{
    esp_task_wdt_reconfigure(&(esp_task_wdt_config_t){.timeout_ms = 10000, .idle_core_mask = 0, .trigger_panic = false });
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "==== Priority Inversion demonstration [Martian Drone] starting ====");
    ESP_LOGI(TAG, "Lock mode: %s (USE_PI_MUTEX=%d)", PI_LOCK_NAME, USE_PI_MUTEX);

    sig_sem          = xSemaphoreCreateBinary();
    control_loop_sem = xSemaphoreCreateBinary();

    /* ISR + responder */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);

    /* Priority-inversion demo */
    start_inversion_demo();
}
