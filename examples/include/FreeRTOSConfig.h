#pragma once

#include <stddef.h>
#include <stdint.h>

extern uint32_t SystemCoreClock;

/* ---------------------------------------------------------
 * Scheduler
 * --------------------------------------------------------- */

#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

#define configTICK_RATE_HZ                      ((TickType_t)1000)

#define configMAX_PRIORITIES                    32
#define configMINIMAL_STACK_SIZE                ((configSTACK_DEPTH_TYPE)256)
#define configTOTAL_HEAP_SIZE                   (128 * 1024)
#define configMAX_TASK_NAME_LEN                 16

#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TIME_SLICING                  1

#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* ---------------------------------------------------------
 * Synchronisation
 * --------------------------------------------------------- */

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    1

#define configQUEUE_REGISTRY_SIZE               8

/* ---------------------------------------------------------
 * Task features
 * --------------------------------------------------------- */

#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     1

#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

#define configUSE_TASK_NOTIFICATIONS            1

/* ---------------------------------------------------------
 * Timers
 *
 * Required by the RP2350 port because it uses
 * xTimerPendFunctionCallFromISR().
 * --------------------------------------------------------- */

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            1024

/* ---------------------------------------------------------
 * Memory allocation
 * --------------------------------------------------------- */

#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0

/* ---------------------------------------------------------
 * Optional API functions
 * --------------------------------------------------------- */

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1

#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

#define INCLUDE_xTimerPendFunctionCall          1

/* ---------------------------------------------------------
 * RP2350 multicore configuration
 *
 * FreeRTOS runs on core 0 only.
 * --------------------------------------------------------- */

#define configNUMBER_OF_CORES                   1
#define configTICK_CORE                         0

#define configUSE_CORE_AFFINITY                 0
#define configUSE_PASSIVE_IDLE_HOOK             0

/* ---------------------------------------------------------
 * RP2350 ARMv8-M configuration
 * --------------------------------------------------------- */

#define configENABLE_FPU                        1
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1
#define configENABLE_MPU                        0

#define configMAX_SYSCALL_INTERRUPT_PRIORITY    16

/* ---------------------------------------------------------
 * Pico SDK interoperability
 * --------------------------------------------------------- */

#define configSUPPORT_PICO_SYNC_INTEROP         1
#define configSUPPORT_PICO_TIME_INTEROP         1

/* ---------------------------------------------------------
 * Assertions
 * --------------------------------------------------------- */

#define configASSERT(x)                         \
    do {                                        \
        if (!(x)) {                             \
            portDISABLE_INTERRUPTS();           \
            for (;;) {                          \
                tight_loop_contents();          \
            }                                   \
        }                                       \
    } while (0)
