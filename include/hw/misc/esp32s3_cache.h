/*
 * ESP32-S3 ICache emulation
 *
 * Copyright (c) 2024 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#pragma once

#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/registerfields.h"
#include "hw/misc/esp32s3_xts_aes.h"

#define TYPE_ESP32S3_CACHE "esp32s3.icache"
#define TYPE_ESP32S3_DCACHE "esp32s3.dcache"
#define ESP32S3_CACHE(obj)           OBJECT_CHECK(ESP32S3CacheState, (obj), TYPE_ESP32S3_CACHE)
#define ESP32S3_CACHE_GET_CLASS(obj) OBJECT_GET_CLASS(ESP32S3CacheState, obj, TYPE_ESP32S3_CACHE)
#define ESP32S3_CACHE_CLASS(klass)   OBJECT_CLASS_CHECK(ESP32S3CacheState, klass, TYPE_ESP32S3_CACHE)

#define ESP32S3_DCACHE_BASE 0x3c000000
#define ESP32S3_ICACHE_BASE 0x42000000

/* The MMU is shared between by both the ICache and the DCache */
#define ESP32S3_MMU_SIZE 0x800
#define ESP32S3_ICACHE_MMU_SIZE                 0x800
#define ESP32S3_DCACHE_MMU_SIZE                 0x800

/* Each MMU virtual page is 64KB big on the ESP32S3 */
#define ESP32S3_PAGE_SIZE (64*1024)

/* Offset of the MMU table in I/O memory range
 * As the MMU table is mapped in the same MemoryRegion as the Cache I/O registers, its base offset
 * will not be 0 */
#define ESP32S3_MMU_TABLE_OFFSET (DR_REG_MMU_TABLE - DR_REG_EXTMEM_BASE)

/* Number of entries count in the table size */
#define ESP32S3_MMU_TABLE_ENTRY_COUNT (ESP32S3_MMU_SIZE/sizeof(uint32_t))

typedef union ESP32S3MMUEntry {
    struct {
        uint32_t    page_number : 8;
        uint32_t    invalid     : 1;
        uint32_t    reserved    : 23;   /* Must always be 0 */
    };
    uint32_t val;
} ESP32S3MMUEntry;

_Static_assert(sizeof(ESP32S3MMUEntry) == sizeof(uint32_t), "MMU Entry size must be 4 bytes");

/**
 * The external memory region on the ESP32-C3 is 2x32MB, but both regions are shared (data & instructions)
 */
// #define ESP32S3_EXTMEM_REGION_SIZE        0x800000
#define ESP32S3_EXTMEM_REGION_SIZE        0x2000000

/**
 * The cache on ESP32-C3 has a size of 16KB, and the block size is 32 bytes (1024 blocks)
 */
#define ESP32S3_CACHE_BLOCK_SIZE    32
#define ESP32S3_CACHE_BLOCK_COUNT   512
#define ESP32S3_CACHE_SIZE          (ESP32S3_CACHE_BLOCK_COUNT * ESP32S3_CACHE_BLOCK_SIZE)

/**
 * Size of the Cache I/O registers area
 */
#define TYPE_ESP32S3_CACHE_IO_SIZE 0x1000

/**
 * Even though the size of the I/O range for extmem cache is 0x1000, the registers actually go
 * up to 0x100.
 */
#define ESP32S3_CACHE_REG_COUNT (0x200 / sizeof(uint32_t))

/**
 * Convert a register address to its index in the registers array
 */
#define ESP32S3_CACHE_REG_IDX(addr) ((addr) / sizeof(uint32_t))

typedef struct {
    SysBusDevice parent;
    BlockBackend *flash_blk;
    MemoryRegion iomem;

    bool         icache_enable;
    bool         dcache_enable;
    hwaddr       dcache_base;
    hwaddr       icache_base;
    MemoryRegion dcache;
    MemoryRegion icache;
    MemoryRegion psram;

    /* Registers for controlling the cache */
    uint32_t regs[ESP32S3_CACHE_REG_COUNT];

    ESP32S3XtsAesState *xts_aes;
    /* Define the MMU itself as an array, it shall be accessible from address ESP32S3_MMU_TABLE */
    ESP32S3MMUEntry mmu[ESP32S3_MMU_TABLE_ENTRY_COUNT];
} ESP32S3CacheState;

