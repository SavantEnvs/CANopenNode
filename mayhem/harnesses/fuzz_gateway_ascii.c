/* CiA 309-3 ASCII gateway command stream (309/CO_gateway_ascii.c): the input is the text a client
 * writes to the gateway's serial/TCP port. It is fed through CO_GTWA_write() in chunks the command
 * FIFO accepts, with the stack processed in between (CO_process with the gateway enabled), then the
 * stack runs on so commands that wait (SDO client to a remote node, LSS, NMT) reach their timeouts.
 * Reaches the command parser, the CO_fifo ASCII datatype converters, the local SDO client into the
 * node's own object dictionary, and the NMT/LSS master command paths. */
#include <stddef.h>
#include <stdint.h>

#include "co_fuzz.h"

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    CO_t* co = cof_init();
    if (co == NULL) {
        return 0;
    }
    CO_NMT_reset_cmd_t reset = cof_gtw_write(co, (const char*)data, size, 1000);
    for (int i = 0; i < 100 && reset == CO_RESET_NOT; i++) {
        reset = cof_step(co, 10000);
    }
    cof_fini(co);
    return 0;
}
