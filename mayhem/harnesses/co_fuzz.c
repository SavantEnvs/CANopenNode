/* Shared runtime of the CANopenNode fuzz harnesses and the self-test.
 *
 * Driver: the functions of example/CO_driver_blank.c, for a host without CAN hardware. Received frames
 * are handed to the stack by cof_deliver() exactly as the blank driver's CO_CANinterrupt() would
 * (first receive buffer whose ident/mask matches); transmitted frames are recorded in cof_txLog and
 * never stay pending.
 *
 * State per input: cof_init() restores the example OD's RAM/persistent sections (OD_RAM,
 * OD_PERSIST_COMM, which SDO/PDO writes change) from a snapshot taken on the first call, and every
 * stack object is allocated fresh by CO_new() and freed by CO_delete(). Nothing else in the stack is
 * kept across inputs. */
#include <stdbool.h>
#include <string.h>

#include "co_fuzz.h"
#include "OD.h"

cof_frame_t cof_txLog[COF_TX_LOG];
uint32_t cof_txCount;
char cof_gtwOut[COF_GTW_OUT];
size_t cof_gtwOutLen;

/* ------------------------------------------------------------------ driver */

void
CO_CANsetConfigurationMode(void* CANptr) {
    (void)CANptr;
}

void
CO_CANsetNormalMode(CO_CANmodule_t* CANmodule) {
    CANmodule->CANnormal = true;
}

