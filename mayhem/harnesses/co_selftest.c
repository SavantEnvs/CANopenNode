/* Known-answer self-test of the CANopenNode stack, built with normal flags and run by mayhem/test.sh.
 * Each check drives the stack through its public interfaces (CAN frames to the receive callbacks,
 * gateway text) and asserts the exact frames or text it answers with. Prints "PASS <name>" or
 * "FAIL <name>: <why>" per check and "SUMMARY <passed> <failed>". */
#include <stdio.h>
#include <string.h>

#include "co_fuzz.h"
#include "OD.h"

static int passed, failed;

static void
check(const char* name, int ok, const char* why) {
    if (ok) {
        printf("PASS %s\n", name);
        passed++;
    } else {
        printf("FAIL %s: %s\n", name, why);
        failed++;
    }
}

/* Index of the last transmitted frame with this ident, or -1. */
static int
last_tx(uint16_t ident) {
    uint32_t n = cof_txCount < COF_TX_LOG ? cof_txCount : COF_TX_LOG;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t i = (cof_txCount - 1U - k) % COF_TX_LOG;
        if (cof_txLog[i].ident == ident) {
            return (int)i;
        }
    }
    return -1;
}

static int
tx_equals(uint16_t ident, uint8_t dlc, const uint8_t* expect) {
    int i = last_tx(ident);
    return i >= 0 && cof_txLog[i].DLC == dlc && memcmp(cof_txLog[i].data, expect, dlc) == 0;
}

static void
sdo_request(CO_t* co, const uint8_t req[8]) {
    cof_deliver(co, 0x600 + COF_NODE_ID, 8, req);
    cof_step(co, 1000);
    cof_step(co, 1000);
}

static CO_t*
fresh(void) {
    CO_t* co = cof_init();
    if (co != NULL) {
        for (int i = 0; i < 5; i++) {
            cof_step(co, 1000);
        }
    }
    return co;
}

static void
gtw(CO_t* co, const char* text) {
    cof_gtwOutLen = 0;
    cof_gtwOut[0] = '\0';
    cof_gtw_write(co, text, strlen(text), 1000);
    for (int i = 0; i < 20; i++) {
        cof_step(co, 1000);
    }
}

int
main(void) {
    CO_t* co = fresh();
    if (co == NULL) {
        printf("FAIL init: CO stack initialization failed\nSUMMARY 0 1\n");
        return 1;
    }

    {
        const uint8_t boot[1] = {0x00};
        check("nmt_bootup_frame", tx_equals(0x700 + COF_NODE_ID, 1, boot), "no boot-up frame 0x70A [00]");
        check("nmt_operational", co->NMT->operatingState == CO_NMT_OPERATIONAL, "node not operational after start");
    }
    {
        const uint8_t req[8] = {0x40, 0x18, 0x10, 0x00, 0, 0, 0, 0};
        const uint8_t rsp[8] = {0x4F, 0x18, 0x10, 0x00, 0x04, 0, 0, 0};
        sdo_request(co, req);
        check("sdo_upload_u8", tx_equals(0x580 + COF_NODE_ID, 8, rsp), "0x1018/0 upload did not answer 4F 18 10 00 04");
    }
    {
        const uint8_t req[8] = {0x40, 0x05, 0x10, 0x00, 0, 0, 0, 0};
        const uint8_t rsp[8] = {0x43, 0x05, 0x10, 0x00, 0x80, 0, 0, 0};
        sdo_request(co, req);
        check("sdo_upload_u32", tx_equals(0x580 + COF_NODE_ID, 8, rsp), "0x1005/0 upload did not answer 43 05 10 00 80");
    }
    {
        const uint8_t req[8] = {0x2B, 0x17, 0x10, 0x00, 0x34, 0x12, 0, 0};
        const uint8_t rsp[8] = {0x60, 0x17, 0x10, 0x00, 0, 0, 0, 0};
        sdo_request(co, req);
        check("sdo_download_u16", tx_equals(0x580 + COF_NODE_ID, 8, rsp) && OD_PERSIST_COMM.x1017_producerHeartbeatTime == 0x1234,
              "0x1017 download was not acknowledged or not stored");
        const uint8_t req2[8] = {0x40, 0x17, 0x10, 0x00, 0, 0, 0, 0};
        const uint8_t rsp2[8] = {0x4B, 0x17, 0x10, 0x00, 0x34, 0x12, 0, 0};
        sdo_request(co, req2);
        check("sdo_readback_u16", tx_equals(0x580 + COF_NODE_ID, 8, rsp2), "0x1017 read back differs");
    }
    {
        const uint8_t req[8] = {0x40, 0x55, 0x55, 0x00, 0, 0, 0, 0};
        const uint8_t rsp[8] = {0x80, 0x55, 0x55, 0x00, 0x00, 0x00, 0x02, 0x06};
        sdo_request(co, req);
        check("sdo_abort_no_object", tx_equals(0x580 + COF_NODE_ID, 8, rsp), "missing object did not abort 06020000");
    }
    {
        const uint8_t nmt[2] = {0x80, COF_NODE_ID};
        cof_deliver(co, 0x000, 2, nmt);
        cof_step(co, 1000);
        check("nmt_enter_preop", co->NMT->operatingState == CO_NMT_PRE_OPERATIONAL, "NMT 0x80 did not enter pre-operational");
    }
    {
        gtw(co, "[1] 10 r 0x1018 0 u8\n");
        check("gtw_read_local", strcmp(cof_gtwOut, "[1] 4\r\n") == 0, cof_gtwOut);
        gtw(co, "[2] 10 w 0x1017 0 u16 500\n");
        check("gtw_write_local", strcmp(cof_gtwOut, "[2] OK\r\n") == 0 && OD_PERSIST_COMM.x1017_producerHeartbeatTime == 500,
              cof_gtwOut);
        gtw(co, "[3] 10 r 0x5555 0\n");
        check("gtw_error_desc", strncmp(cof_gtwOut, "[3] ERROR:0x06020000", 20) == 0, cof_gtwOut);
        gtw(co, "[4] 10 r 0x1005 0 x32\n");
        check("gtw_read_hex", strcmp(cof_gtwOut, "[4] 0x00000080\r\n") == 0, cof_gtwOut);
        gtw(co, "[5] bogus\n");
        check("gtw_syntax_error", strncmp(cof_gtwOut, "[5] ERROR:", 10) == 0, cof_gtwOut);
    }
    cof_fini(co);

    /* A fresh stack starts from the example OD's initial values again. */
    co = fresh();
    check("od_restored", co != NULL && OD_PERSIST_COMM.x1017_producerHeartbeatTime == 0, "OD not restored by cof_init");
    cof_fini(co);

    printf("SUMMARY %d %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
