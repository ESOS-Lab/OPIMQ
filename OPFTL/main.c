//////////////////////////////////////////////////////////////////////////////////
// main.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//				  Kibin Park <kbpark@enc.hanyang.ac.kr>
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
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//			 Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//			 Kibin Park <kbpark@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Main
// File Name: main.c
//
// Version: v1.0.2
//
// Description:
//   - initializes caches, MMU, exception handler
//   - calls nvme_main function
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.2
//   - An address region (0x0020_0000 ~ 0x179F_FFFF) is used to uncached & nonbuffered region
//   - An address region (0x1800_0000 ~ 0x3FFF_FFFF) is used to cached & buffered region
//
// * v1.0.1
//   - Paging table setting is modified for QSPI or SD card boot mode
//     * An address region (0x0010_0000 ~ 0x001F_FFFF) is used to place code, data, heap and stack sections
//     * An address region (0x0010_0000 ~ 0x001F_FFFF) is setted a cached&bufferd region
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////
#include "xil_cache.h"
#include "xil_exception.h"
#include "xil_mmu.h"
#include "xparameters_ps.h"
#include "xscugic_hw.h"
#include "xscugic.h"
#include "xil_printf.h"
#include "debug.h"
#include "xtime_l.h"

#include "host/nvme/io_access.h"
#include "hal/lld/host_lld.h"

#include "flash/flash_control.h"
#include "ftl/ftl_config.h"

#include "request_allocation.h"
#include "request_schedule.h"
#include "request_transform.h"
#include "data_buffer.h"
#include "host/nvme/nvme.h"
#include "host/nvme/nvme_main.h"

//Juwon Added
#include "xtime_l.h"
#include "xparameters.h"

XScuGic GicInstance;

static void _dev_irq_init()
{
	DEV_IRQ_REG devReg;

	devReg.dword = 0;
	devReg.pcieLink = 1;
	devReg.busMaster = 1;
	devReg.pcieIrq = 1;
	devReg.pcieMsi = 1;
	devReg.pcieMsix = 1;
	devReg.nvmeCcEn = 1;
	devReg.nvmeCcShn = 1;
	devReg.mAxiWriteErr = 1;
	devReg.pcieMreqErr = 1;
	devReg.pcieCpldErr = 1;
	devReg.pcieCpldLenErr = 1;

	IO_WRITE32(DEV_IRQ_MASK_REG_ADDR, devReg.dword);
}


static void _init_core(void)
{
	unsigned int u;

	XScuGic_Config *IntcConfig;

	Xil_ICacheDisable();
	Xil_DCacheDisable();
	Xil_DisableMMU();

	// Paging table set
	#define MB (1024*1024)
	for (u = 0; u < 4096; u++)
	{
		if (u < 0x2)
			Xil_SetTlbAttributes(u * MB, 0xC1E); // cached & buffered
		else if (u < 0x180)
			Xil_SetTlbAttributes(u * MB, 0xC12); // uncached & nonbuffered
		else if (u < 0x400)
			Xil_SetTlbAttributes(u * MB, 0xC1E); // cached & buffered
		else
			Xil_SetTlbAttributes(u * MB, 0xC12); // uncached & nonbuffered
	}

	Xil_EnableMMU();
	Xil_ICacheEnable();
	Xil_DCacheEnable();
	xil_printf("[!] MMU has been enabled.\r\n");


	xil_printf("\r\n Hello COSMOS+ OpenSSD !!! \r\n");


	Xil_ExceptionInit();

	IntcConfig = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
	XScuGic_CfgInitialize(&GicInstance, IntcConfig, IntcConfig->CpuBaseAddress);
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
								(Xil_ExceptionHandler)XScuGic_InterruptHandler,
								&GicInstance);

	XScuGic_Connect(&GicInstance, 61,
					(Xil_ExceptionHandler)dev_irq_handler,
					(void *)0);

	XScuGic_Enable(&GicInstance, 61);

	// Enable interrupts in the Processor.
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
	Xil_ExceptionEnable();

	_dev_irq_init();
}

int main()
{
	_init_core();

	xil_printf("!!! Wait until FTL init complete !!! \r\n");

	host_task_init();

	xil_printf("!!! host task done !!! \r\n");
	RequestAllocation_InitReqPool();
	xil_printf("!!! reqalloc done !!! \r\n");
	InitDependencyTable();
	xil_printf("!!! initdep done !!! \r\n");
	RequestScheduler_Init();

	xil_printf("!!! reqsched done !!! \r\n");
	BufferManagement_Init();

	flash_init();

	xil_printf("!!! flash init done !!! \r\n");
	ftl_init();

	xil_printf("\r\nFTL init complete!!! \r\n");
	xil_printf("Turn on the host PC \r\n");

	//XTime_StartTimer();
	while (1)
	{
		host_task_run();

		ftl_task_run();

		flash_task_run();
	}

	xil_printf("done\r\n");

	return 0;
}
