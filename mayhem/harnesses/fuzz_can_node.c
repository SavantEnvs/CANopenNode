/* CAN frames received by a CANopen node (node-id 10): every receive callback the stack registers
 * (SDO server expedited/segmented/block transfers into the object dictionary, NMT, heartbeat and
 * EMCY consumers, SYNC, TIME, RPDOs, LSS slave, node guarding) via the same ident/mask dispatch a CAN
 * controller driver does. The input is a sequence of 12-byte records
 *   u16 ident (LE, bit 0x800 = RTR) | u8 DLC (mod 9) | 8 data bytes | u8 time step (x 100 us)
 * each delivered and followed by one main-loop/timer pass with that time step. */
#include <stddef.h>
#include <stdint.h>

#include "co_fuzz.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    CO_t* co = cof_init();
    if (co == NULL) {
        return 0;
    }
    CO_NMT_reset_cmd_t reset = cof_step(co, 1000);
    while (size >= 12U && reset == CO_RESET_NOT) {
        uint16_t ident = (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
        cof_deliver(co, ident, (uint8_t)(data[2] % 9U), data + 3);
        reset = cof_step(co, (uint32_t)data[11] * 100U);
        data += 12;
        size -= 12U;
    }
    cof_fini(co);
    return 0;
}
