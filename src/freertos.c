#include <stdarg.h>
#include <stdio.h>
#include "stm32f7xx_remote_io.h"

SemaphoreHandle_t loggingSemaphoreHandle;
extern SemaphoreHandle_t deleteTaskSemaphoreHandle;

extern void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                          StackType_t **ppxIdleTaskStackBuffer,
                                          uint32_t *puxIdleTaskStackSize);

extern void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                           StackType_t **ppxTimerTaskStackBuffer,
                                           uint32_t *puxTimerTaskStackSize);

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *puxIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *puxIdleTaskStackSize = (uint32_t)(sizeof(uxIdleTaskStack) / sizeof(StackType_t)); //(configSTACK_DEPTH_TYPE)(configMINIMAL_STACK_SIZE * 2);
}
/*-----------------------------------------------------------*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *puxTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *puxTimerTaskStackSize = (uint32_t)(sizeof(uxTimerTaskStack) / sizeof(StackType_t));
}

// FreeRTOS logging function
void vLoggingPrintf(const char *pcFormatString, ...)
{
    xSemaphoreTake(loggingSemaphoreHandle, portMAX_DELAY);

    // SEGGER_SYSVIEW_PrintfTarget(pcFormatString);
    va_list arg;

    va_start(arg, pcFormatString);
    vprintf(pcFormatString, arg);
    va_end(arg);

    xSemaphoreGive(loggingSemaphoreHandle);
}

void vApplicationMallocFailedHook(void)
{
    static volatile uint32_t ulMallocFailures = 0;

    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    ulMallocFailures++;

    vLoggingPrintf("Malloc failed %d times\n", ulMallocFailures);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)xTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}

void vApplicationIdleHook(void)
{
    // vLoggingPrintf("Idle Hook\n");

    // check if delete task semaphore is not null
    if (deleteTaskSemaphoreHandle != NULL)
    {
        // give the semaphore to delete the task
        xSemaphoreGive(deleteTaskSemaphoreHandle);
    }
}

void freertos_init()
{
    // initialize the handle of semaphore for logging
    // to protect the critical section in the vLoggingPrintf function.
    loggingSemaphoreHandle = xSemaphoreCreateBinary();
    xSemaphoreGive(loggingSemaphoreHandle);

    // set priority grouping to 0 to pass the assertion of FreeRTOS at port.c
    HAL_NVIC_SetPriorityGrouping(0);
}