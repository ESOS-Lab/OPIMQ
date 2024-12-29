//////////////////////////////////////////////////////////////////////////////////
// nvme_main.c for Cosmos+ OpenSSD
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
// Module Name: NVMe Main
// File Name: nvme_main.c
//
// Version: v1.2.0
//
// Description:
//   - initializes FTL and NAND
//   - handles NVMe controller
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.2.0
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//   - Low level scheduler execution is allowed when there is no i/o command
//
// * v1.1.0
//   - DMA status initialization is added
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "xil_printf.h"
#include "../../debug.h"
#include "io_access.h"
#include "xtime_l.h"

#include "../../hal/lld/host_lld.h"
#include "../../hal/hal_host.h"

#include "nvme.h"
#include "nvme_main.h"
#include "nvme_admin_cmd.h"
#include "nvme_io_cmd.h"

#include "../../ftl/barrier_ftl.h"

#include "../../memory_map.h"

volatile NVME_TASK_CONTEXT g_nvmeTask;
NVME_COMMAND_CONTEXT_T g_nvme_command_context;

void dev_irq_handler()
{
	DEV_IRQ_REG devReg;
//	Xil_ExceptionDisable();

	devReg.dword = IO_READ32(DEV_IRQ_STATUS_REG_ADDR);
	IO_WRITE32(DEV_IRQ_CLEAR_REG_ADDR, devReg.dword);
//	xil_printf("IRQ: 0x%X\r\n", devReg.dword);

	if(devReg.pcieLink == 1)
	{
		PCIE_STATUS_REG pcieReg;
		pcieReg.dword = IO_READ32(PCIE_STATUS_REG_ADDR);
		xil_printf("PCIe Link: %d\r\n", pcieReg.pcieLinkUp);
		// set_link_width(0) //you can choose pcie lane width 0,2,4,8 mini board -> maximum 4, cosmos+ board -> maximum 8
		if(pcieReg.pcieLinkUp == 0)
			g_nvmeTask.status = NVME_TASK_RESET;
	}

	if(devReg.busMaster == 1)
	{
		PCIE_FUNC_REG pcieReg;
		pcieReg.dword = IO_READ32(PCIE_FUNC_REG_ADDR);
		xil_printf("PCIe Bus Master: %d\r\n", pcieReg.busMaster);
	}

	if(devReg.pcieIrq == 1)
	{
		PCIE_FUNC_REG pcieReg;
		pcieReg.dword = IO_READ32(PCIE_FUNC_REG_ADDR);
		xil_printf("PCIe IRQ Disable: %d\r\n", pcieReg.irqDisable);
	}

	if(devReg.pcieMsi == 1)
	{
		PCIE_FUNC_REG pcieReg;
		pcieReg.dword = IO_READ32(PCIE_FUNC_REG_ADDR);
		xil_printf("PCIe MSI Enable: %d, 0x%x\r\n", pcieReg.msiEnable, pcieReg.msiVecNum);
	}

	if(devReg.pcieMsix == 1)
	{
		PCIE_FUNC_REG pcieReg;
		pcieReg.dword = IO_READ32(PCIE_FUNC_REG_ADDR);
		xil_printf("PCIe MSI-X Enable: %d\r\n", pcieReg.msixEnable);
	}

	if(devReg.nvmeCcEn == 1)
	{
		NVME_STATUS_REG nvmeReg;
		nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);
		xil_printf("NVME CC.EN: %d\r\n", nvmeReg.ccEn);

		if(nvmeReg.ccEn == 1)
		{
			g_nvmeTask.status = NVME_TASK_WAIT_CC_EN;
		}
		else
		{
			g_nvmeTask.status = NVME_TASK_RESET;
		}

	}

	if(devReg.nvmeCcShn == 1)
	{
		NVME_STATUS_REG nvmeReg;
		nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);
		xil_printf("NVME CC.SHN: %d\r\n", nvmeReg.ccShn);
		if(nvmeReg.ccShn == 1)
			g_nvmeTask.status = NVME_TASK_SHUTDOWN;
	}

	if(devReg.mAxiWriteErr == 1)
	{
		xil_printf("mAxiWriteErr\r\n");
	}

	if(devReg.pcieMreqErr == 1)
	{
		xil_printf("pcieMreqErr\r\n");
	}

	if(devReg.pcieCpldErr == 1)
	{
		xil_printf("pcieCpldErr\r\n");
	}

	if(devReg.pcieCpldLenErr == 1)
	{
		xil_printf("pcieCpldLenErr\r\n");
	}
//	Xil_ExceptionEnable();
}

