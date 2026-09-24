#include "main.h"
#include "canard_stm32.h"
#include "can_rx_irq.h"

#if CANARD_STM32_USE_CAN2
# define RX_CAN CAN2
# define RX0_IRQn CAN2_RX0_IRQn
# define RX1_IRQn CAN2_RX1_IRQn
#else
# define RX_CAN CAN1
# define RX0_IRQn CAN1_RX0_IRQn
# define RX1_IRQn CAN1_RX1_IRQn
#endif

bool canDriverConfigureRxInterrupt(bool enabled) {
    RX_CAN->IER &= ~(CAN_IER_FMPIE0 | CAN_IER_FMPIE1);
    if (!enabled) {
        return false;
    }

    // Filter registers are shared through CAN1, including when CAN2 is used.
    uint32_t filters = CAN1->FA1R;
#ifdef CAN2
    const unsigned split = (CAN1->FMR & CAN_FMR_CAN2SB) >> 8U;
    if (split > 28U) {
        return false;
    }
    const uint32_t can1_filters = (1U << split) - 1U;
    filters &= CANARD_STM32_USE_CAN2 ? (0x0FFFFFFFU & ~can1_filters) : can1_filters;
#endif
    const bool fifo0 = (filters & ~CAN1->FFA1R) != 0;
    const bool fifo1 = (filters & CAN1->FFA1R) != 0;
    if ((!fifo0 && !fifo1) ||
        (fifo0 && !NVIC_GetEnableIRQ(RX0_IRQn)) ||
        (fifo1 && !NVIC_GetEnableIRQ(RX1_IRQn))) {
        return false;
    }
    // Both callbacks share the queue and low-level state without nested writers.
    if (fifo0 && fifo1 && NVIC_GetPriority(RX0_IRQn) != NVIC_GetPriority(RX1_IRQn)) {
        return false;
    }

    RX_CAN->IER |= (fifo0 ? CAN_IER_FMPIE0 : 0U) | (fifo1 ? CAN_IER_FMPIE1 : 0U);
    return true;
}

const char* canDriverGetRxMode(void) {
    const uint32_t notifications = RX_CAN->IER & (CAN_IER_FMPIE0 | CAN_IER_FMPIE1);
    return notifications == (CAN_IER_FMPIE0 | CAN_IER_FMPIE1) ? "irq fifo0 & fifo1" :
           notifications == CAN_IER_FMPIE0 ? "irq fifo0" :
           notifications == CAN_IER_FMPIE1 ? "irq fifo1" : "poll";
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* can) {
    if (can->Instance == RX_CAN) {
        canDriverHandleRxInterrupt();
    }
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* can) {
    HAL_CAN_RxFifo0MsgPendingCallback(can);
}