/* Assert that the size of the MMU table in the structure is of size ESP32S3_MMU_SIZE */
_Static_assert(sizeof(((ESP32S3CacheState*)0)->mmu) == ESP32S3_MMU_SIZE,
               "The size of `mmu` field in structure ESP32C3CacheState must be equal to ESP32S3_MMU_SIZE");


REG32(EXTMEM_DCACHE_CTRL, 0x000)
    FIELD(EXTMEM_DCACHE_CTRL, ENABLE, 0, 1)

REG32(EXTMEM_DCACHE_CTRL1, 0x004)
    FIELD(EXTMEM_DCACHE_CTRL1, SHUT_DBUS, 1, 1)
    FIELD(EXTMEM_DCACHE_CTRL1, SHUT_IBUS, 0, 1)

REG32(EXTMEM_DCACHE_TAG_POWER_CTRL, 0x008)
    FIELD(EXTMEM_DCACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_PU, 2, 1)
    FIELD(EXTMEM_DCACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_PD, 1, 1)
    FIELD(EXTMEM_DCACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_ON, 0, 1)

REG32(EXTMEM_DCACHE_PRELOCK_CTRL, 0x00C)
    FIELD(EXTMEM_DCACHE_PRELOCK_CTRL, PRELOCK_SCT1_EN, 1, 1)
    FIELD(EXTMEM_DCACHE_PRELOCK_CTRL, PRELOCK_SCT0_EN, 0, 1)

REG32(EXTMEM_DCACHE_PRELOCK_SCT0_ADDR, 0x010)
    FIELD(EXTMEM_DCACHE_PRELOCK_SCT0_ADDR, PRELOCK_SCT0_ADDR, 0, 32)

REG32(EXTMEM_DCACHE_PRELOCK_SCT1_ADDR, 0x014)
    FIELD(EXTMEM_DCACHE_PRELOCK_SCT1_ADDR, PRELOCK_SCT1_ADDR, 0, 32)

REG32(EXTMEM_DCACHE_PRELOCK_SCT_SIZE, 0x018)
    FIELD(EXTMEM_DCACHE_PRELOCK_SCT_SIZE, PRELOCK_SCT0_SIZE, 16, 16)
    FIELD(EXTMEM_DCACHE_PRELOCK_SCT_SIZE, PRELOCK_SCT1_SIZE, 0, 16)

REG32(EXTMEM_DCACHE_LOCK_CTRL, 0x01C)
    FIELD(EXTMEM_DCACHE_LOCK_CTRL, LOCK_DONE, 2, 1)
    FIELD(EXTMEM_DCACHE_LOCK_CTRL, UNLOCK_ENA, 1, 1)
    FIELD(EXTMEM_DCACHE_LOCK_CTRL, LOCK_ENA, 0, 1)

REG32(EXTMEM_DCACHE_LOCK_ADDR, 0x020)
    FIELD(EXTMEM_DCACHE_LOCK_ADDR, LOCK_ADDR, 0, 32)

REG32(EXTMEM_DCACHE_LOCK_SIZE, 0x024)
    FIELD(EXTMEM_DCACHE_LOCK_SIZE, LOCK_SIZE, 0, 16)

REG32(EXTMEM_DCACHE_SYNC_CTRL, 0x028)
    FIELD(EXTMEM_DCACHE_SYNC_CTRL, SYNC_DONE, 1, 1)
    FIELD(EXTMEM_DCACHE_SYNC_CTRL, INVALIDATE_ENA, 0, 1)

REG32(EXTMEM_DCACHE_SYNC_ADDR, 0x02C)
    FIELD(EXTMEM_DCACHE_SYNC_ADDR, SYNC_ADDR, 0, 32)


REG32(EXTMEM_DCACHE_SYNC_SIZE, 0x030)
    FIELD(EXTMEM_DCACHE_SYNC_SIZE, SYNC_SIZE, 0, 23)


REG32(EXTMEM_DCACHE_PRELOAD_CTRL, 0x040)
    FIELD(EXTMEM_DCACHE_PRELOAD_CTRL, PRELOAD_ORDER, 2, 1)
    FIELD(EXTMEM_DCACHE_PRELOAD_CTRL, PRELOAD_DONE, 1, 1)
    FIELD(EXTMEM_DCACHE_PRELOAD_CTRL, PRELOAD_ENA, 0, 1)

