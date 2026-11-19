/* Shared runtime of the CANopenNode fuzz harnesses and the self-test: a host CAN driver that hands
 * frames to the stack's own receive callbacks, plus the stack lifecycle of example/main_blank.c. */
#ifndef CO_FUZZ_H
#define CO_FUZZ_H

#include <stddef.h>
#include <stdint.h>

#include "CANopen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COF_NODE_ID 10U

/* Every frame the stack transmits is recorded here (the newest COF_TX_LOG frames). */
#define COF_TX_LOG 64U
typedef struct {
    uint16_t ident; /* 11-bit identifier */
    uint8_t DLC;
    uint8_t data[8];
} cof_frame_t;
extern cof_frame_t cof_txLog[COF_TX_LOG];
extern uint32_t cof_txCount;

/* Gateway output (CO_GTWA_initRead sink), NUL-terminated, truncated at COF_GTW_OUT - 1 bytes. */
#define COF_GTW_OUT 8192U
extern char cof_gtwOut[COF_GTW_OUT];
extern size_t cof_gtwOutLen;

/* Fresh stack: restores the example OD to its initial values, allocates and initializes all objects
 * (node-id 10, gateway on), and goes to CAN normal mode. Returns NULL on an init failure. */
CO_t* cof_init(void);
void cof_fini(CO_t* co);

/* Deliver one received frame (ident bit 0x800 = RTR) to the first matching receive buffer, as a CAN
 * controller's receive interrupt would. dlc is 0..8. */
void cof_deliver(CO_t* co, uint16_t ident, uint8_t dlc, const uint8_t* data);

/* One main-loop + timer-thread pass (CO_process with the gateway, SYNC, RPDO, TPDO).
 * Returns the NMT reset command CO_process asked for (CO_RESET_NOT to keep going). */
CO_NMT_reset_cmd_t cof_step(CO_t* co, uint32_t timeDifference_us);

/* Write gateway text: in chunks that fit the command FIFO, with a processing pass after each. */
CO_NMT_reset_cmd_t cof_gtw_write(CO_t* co, const char* text, size_t len, uint32_t timeDifference_us);

#ifdef __cplusplus
}
#endif

#endif /* CO_FUZZ_H */
