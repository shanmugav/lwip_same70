/* lwIP FreeRTOS OS abstraction implementation */
#include "lwip/sys.h"
#include "lwip/opt.h"
#include "arch/sys_arch.h"

#if NO_SYS == 0

void sys_init(void)
{
    /* Nothing to do */
}

u32_t sys_now(void)
{
    return (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* --- Semaphores --- */

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    *sem = xSemaphoreCreateCounting(0xFFFF, count);
    if (*sem == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    vSemaphoreDelete(*sem);
    *sem = NULL;
}

void sys_sem_signal(sys_sem_t *sem)
{
    xSemaphoreGive(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    TickType_t ticks;
    TickType_t start = xTaskGetTickCount();

    if (timeout == 0) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout);
    }

    if (xSemaphoreTake(*sem, ticks) == pdTRUE) {
        u32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
        return elapsed;
    }
    return SYS_ARCH_TIMEOUT;
}

/* --- Mailboxes --- */

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (size <= 0) {
        size = 16;
    }
    *mbox = xQueueCreate((UBaseType_t)size, sizeof(void *));
    if (*mbox == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    vQueueDelete(*mbox);
    *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    while (xQueueSend(*mbox, &msg, portMAX_DELAY) != pdTRUE) {
        /* Retry */
    }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (xQueueSend(*mbox, &msg, 0) == pdTRUE) {
        return ERR_OK;
    }
    return ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    TickType_t ticks;
    TickType_t start = xTaskGetTickCount();
    void *dummy;

    if (msg == NULL) {
        msg = &dummy;
    }

    if (timeout == 0) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout);
    }

    if (xQueueReceive(*mbox, msg, ticks) == pdTRUE) {
        u32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
        return elapsed;
    }
    *msg = NULL;
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *dummy;
    if (msg == NULL) {
        msg = &dummy;
    }

    if (xQueueReceive(*mbox, msg, 0) == pdTRUE) {
        return 0;
    }
    return SYS_MBOX_EMPTY;
}

/* --- Threads --- */

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                            int stacksize, int prio)
{
    TaskHandle_t handle = NULL;
    xTaskCreate(thread, name, (uint16_t)stacksize, arg, (UBaseType_t)prio, &handle);
    return handle;
}

/* --- Critical section protection --- */

#if SYS_LIGHTWEIGHT_PROT

sys_prot_t sys_arch_protect(void)
{
    taskENTER_CRITICAL();
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    taskEXIT_CRITICAL();
}

#endif /* SYS_LIGHTWEIGHT_PROT */

#endif /* NO_SYS == 0 */