REG32(EXTMEM_DCACHE_AUTOLOAD_CTRL, 0x04c)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_RQST, 5, 2)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_ORDER, 4, 1)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_DONE, 3, 1)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_ENA, 2, 1)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_SCT1_ENA, 1, 1)
    FIELD(EXTMEM_DCACHE_AUTOLOAD_CTRL, AUTOLOAD_SCT0_ENA, 0, 1)


// -----


REG32(EXTMEM_ICACHE_CTRL, 0x000 + 0x060)
    FIELD(EXTMEM_ICACHE_CTRL, ENABLE, 0, 1)

REG32(EXTMEM_ICACHE_CTRL1, 0x004 + 0x060)
    FIELD(EXTMEM_ICACHE_CTRL1, SHUT_DBUS, 1, 1)
    FIELD(EXTMEM_ICACHE_CTRL1, SHUT_IBUS, 0, 1)

REG32(EXTMEM_ICACHE_TAG_POWER_CTRL, 0x008 + 0x060)
    FIELD(EXTMEM_ICACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_PU, 2, 1)
    FIELD(EXTMEM_ICACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_PD, 1, 1)
    FIELD(EXTMEM_ICACHE_TAG_POWER_CTRL, TAG_MEM_FORCE_ON, 0, 1)

REG32(EXTMEM_ICACHE_PRELOCK_CTRL, 0x00C + 0x060)
    FIELD(EXTMEM_ICACHE_PRELOCK_CTRL, PRELOCK_SCT1_EN, 1, 1)
    FIELD(EXTMEM_ICACHE_PRELOCK_CTRL, PRELOCK_SCT0_EN, 0, 1)

REG32(EXTMEM_ICACHE_PRELOCK_SCT0_ADDR, 0x70)
    FIELD(EXTMEM_ICACHE_PRELOCK_SCT0_ADDR, PRELOCK_SCT0_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_PRELOCK_SCT1_ADDR, 0x074)
    FIELD(EXTMEM_ICACHE_PRELOCK_SCT1_ADDR, PRELOCK_SCT1_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_PRELOCK_SCT_SIZE, 0x078)
    FIELD(EXTMEM_ICACHE_PRELOCK_SCT_SIZE, PRELOCK_SCT0_SIZE, 16, 16)
    FIELD(EXTMEM_ICACHE_PRELOCK_SCT_SIZE, PRELOCK_SCT1_SIZE, 0, 16)

REG32(EXTMEM_ICACHE_LOCK_CTRL, 0x07C)
    FIELD(EXTMEM_ICACHE_LOCK_CTRL, LOCK_DONE, 2, 1)
    FIELD(EXTMEM_ICACHE_LOCK_CTRL, UNLOCK_ENA, 1, 1)
    FIELD(EXTMEM_ICACHE_LOCK_CTRL, LOCK_ENA, 0, 1)

REG32(EXTMEM_ICACHE_LOCK_ADDR, 0x080)
    FIELD(EXTMEM_ICACHE_LOCK_ADDR, LOCK_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_LOCK_SIZE, 0x084)
    FIELD(EXTMEM_ICACHE_LOCK_SIZE, LOCK_SIZE, 0, 16)

REG32(EXTMEM_ICACHE_SYNC_CTRL, 0x88)
    FIELD(EXTMEM_ICACHE_SYNC_CTRL, SYNC_DONE, 1, 1)
    FIELD(EXTMEM_ICACHE_SYNC_CTRL, INVALIDATE_ENA, 0, 1)

REG32(EXTMEM_ICACHE_SYNC_ADDR, 0x6C)
    FIELD(EXTMEM_ICACHE_SYNC_ADDR, SYNC_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_SYNC_SIZE, 0x090)
    FIELD(EXTMEM_ICACHE_SYNC_SIZE, SYNC_SIZE, 0, 23)

