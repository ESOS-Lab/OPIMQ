//////////////////////////////////////////////////////////////////////////////////
// ftl_config.c for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Flash Translation Layer Configuration Manager
// File Name: ftl_config.c
//
// Version: v1.0.0
//
// Description:
//   - initialize flash translation layer
//	 - check configuration options
//	 - initialize NAND device
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include <assert.h>
#include "xtime_l.h"
#include "xil_printf.h"
#include "../memory_map.h"

#if (SUPPORT_BARRIER_FTL == 1)
#include "barrier_ftl.h"
#endif

unsigned int storageCapacity_L;

ftl_context g_ftl_context;

static void _init_ftl_context()
{
	memset((void*)&g_ftl_context, 0x00, sizeof(ftl_context));

	XTime_GetTime(&g_ftl_context.start_tick_internal_flush);
}

static void _check_ftl_config_restriction()
{
	static_assert((USER_CHANNELS <= NSC_MAX_CHANNELS), "[WARNING] Configuration Error: Channel [WARNING]");
	static_assert((USER_WAYS <= NSC_MAX_WAYS), "[WARNING] Configuration Error: Way [WARNING]");
	static_assert((USER_BLOCKS_PER_LUN <= MAIN_BLOCKS_PER_LUN), "[WARNING] Configuration Error: Block [WARNING]");
	static_assert((BITS_PER_FLASH_CELL == SLC_MODE), "[WARNING] Configuration Error: BIT_PER_FLASH_CELL [WARNING]");

	static_assert((RESERVED_DATA_BUFFER_BASE_ADDR + 0x00200000 <= COMPLETE_FLAG_TABLE_ADDR),
			"[WARNING] Configuration Error: Data buffer size is too large to be allocated to predefined range [WARNING]");
	static_assert((TEMPORARY_PAY_LOAD_ADDR + 0x00001000 <= DATA_BUFFER_MAP_ADDR),
			"[WARNING] Configuration Error: Metadata for NAND request completion process is too large to be allocated to predefined range [WARNING]");
	static_assert((FTL_MANAGEMENT_END_ADDR <= DRAM_END_ADDR),
			"[WARNING] Configuration Error: Metadata of FTL is too large to be allocated to DRAM [WARNING]");
}

static void _process_internal_flush()
{
	XTime_GetTime(&g_ftl_context.end_tick_internal_flush);

	unsigned int elapsed_time_ms = GET_TIME_MS(g_ftl_context.start_tick_internal_flush, g_ftl_context.end_tick_internal_flush);

	if (INTERNAL_FLUSH_PERIOD_MS <= elapsed_time_ms)
	{
		//xil_printf("internal flush (%llu ms)\r\n", elapsed_time_ms);
		/*
#if (SUPPORT_BARRIER_FTL == 1)
		barrier_flush_operation();
#endif*/

		//FlushWriteDataToNand();
		//barrier_search_suspension_list_dual_stream();

		XTime_GetTime(&g_ftl_context.start_tick_internal_flush);

		elapsed_time_ms = GET_TIME_MS(g_ftl_context.end_tick_internal_flush, g_ftl_context.end_tick_internal_flush);

		if (elapsed_time_ms > 1)
		{
			xil_printf("internal flush done (%llu)\r\n", elapsed_time_ms);
		}
	}
}

void ftl_init()
{
	_check_ftl_config_restriction();
	xil_printf("_check_ftl_config_restriction DONE\n");
	_init_ftl_context();
	xil_printf("_init_ftl_context DONE\n");

	InitAddressMap();
	xil_printf("InitAddressMap DONE\n");

	GarbageCollection_Init();
	xil_printf("GarbageCollection_Init DONE\n");

#if (SUPPORT_BARRIER_FTL == 1)
	barrier_init();
#endif

	storageCapacity_L = (MB_PER_SSD - (MB_PER_MIN_FREE_BLOCK_SPACE + mbPerbadBlockSpace + MB_PER_OVER_PROVISION_BLOCK_SPACE)) * ((1024*1024) / BYTES_PER_NVME_BLOCK);

	xil_printf("[ storage capacity %d MB ]\r\n", storageCapacity_L / ((1024*1024) / BYTES_PER_NVME_BLOCK));
	xil_printf("[ ftl configuration complete. ]\r\n");
}

void ftl_task_run(void)
{
	// SP: doing ftl jobs as below here

	// host data internal flush
	_process_internal_flush();

	// garbage collection
}

