/* LSS master (305/CO_LSSmaster.c) against malicious LSS slaves. The gateway issues one fixed LSS
 * command (first input byte selects it: switch selective, inquire, configure node-id/bit rate,
 * store, fastscan, allnodes), then the rest of the input is the slaves' responses on 0x7E4, as
 * 10-byte records  u8 DLC (mod 9) | 8 data bytes | u8 time step (x 100 us), each followed by a
 * processing pass. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "co_fuzz.h"

static const char* const commands[] = {
    "[1] lss_switch_sel 0x1 0x2 0x3 0x4\n",
    "[1] lss_switch_glob 1\n[1] lss_set_node 5\n",
    "[1] lss_switch_glob 1\n[1] lss_conf_bitrate 0 4\n",
    "[1] lss_switch_glob 1\n[1] lss_activate_bitrate 100\n",
    "[1] lss_switch_glob 1\n[1] lss_store\n",
    "[1] lss_switch_glob 1\n[1] lss_inquire_addr\n",
    "[1] lss_switch_glob 1\n[1] lss_inquire_addr 0\n",
    "[1] lss_switch_glob 1\n[1] lss_get_node\n",
    "[1] _lss_fastscan 100\n",
    "[1] lss_allnodes\n",
    "[1] lss_allnodes 100 0 1 0 2 0 0 0 0 0\n",
    "[1] lss_switch_sel 0x1 0x2 0x3 0x4\n[1] lss_set_node 7\n[1] lss_store\n",
};
#define N_COMMANDS (sizeof(commands) / sizeof(commands[0]))

int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1U) {
        return -1;
    }
    CO_t* co = cof_init();
    if (co == NULL) {
        return 0;
    }
    const char* cmd = commands[data[0] % N_COMMANDS];
    data++;
    size--;
    CO_NMT_reset_cmd_t reset = cof_gtw_write(co, cmd, strlen(cmd), 1000);
    while (size >= 10U && reset == CO_RESET_NOT) {
        cof_deliver(co, 0x7E4, (uint8_t)(data[0] % 9U), data + 1);
        reset = cof_step(co, (uint32_t)data[9] * 100U);
        data += 10;
        size -= 10U;
    }
    for (int i = 0; i < 60 && reset == CO_RESET_NOT; i++) {
        reset = cof_step(co, 10000);
    }
    cof_fini(co);
    return 0;
}