REG32(EXTMEM_ICACHE_PRELOAD_CTRL, 0x094)
    FIELD(EXTMEM_ICACHE_PRELOAD_CTRL, PRELOAD_ORDER, 2, 1)
    FIELD(EXTMEM_ICACHE_PRELOAD_CTRL, PRELOAD_DONE, 1, 1)
    FIELD(EXTMEM_ICACHE_PRELOAD_CTRL, PRELOAD_ENA, 0, 1)

REG32(EXTMEM_ICACHE_PRELOAD_ADDR, 0x98)
    FIELD(EXTMEM_ICACHE_PRELOAD_ADDR, PRELOAD_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_PRELOAD_SIZE, 0x9C)
    FIELD(EXTMEM_ICACHE_PRELOAD_SIZE, PRELOAD_SIZE, 0, 16)

REG32(EXTMEM_ICACHE_AUTOLOAD_CTRL, 0x0A0)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_RQST, 5, 2)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_ORDER, 4, 1)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_DONE, 3, 1)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_ENA, 2, 1)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_SCT1_ENA, 1, 1)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_CTRL, AUTOLOAD_SCT0_ENA, 0, 1)

REG32(EXTMEM_ICACHE_AUTOLOAD_SCT0_ADDR, 0x0A4)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_SCT0_ADDR, AUTOLOAD_SCT0_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_AUTOLOAD_SCT0_SIZE, 0x0A8)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_SCT0_SIZE, AUTOLOAD_SCT0_SIZE, 0, 27)

REG32(EXTMEM_ICACHE_AUTOLOAD_SCT1_ADDR, 0x0AC)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_SCT1_ADDR, AUTOLOAD_SCT1_ADDR, 0, 32)

REG32(EXTMEM_ICACHE_AUTOLOAD_SCT1_SIZE, 0x0B0)
    FIELD(EXTMEM_ICACHE_AUTOLOAD_SCT1_SIZE, AUTOLOAD_SCT1_SIZE, 0, 27)

REG32(EXTMEM_IBUS_TO_FLASH_START_VADDR, 0x0B4)
    FIELD(EXTMEM_IBUS_TO_FLASH_START_VADDR, IBUS_TO_FLASH_START_VADDR, 0, 32)

REG32(EXTMEM_IBUS_TO_FLASH_END_VADDR, 0x0B8)
    FIELD(EXTMEM_IBUS_TO_FLASH_END_VADDR, IBUS_TO_FLASH_END_VADDR, 0, 32)

REG32(EXTMEM_DBUS_TO_FLASH_START_VADDR, 0x0BC)
    FIELD(EXTMEM_DBUS_TO_FLASH_START_VADDR, DBUS_TO_FLASH_START_VADDR, 0, 32)

REG32(EXTMEM_DBUS_TO_FLASH_END_VADDR, 0x0C0)
    FIELD(EXTMEM_DBUS_TO_FLASH_END_VADDR, DBUS_TO_FLASH_END_VADDR, 0, 32)

REG32(EXTMEM_CACHE_ACS_CNT_CLR, 0x0C4)
    FIELD(EXTMEM_CACHE_ACS_CNT_CLR, DBUS_ACS_CNT_CLR, 1, 1)
    FIELD(EXTMEM_CACHE_ACS_CNT_CLR, IBUS_ACS_CNT_CLR, 0, 1)

REG32(EXTMEM_IBUS_ACS_MISS_CNT, 0x0C8)
    FIELD(EXTMEM_IBUS_ACS_MISS_CNT, IBUS_ACS_MISS_CNT, 0, 32)

REG32(EXTMEM_IBUS_ACS_CNT, 0x0CC)
    FIELD(EXTMEM_IBUS_ACS_CNT, IBUS_ACS_CNT, 0, 32)

REG32(EXTMEM_DBUS_ACS_FLASH_MISS_CNT, 0x0D0)
    FIELD(EXTMEM_DBUS_ACS_FLASH_MISS_CNT, DBUS_ACS_FLASH_MISS_CNT, 0, 32)

REG32(EXTMEM_DBUS_ACS_SPIRAM_MISS_CNT, 0x0D4)
    FIELD(EXTMEM_DBUS_ACS_SPIRAM_MISS_CNT, DBUS_ACS_SPIRAM_MISS_CNT, 0, 32)


