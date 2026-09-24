#ifndef BXCAN_CAN_RX_IRQ_H
#define BXCAN_CAN_RX_IRQ_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional platform hook: called with interrupts masked; must not wait.
 * Disable CAN RX notifications on false; on true, enable them only if the
 * required RX handlers are configured. Return false to select polling.
 * NVIC enablement and priority belong to the board/CubeMX configuration.
 */
bool canDriverConfigureRxInterrupt(bool enabled);
const char* canDriverGetRxMode(void);

/* Call only from the owned CAN RX vector(s), at the same preemption priority.
 * Drains at most three frames into the queue; never calls protocol callbacks.
 */
void canDriverHandleRxInterrupt(void);

#ifdef __cplusplus
}
#endif
#endif