NVME_COMMAND_ENTRY* nvme_command_context_get_command_entry(unsigned int cmdSlotTag)
{
	return &g_nvme_command_context.command_list[cmdSlotTag];
}

void nvme_command_context_increase_read_outstanding_count()
{
	g_nvme_command_context.outstanding_read_command_count++;
}

void nvme_command_context_increase_write_outstanding_count()
{
	g_nvme_command_context.outstanding_write_command_count++;
}

void nvme_command_context_decrease_read_outstanding_count()
{
	g_nvme_command_context.outstanding_read_command_count--;
}

void nvme_command_context_decrease_write_outstanding_count()
{
	g_nvme_command_context.outstanding_write_command_count--;
}

void nvme_command_context_increase_complete_command_count()
{
	g_nvme_command_context.complete_io_command_count++;
}

static void _nvme_task_run()
{
	unsigned int cmdSlotTag = hal_host_fetch_nvme_command(g_nvme_command_context.command_list);

	if (INVALID_CMD_SLOT_TAG != cmdSlotTag)
	{
		NVME_COMMAND_ENTRY* p_cmdEntry = &g_nvme_command_context.command_list[cmdSlotTag];

		if (BRANCH_LIKELY((0 != p_cmdEntry->qID)))
		{
			g_nvme_command_context.fetch_io_command_count++;

			NVMCommand_process(p_cmdEntry);
		}
		else
		{
			AdminCommand_process(p_cmdEntry);
		}
	}
}

void host_task_init(void)
{
	memset((void*)&g_nvmeTask, 0x00, sizeof(NVME_TASK_CONTEXT));
	memset((void*)&g_nvme_command_context, 0x00, sizeof(NVME_COMMAND_CONTEXT_T));
}

void host_task_run(void)
{
	static unsigned int rstCnt = 0;

	if (BRANCH_LIKELY((NVME_TASK_RUNNING == g_nvmeTask.status)))
	{
		_nvme_task_run();

		if (0 != RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NVME_DMA, 0, 0))
		{
			hal_host_handle_hdma_result();
		}
	}
	else
	{
		if (NVME_TASK_WAIT_CC_EN == g_nvmeTask.status)
		{
			unsigned int b_ccEnSet = hal_host_set_nvme_ccen();

			if(1 == b_ccEnSet)
			{
				g_nvmeTask.status = NVME_TASK_RUNNING;

				g_nvme_command_context.fetch_io_command_count = 0;
				g_nvme_command_context.outstanding_read_command_count = 0;
				g_nvme_command_context.outstanding_write_command_count = 0;

				g_nvme_command_context.complete_io_command_count = 0;

				xil_printf("\r\nNVMe ready!!!\r\n");
			}
		}
		else if (NVME_TASK_SHUTDOWN == g_nvmeTask.status)
		{
			FlushWriteDataToNand();

			unsigned int b_shutdown = hal_host_nvme_shutdown();

			if(0 != b_shutdown)
			{
				g_nvmeTask.cacheEn = 0;
				g_nvmeTask.status = NVME_TASK_WAIT_RESET;

				//flush grown bad block info
				UpdateBadBlockTableForGrownBadBlock(RESERVED_DATA_BUFFER_BASE_ADDR);

				xil_printf("\r\nNVMe shutdown done.\r\n");
			}
		}
		else if(NVME_TASK_WAIT_RESET == g_nvmeTask.status)
		{
			unsigned int b_ccEnSet = hal_host_clear_nvme_ccen();

			if (0 == b_ccEnSet)
			{
				g_nvmeTask.cacheEn = 0;
				g_nvmeTask.status = NVME_TASK_IDLE;

				xil_printf("\r\nNVMe disable!!!\r\n");
			}
		}
		else if(NVME_TASK_RESET == g_nvmeTask.status)
		{
			unsigned int qID;
			for(qID = 0; qID < 8; qID++)
			{
				lld_nvme_set_io_cq(qID, 0, 0, 0, 0, 0, 0);
				lld_nvme_set_io_sq(qID, 0, 0, 0, 0, 0);
			}

			if (rstCnt>= 5){
				lld_pcie_async_reset(rstCnt);
				rstCnt = 0;
				xil_printf("\r\nPcie iink disable!!!\r\n");
				xil_printf("Wait few minute or reconnect the PCIe cable\r\n");
			}
			else
				rstCnt++;

			g_nvmeTask.cacheEn = 0;
			lld_nvme_set_admin_queue(0, 0, 0);
			lld_nvme_set_csts_shst(0);
			lld_nvme_set_csts_rdy(0);
			g_nvmeTask.status = NVME_TASK_IDLE;

			xil_printf("\r\nNVMe reset!!!\r\n");
		}
	}
}