REG32(EXTMEM_DBUS_ACS_CNT, 0x0D8)
    FIELD(EXTMEM_DBUS_ACS_CNT, DBUS_ACS_CNT, 0, 32)

REG32(EXTMEM_CACHE_ILG_INT_ENA, 0x0DC)
    FIELD(EXTMEM_CACHE_ILG_INT_ENA, DBUS_CNT_OVF_INT_ENA, 8, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ENA, IBUS_CNT_OVF_INT_ENA, 7, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ENA, MMU_ENTRY_FAULT_INT_ENA, 5, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ENA, ICACHE_PRELOAD_OP_FAULT_INT_ENA, 1, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ENA, ICACHE_SYNC_OP_FAULT_INT_ENA, 0, 1)

REG32(EXTMEM_CACHE_ILG_INT_CLR, 0x0E0)
    FIELD(EXTMEM_CACHE_ILG_INT_CLR, DBUS_CNT_OVF_INT_CLR, 8, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_CLR, IBUS_CNT_OVF_INT_CLR, 7, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_CLR, MMU_ENTRY_FAULT_INT_CLR, 5, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_CLR, ICACHE_PRELOAD_OP_FAULT_INT_CLR, 1, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_CLR, ICACHE_SYNC_OP_FAULT_INT_CLR, 0, 1)

REG32(EXTMEM_CACHE_ILG_INT_ST, 0x0E4)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, DBUS_ACS_FLASH_MISS_CNT_OVF_ST, 10, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, DBUS_ACS_CNT_OVF_ST, 9, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, IBUS_ACS_MISS_CNT_OVF_ST, 8, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, IBUS_ACS_CNT_OVF_ST, 7, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, MMU_ENTRY_FAULT_ST, 5, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, ICACHE_PRELOAD_OP_FAULT_ST, 1, 1)
    FIELD(EXTMEM_CACHE_ILG_INT_ST, ICACHE_SYNC_OP_FAULT_ST, 0, 1)

REG32(EXTMEM_CORE0_ACS_CACHE_INT_ENA, 0x0E8)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_DBUS_WR_IC_INT_ENA, 5, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_DBUS_REJECT_INT_ENA, 4, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_DBUS_ACS_MSK_IC_INT_ENA, 3, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_IBUS_REJECT_INT_ENA, 2, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_IBUS_WR_IC_INT_ENA, 1, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ENA, CORE0_IBUS_ACS_MSK_IC_INT_ENA, 0, 1)

REG32(EXTMEM_CORE0_ACS_CACHE_INT_CLR, 0x0EC)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_DBUS_WR_IC_INT_CLR, 5, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_DBUS_REJECT_INT_CLR, 4, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_DBUS_ACS_MSK_IC_INT_CLR, 3, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_IBUS_REJECT_INT_CLR, 2, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_IBUS_WR_IC_INT_CLR, 1, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_CLR, CORE0_IBUS_ACS_MSK_IC_INT_CLR, 0, 1)

REG32(EXTMEM_CORE0_ACS_CACHE_INT_ST, 0x0F0)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_DBUS_WR_ICACHE_ST, 5, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_DBUS_REJECT_ST, 4, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_DBUS_ACS_MSK_ICACHE_ST, 3, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_IBUS_REJECT_ST, 2, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_IBUS_WR_ICACHE_ST, 1, 1)
    FIELD(EXTMEM_CORE0_ACS_CACHE_INT_ST, CORE0_IBUS_ACS_MSK_ICACHE_ST, 0, 1)

REG32(EXTMEM_CORE1_ACS_CACHE_INT_ENA, 0x0F4)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_DBUS_WR_ICACHE_ST, 5, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_DBUS_REJECT_ST, 4, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_DBUS_ACS_MSK_ICACHE_ST, 3, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_IBUS_REJECT_ST, 2, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_IBUS_WR_ICACHE_ST, 1, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ENA, CORE1_IBUS_ACS_MSK_ICACHE_ST, 0, 1)

REG32(EXTMEM_CORE1_ACS_CACHE_INT_CLR, 0x0F8)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_DBUS_WR_IC_INT_CLR, 5, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_DBUS_REJECT_INT_CLR, 4, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_DBUS_ACS_MSK_IC_INT_CLR, 3, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_IBUS_REJECT_INT_CLR, 2, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_IBUS_WR_IC_INT_CLR, 1, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_CLR, CORE1_IBUS_ACS_MSK_IC_INT_CLR, 0, 1)