CO_ReturnError_t
CO_CANmodule_init(CO_CANmodule_t* CANmodule, void* CANptr, CO_CANrx_t rxArray[], uint16_t rxSize, CO_CANtx_t txArray[],
                  uint16_t txSize, uint16_t CANbitRate) {
    (void)CANbitRate;
    if (CANmodule == NULL || rxArray == NULL || txArray == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;
    for (uint16_t i = 0U; i < rxSize; i++) {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (uint16_t i = 0U; i < txSize; i++) {
        txArray[i].bufferFull = false;
    }
    return CO_ERROR_NO;
}

void
CO_CANmodule_disable(CO_CANmodule_t* CANmodule) {
    (void)CANmodule;
}

CO_ReturnError_t
CO_CANrxBufferInit(CO_CANmodule_t* CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr, void* object,
                   void (*CANrx_callback)(void* object, void* message)) {
    if ((CANmodule == NULL) || (object == NULL) || (CANrx_callback == NULL) || (index >= CANmodule->rxSize)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    CO_CANrx_t* buffer = &CANmodule->rxArray[index];
    buffer->object = object;
    buffer->CANrx_callback = CANrx_callback;
    buffer->ident = ident & 0x07FFU;
    if (rtr) {
        buffer->ident |= 0x0800U;
    }
    buffer->mask = (mask & 0x07FFU) | 0x0800U;
    return CO_ERROR_NO;
}

CO_CANtx_t*
CO_CANtxBufferInit(CO_CANmodule_t* CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                   bool_t syncFlag) {
    if ((CANmodule == NULL) || (index >= CANmodule->txSize)) {
        return NULL;
    }
    CO_CANtx_t* buffer = &CANmodule->txArray[index];
    buffer->ident = ((uint32_t)ident & 0x07FFU) | ((uint32_t)(((uint32_t)noOfBytes & 0xFU) << 11U))
                    | ((uint32_t)(rtr ? 0x8000U : 0U));
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    return buffer;
}

CO_ReturnError_t
CO_CANsend(CO_CANmodule_t* CANmodule, CO_CANtx_t* buffer) {
    cof_frame_t* f = &cof_txLog[cof_txCount % COF_TX_LOG];
    uint8_t dlc = (uint8_t)((buffer->ident >> 11U) & 0xFU);
    f->ident = (uint16_t)(buffer->ident & 0x07FFU);
    f->DLC = dlc;
    memset(f->data, 0, sizeof(f->data));
    memcpy(f->data, buffer->data, dlc <= 8U ? dlc : 8U);
    cof_txCount++;
    CANmodule->bufferInhibitFlag = buffer->syncFlag;
    CANmodule->firstCANtxMessage = false;
    return CO_ERROR_NO;
}

void
CO_CANclearPendingSyncPDOs(CO_CANmodule_t* CANmodule) {
    if (CANmodule->bufferInhibitFlag) {
        CANmodule->bufferInhibitFlag = false;
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}

void
CO_CANmodule_process(CO_CANmodule_t* CANmodule) {
    (void)CANmodule;
}

void
cof_deliver(CO_t* co, uint16_t ident, uint8_t dlc, const uint8_t* data) {
    CO_CANmodule_t* m = co->CANmodule;
    CO_CANrxMsg_t msg;
    msg.ident = (uint16_t)(ident & 0x0FFFU);
    msg.DLC = dlc > 8U ? 8U : dlc;
    memset(msg.data, 0, sizeof(msg.data));
    memcpy(msg.data, data, msg.DLC);
    if (!m->CANnormal) {
        return;
    }
    for (uint16_t i = 0U; i < m->rxSize; i++) {
        CO_CANrx_t* b = &m->rxArray[i];
        if (b->CANrx_callback != NULL && (((msg.ident ^ b->ident) & b->mask) == 0U)) {
            b->CANrx_callback(b->object, &msg);
            break;
        }
    }
}

/* --------------------------------------------------------------- lifecycle */

static OD_RAM_t od_ram_initial;
static OD_PERSIST_COMM_t od_persist_initial;
static bool od_saved;

static size_t
gtw_read(void* object, const char* buf, size_t count, uint8_t* connectionOK) {
    (void)object;
    if (connectionOK != NULL) {
        *connectionOK = 1;
    }
    size_t room = COF_GTW_OUT - 1U - cof_gtwOutLen;
    size_t n = count < room ? count : room;
    memcpy(cof_gtwOut + cof_gtwOutLen, buf, n);
    cof_gtwOutLen += n;
    cof_gtwOut[cof_gtwOutLen] = '\0';
    return count; /* the rest is discarded, as a closed terminal would */
}

static uint8_t pendingNodeId;
static uint16_t pendingBitRate;

CO_t*
cof_init(void) {
    if (!od_saved) {
        od_ram_initial = OD_RAM;
        od_persist_initial = OD_PERSIST_COMM;
        od_saved = true;
    } else {
        OD_RAM = od_ram_initial;
        OD_PERSIST_COMM = od_persist_initial;
    }
    cof_txCount = 0;
    memset(cof_txLog, 0, sizeof(cof_txLog));
    cof_gtwOutLen = 0;
    cof_gtwOut[0] = '\0';
    pendingNodeId = COF_NODE_ID;
    pendingBitRate = 125;

    uint32_t heap = 0;
    CO_t* co = CO_new(NULL, &heap);
    if (co == NULL) {
        return NULL;
    }
    if (CO_CANinit(co, NULL, pendingBitRate) != CO_ERROR_NO) {
        goto fail;
    }
    CO_LSS_address_t lssAddress = {.identity = {.vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
                                                .productCode = OD_PERSIST_COMM.x1018_identity.productCode,
                                                .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
                                                .serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber}};
    if (CO_LSSinit(co, &lssAddress, &pendingNodeId, &pendingBitRate) != CO_ERROR_NO) {
        goto fail;
    }
    uint32_t errInfo = 0;
    CO_ReturnError_t err = CO_CANopenInit(co, NULL, NULL, OD, NULL,
                                          CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_ERR_REG | CO_ERR_REG_GENERIC_ERR
                                              | CO_ERR_REG_COMMUNICATION,
                                          500, 1000, 500, false, pendingNodeId, &errInfo);
    if (err != CO_ERROR_NO) {
        goto fail;
    }
    if (CO_CANopenInitPDO(co, co->em, OD, pendingNodeId, &errInfo) != CO_ERROR_NO) {
        goto fail;
    }
    CO_GTWA_initRead(co->gtwa, gtw_read, NULL);
    CO_CANsetNormalMode(co->CANmodule);
    return co;
fail:
    CO_delete(co);
    return NULL;
}

void
cof_fini(CO_t* co) {
    if (co != NULL) {
        CO_CANsetConfigurationMode(NULL);
        CO_delete(co);
    }
}

CO_NMT_reset_cmd_t
cof_step(CO_t* co, uint32_t timeDifference_us) {
    CO_NMT_reset_cmd_t reset = CO_process(co, true, timeDifference_us, NULL);
    if (!co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
        bool_t syncWas = CO_process_SYNC(co, timeDifference_us, NULL);
        CO_process_RPDO(co, syncWas, timeDifference_us, NULL);
        CO_process_TPDO(co, syncWas, timeDifference_us, NULL);
    }
    return reset;
}

CO_NMT_reset_cmd_t
cof_gtw_write(CO_t* co, const char* text, size_t len, uint32_t timeDifference_us) {
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    unsigned stalls = 0;
    while (len > 0U && reset == CO_RESET_NOT) {
        size_t space = CO_GTWA_write_getSpace(co->gtwa);
        size_t n = len < space ? len : space;
        if (n > 0U) {
            n = CO_GTWA_write(co->gtwa, text, n);
            text += n;
            len -= n;
            stalls = 0;
        } else if (++stalls > 1000U) {
            break; /* the gateway stopped consuming input */
        }
        reset = cof_step(co, timeDifference_us);
    }
    return reset;
}
