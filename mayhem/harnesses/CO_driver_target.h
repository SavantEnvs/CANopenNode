/*
 * CO_driver_target.h for the Mayhem fuzz harnesses and the self-test.
 *
 * Takes the place of example/CO_driver_target.h (the harness directory comes first on the include
 * path). It differs from the blank example only where the blank one cannot carry a received frame:
 * CO_CANrxMsg_read*() read a real CO_CANrxMsg_t, and the stack configuration enables every module
 * that reads data from the bus or the gateway (SDO server/client incl. block transfer, LSS slave and
 * master, NMT master, EMCY consumer, node guarding, the CiA 309-3 ASCII gateway and its FIFO).
 */
#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Stack configuration (CO_config.h overrides) ---- */
#define CO_CONFIG_NMT (CO_CONFIG_NMT_CALLBACK_CHANGE | CO_CONFIG_NMT_MASTER | CO_CONFIG_FLAG_TIMERNEXT)
#define CO_CONFIG_HB_CONS                                                                                              \
    (CO_CONFIG_HB_CONS_ENABLE | CO_CONFIG_HB_CONS_CALLBACK_CHANGE \
     | CO_CONFIG_HB_CONS_QUERY_FUNCT | CO_CONFIG_FLAG_TIMERNEXT | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_EM                                                                                                   \
    (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_PROD_CONFIGURABLE | CO_CONFIG_EM_PROD_INHIBIT | CO_CONFIG_EM_HISTORY         \
     | CO_CONFIG_EM_STATUS_BITS | CO_CONFIG_EM_CONSUMER | CO_CONFIG_FLAG_TIMERNEXT)
#define CO_CONFIG_SDO_SRV                                                                                              \
    (CO_CONFIG_SDO_SRV_SEGMENTED | CO_CONFIG_SDO_SRV_BLOCK | CO_CONFIG_FLAG_TIMERNEXT | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 900
#define CO_CONFIG_SDO_CLI                                                                                              \
    (CO_CONFIG_SDO_CLI_ENABLE | CO_CONFIG_SDO_CLI_SEGMENTED | CO_CONFIG_SDO_CLI_BLOCK | CO_CONFIG_SDO_CLI_LOCAL        \
     | CO_CONFIG_FLAG_TIMERNEXT | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 1000
#define CO_CONFIG_TIME (CO_CONFIG_TIME_ENABLE | CO_CONFIG_TIME_PRODUCER | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_SYNC (CO_CONFIG_SYNC_ENABLE | CO_CONFIG_SYNC_PRODUCER | CO_CONFIG_FLAG_TIMERNEXT | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_PDO                                                                                                  \
    (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE | CO_CONFIG_RPDO_TIMERS_ENABLE | CO_CONFIG_TPDO_TIMERS_ENABLE       \
     | CO_CONFIG_PDO_SYNC_ENABLE | CO_CONFIG_PDO_OD_IO_ACCESS | CO_CONFIG_FLAG_TIMERNEXT | CO_CONFIG_FLAG_OD_DYNAMIC)
#define CO_CONFIG_LSS (CO_CONFIG_LSS_SLAVE | CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND | CO_CONFIG_LSS_MASTER)
#define CO_CONFIG_GTW                                                                                                  \
    (CO_CONFIG_GTW_ASCII | CO_CONFIG_GTW_ASCII_SDO | CO_CONFIG_GTW_ASCII_NMT | CO_CONFIG_GTW_ASCII_LSS                 \
     | CO_CONFIG_GTW_ASCII_LOG | CO_CONFIG_GTW_ASCII_ERROR_DESC | CO_CONFIG_GTW_ASCII_PRINT_HELP                       \
     | CO_CONFIG_GTW_ASCII_PRINT_LEDS)
#define CO_CONFIG_GTW_BLOCK_DL_LOOP  3
#define CO_CONFIG_GTWA_COMM_BUF_SIZE 2000
#define CO_CONFIG_GTWA_LOG_BUF_SIZE  2000
#define CO_CONFIG_CRC16              (CO_CONFIG_CRC16_ENABLE)
#define CO_CONFIG_FIFO                                                                                                 \
    (CO_CONFIG_FIFO_ENABLE | CO_CONFIG_FIFO_ALT_READ | CO_CONFIG_FIFO_CRC16_CCITT | CO_CONFIG_FIFO_ASCII_COMMANDS      \
     | CO_CONFIG_FIFO_ASCII_DATATYPES)

/* Basic definitions (little-endian host). */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

/* A received CAN frame, as the harness hands it to the stack's receive callbacks. */
typedef struct {
    uint16_t ident; /* 11-bit identifier, 0x800 = RTR */
    uint8_t DLC;
    uint8_t data[8];
} CO_CANrxMsg_t;

#define CO_CANrxMsg_readIdent(msg) ((uint16_t)(((const CO_CANrxMsg_t*)(msg))->ident & 0x07FFU))
#define CO_CANrxMsg_readDLC(msg)   ((uint8_t)(((const CO_CANrxMsg_t*)(msg))->DLC))
#define CO_CANrxMsg_readData(msg)  ((const uint8_t*)(((const CO_CANrxMsg_t*)(msg))->data))

typedef struct {
    uint16_t ident;
    uint16_t mask;
    void* object;
    void (*CANrx_callback)(void* object, void* message);
} CO_CANrx_t;

typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

typedef struct {
    void* CANptr;
    CO_CANrx_t* rxArray;
    uint16_t rxSize;
    CO_CANtx_t* txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
} CO_CANmodule_t;

typedef struct {
    void* addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    void* addrNV;
} CO_storage_entry_t;

#define CO_LOCK_CAN_SEND(CAN_MODULE)
#define CO_UNLOCK_CAN_SEND(CAN_MODULE)
#define CO_LOCK_EMCY(CAN_MODULE)
#define CO_UNLOCK_EMCY(CAN_MODULE)
#define CO_LOCK_OD(CAN_MODULE)
#define CO_UNLOCK_OD(CAN_MODULE)

#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                                                                                             \
    {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = (void*)1L;                                                                                             \
    }
#define CO_FLAG_CLEAR(rxNew)                                                                                           \
    {                                                                                                                  \
        CO_MemoryBarrier();                                                                                            \
        rxNew = NULL;                                                                                                  \
    }

#ifdef __cplusplus
}
#endif

#endif /* CO_DRIVER_TARGET_H */