REG32(EXTMEM_CORE1_ACS_CACHE_INT_ST, 0x0FC)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_DBUS_WR_ICACHE_ST, 5, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_DBUS_REJECT_ST, 4, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_DBUS_ACS_MSK_ICACHE_ST, 3, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_IBUS_REJECT_ST, 2, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_IBUS_WR_ICACHE_ST, 1, 1)
    FIELD(EXTMEM_CORE1_ACS_CACHE_INT_ST, CORE1_IBUS_ACS_MSK_ICACHE_ST, 0, 1)

REG32(EXTMEM_CORE0_DBUS_REJECT_ST, 0x100)
    FIELD(EXTMEM_CORE0_DBUS_REJECT_ST, CORE0_DBUS_WORLD, 3, 1)
    FIELD(EXTMEM_CORE0_DBUS_REJECT_ST, CORE0_DBUS_ATTR, 0, 3)

REG32(EXTMEM_CORE0_DBUS_REJECT_VADDR, 0x104)
    FIELD(EXTMEM_CORE0_DBUS_REJECT_VADDR, CORE0_DBUS_VADDR, 0, 32)

REG32(EXTMEM_CORE0_IBUS_REJECT_ST, 0x108)
    FIELD(EXTMEM_CORE0_IBUS_REJECT_ST, CORE0_IBUS_WORLD, 3, 1)
    FIELD(EXTMEM_CORE0_IBUS_REJECT_ST, CORE0_IBUS_ATTR, 0, 3)

REG32(EXTMEM_CORE0_IBUS_REJECT_VADDR, 0x10C)
    FIELD(EXTMEM_CORE0_IBUS_REJECT_VADDR, CORE0_IBUS_VADDR, 0, 32)

REG32(EXTMEM_CORE1_DBUS_REJECT_ST, 0x110)
    FIELD(EXTMEM_CORE1_DBUS_REJECT_ST, CORE1_DBUS_WORLD, 3, 1)
    FIELD(EXTMEM_CORE1_DBUS_REJECT_ST, CORE1_DBUS_ATTR, 0, 3)

REG32(EXTMEM_CORE1_DBUS_REJECT_VADDR, 0x114)
    FIELD(EXTMEM_CORE1_DBUS_REJECT_VADDR, CORE1_DBUS_VADDR, 0, 32)

REG32(EXTMEM_CORE1_IBUS_REJECT_ST, 0x118)
    FIELD(EXTMEM_CORE1_IBUS_REJECT_ST, CORE1_IBUS_WORLD, 3, 1)
    FIELD(EXTMEM_CORE1_IBUS_REJECT_ST, CORE1_IBUS_ATTR, 0, 3)

REG32(EXTMEM_CORE1_IBUS_REJECT_VADDR, 0x11C)
    FIELD(EXTMEM_CORE1_IBUS_REJECT_VADDR, CORE1_IBUS_VADDR, 0, 32)



REG32(EXTMEM_CACHE_MMU_FAULT_CONTENT, 0x120)
    FIELD(EXTMEM_CACHE_MMU_FAULT_CONTENT, CACHE_MMU_FAULT_CODE, 10, 4)
    FIELD(EXTMEM_CACHE_MMU_FAULT_CONTENT, CACHE_MMU_FAULT_CONTENT, 0, 10)

REG32(EXTMEM_CACHE_MMU_FAULT_VADDR, 0x124)
    FIELD(EXTMEM_CACHE_MMU_FAULT_VADDR, CACHE_MMU_FAULT_VADDR, 0, 32)

REG32(EXTMEM_CACHE_WRAP_AROUND_CTRL, 0x128)
    FIELD(EXTMEM_CACHE_WRAP_AROUND_CTRL, CACHE_FLASH_WRAP_AROUND, 0, 1)

REG32(EXTMEM_CACHE_MMU_POWER_CTRL, 0x12C)
    FIELD(EXTMEM_CACHE_MMU_POWER_CTRL, CACHE_MMU_MEM_FORCE_PU, 2, 1)
    FIELD(EXTMEM_CACHE_MMU_POWER_CTRL, CACHE_MMU_MEM_FORCE_PD, 1, 1)
    FIELD(EXTMEM_CACHE_MMU_POWER_CTRL, CACHE_MMU_MEM_FORCE_ON, 0, 1)

