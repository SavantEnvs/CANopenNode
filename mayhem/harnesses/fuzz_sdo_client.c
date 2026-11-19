/* SDO client (301/CO_SDOclient.c) against a malicious SDO server. The gateway issues one fixed
 * command to remote node 5 (first input byte selects it: expedited/segmented/block upload or
 * download), then the rest of the input is the server's response stream on 0x585, as 10-byte
 * records  u8 DLC (mod 9) | 8 data bytes | u8 time step (x 100 us), each followed by a processing
 * pass. Reaches the client's upload/download state machines, block-transfer CRC and the gateway's
 * binary-to-ASCII output of the uploaded data. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "co_fuzz.h"

static const char* const commands[] = {
    "[1] 5 r 0x1008 0 vs\n",
    "[1] 5 r 0x1018 1 u32\n",
    "[1] 5 r 0x2000 0 d\n",
    "[1] 5 r 0x1000 0\n",
    "[1] 5 w 0x2000 0 u8 0x12\n",
    "[1] 5 w 0x1017 0 u16 1000\n",
    "[1] 5 w 0x1008 0 vs \"a segmented download of a visible string\"\n",
    "[1] 5 w 0x2000 0 d AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\n",
    "[1] set sdo_block 1\n[1] 5 r 0x1008 0 vs\n",
    "[1] set sdo_block 1\n[1] 5 r 0x2000 0 d\n",
    "[1] set sdo_block 1\n[1] 5 w 0x1008 0 vs \"a block download of a visible string, longer than seven bytes\"\n",
    "[1] set sdo_block 1\n[1] 5 w 0x2000 0 d AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4v\n",
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
        cof_deliver(co, 0x585, (uint8_t)(data[0] % 9U), data + 1);
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