REG32(EXTMEM_CACHE_STATE, 0x130)
    FIELD(EXTMEM_CACHE_STATE, DCACHE_STATE, 0, 12)
    FIELD(EXTMEM_CACHE_STATE, ICACHE_STATE, 12, 12)

REG32(EXTMEM_CACHE_ENCRYPT_DECRYPT_RECORD_DISABLE, 0x134)
    FIELD(EXTMEM_CACHE_ENCRYPT_DECRYPT_RECORD_DISABLE, RECORD_DISABLE_G0CB_DECRYPT, 1, 1)
    FIELD(EXTMEM_CACHE_ENCRYPT_DECRYPT_RECORD_DISABLE, RECORD_DISABLE_DB_ENCRYPT, 0, 1)

REG32(EXTMEM_CACHE_ENCRYPT_DECRYPT_CLK_FORCE_ON, 0x138)
    FIELD(EXTMEM_CACHE_ENCRYPT_DECRYPT_CLK_FORCE_ON, CLK_FORCE_ON_CRYPT, 2, 1)
    FIELD(EXTMEM_CACHE_ENCRYPT_DECRYPT_CLK_FORCE_ON, CLK_FORCE_ON_AUTO_CRYPT, 1, 1)
    FIELD(EXTMEM_CACHE_ENCRYPT_DECRYPT_CLK_FORCE_ON, CLK_FORCE_ON_MANUAL_CRYPT, 0, 1)

REG32(EXTMEM_CACHE_BRIDGE_ARBITER_CTRL, 0x13C)
    FIELD(EXTMEM_CACHE_BRIDGE_ARBITER_CTRL, ALLOC_WB_HOLD_ARBITER, 0, 1)

REG32(EXTMEM_CACHE_PRELOAD_INT_CTRL, 0x140)
    FIELD(EXTMEM_CACHE_PRELOAD_INT_CTRL, ICACHE_PRELOAD_INT_CLR, 2, 1)
    FIELD(EXTMEM_CACHE_PRELOAD_INT_CTRL, ICACHE_PRELOAD_INT_ENA, 1, 1)
    FIELD(EXTMEM_CACHE_PRELOAD_INT_CTRL, ICACHE_PRELOAD_INT_ST, 0, 1)

REG32(EXTMEM_CACHE_SYNC_INT_CTRL, 0x144)
    FIELD(EXTMEM_CACHE_SYNC_INT_CTRL, ICACHE_SYNC_INT_CLR, 2, 1)
    FIELD(EXTMEM_CACHE_SYNC_INT_CTRL, ICACHE_SYNC_INT_ENA, 1, 1)
    FIELD(EXTMEM_CACHE_SYNC_INT_CTRL, ICACHE_SYNC_INT_ST, 0, 1)

REG32(EXTMEM_CACHE_MMU_OWNER, 0x148)
    FIELD(EXTMEM_CACHE_MMU_OWNER, CACHE_MMU_OWNER, 0, 4)

REG32(EXTMEM_CACHE_CONF_MISC, 0x14C)
    FIELD(EXTMEM_CACHE_CONF_MISC, CACHE_TRACE_ENA, 2, 1)
    FIELD(EXTMEM_CACHE_CONF_MISC, CACHE_IGNORE_SYNC_MMU_ENTRY_FAULT, 1, 1)
    FIELD(EXTMEM_CACHE_CONF_MISC, CACHE_IGNORE_PRELOAD_MMU_ENTRY_FAULT, 0, 1)

REG32(EXTMEM_DCACHE_FREEZE, 0x150)
    FIELD(EXTMEM_DCACHE_FREEZE, DCACHE_FREEZE_DONE, 2, 1)
    FIELD(EXTMEM_DCACHE_FREEZE, DCACHE_FREEZE_MODE, 1, 1)
    FIELD(EXTMEM_DCACHE_FREEZE, DCACHE_FREEZE_ENA, 0, 1)

REG32(EXTMEM_ICACHE_FREEZE, 0x154)
    FIELD(EXTMEM_ICACHE_FREEZE, ICACHE_FREEZE_DONE, 2, 1)
    FIELD(EXTMEM_ICACHE_FREEZE, ICACHE_FREEZE_MODE, 1, 1)
    FIELD(EXTMEM_ICACHE_FREEZE, ICACHE_FREEZE_ENA, 0, 1)



REG32(EXTMEM_ICACHE_ATOMIC_OPERATE_ENA, 0x158)
    FIELD(EXTMEM_ICACHE_ATOMIC_OPERATE_ENA, ICACHE_ATOMIC_OPERATE_ENA, 0, 1)

REG32(EXTMEM_DCACHE_ATOMIC_OPERATE_ENA, 0x15C)
    FIELD(EXTMEM_DCACHE_ATOMIC_OPERATE_ENA, DCACHE_ATOMIC_OPERATE_ENA, 0, 1)

REG32(EXTMEM_CACHE_REQUEST, 0x160)
    FIELD(EXTMEM_CACHE_REQUEST, CACHE_REQUEST_BYPASS, 0, 1)



REG32(EXTMEM_IBUS_PMS_TBL_LOCK, 0x0D8 + 0x060)
    FIELD(EXTMEM_IBUS_PMS_TBL_LOCK, IBUS_PMS_LOCK, 0, 1)

REG32(EXTMEM_IBUS_PMS_TBL_BOUNDARY0, 0x0DC + 0x060)
    FIELD(EXTMEM_IBUS_PMS_TBL_BOUNDARY0, IBUS_PMS_BOUNDARY0, 0, 12)

REG32(EXTMEM_IBUS_PMS_TBL_BOUNDARY1, 0x0E0 + 0x060)
    FIELD(EXTMEM_IBUS_PMS_TBL_BOUNDARY1, IBUS_PMS_BOUNDARY1, 0, 12)

REG32(EXTMEM_IBUS_PMS_TBL_BOUNDARY2, 0x0E4 + 0x060)
    FIELD(EXTMEM_IBUS_PMS_TBL_BOUNDARY2, IBUS_PMS_BOUNDARY2, 0, 12)

REG32(EXTMEM_IBUS_PMS_TBL_ATTR, 0x0E8 + 0x060)
    FIELD(EXTMEM_IBUS_PMS_TBL_ATTR, IBUS_PMS_SCT2_ATTR, 4, 4)
    FIELD(EXTMEM_IBUS_PMS_TBL_ATTR, IBUS_PMS_SCT1_ATTR, 0, 4)

REG32(EXTMEM_DBUS_PMS_TBL_LOCK, 0x0EC + 0x060)
    FIELD(EXTMEM_DBUS_PMS_TBL_LOCK, DBUS_PMS_LOCK, 0, 1)

REG32(EXTMEM_DBUS_PMS_TBL_BOUNDARY0, 0x0F0 + 0x060)
    FIELD(EXTMEM_DBUS_PMS_TBL_BOUNDARY0, DBUS_PMS_BOUNDARY0, 0, 12)

REG32(EXTMEM_DBUS_PMS_TBL_BOUNDARY1, 0x0F4 + 0x060)
    FIELD(EXTMEM_DBUS_PMS_TBL_BOUNDARY1, DBUS_PMS_BOUNDARY1, 0, 12)

REG32(EXTMEM_DBUS_PMS_TBL_BOUNDARY2, 0x0F8 + 0x060)
    FIELD(EXTMEM_DBUS_PMS_TBL_BOUNDARY2, DBUS_PMS_BOUNDARY2, 0, 12)

REG32(EXTMEM_DBUS_PMS_TBL_ATTR, 0x0FC + 0x060)
    FIELD(EXTMEM_DBUS_PMS_TBL_ATTR, DBUS_PMS_SCT2_ATTR, 2, 2)
    FIELD(EXTMEM_DBUS_PMS_TBL_ATTR, DBUS_PMS_SCT1_ATTR, 0, 2)

REG32(EXTMEM_CLOCK_GATE, 0x164)
    FIELD(EXTMEM_CLOCK_GATE, CLK_EN, 0, 1)

REG32(EXTMEM_DATE, 0x3FC)
    FIELD(EXTMEM_DATE, DATE, 0, 28)

