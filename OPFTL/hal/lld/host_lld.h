//////////////////////////////////////////////////////////////////////////////////
// host_lld.h for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
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
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe Low Level Driver
// File Name: host_lld.h
//
// Version: v1.1.0
//
// Description:
//   - defines parameters and data structures of the NVMe low level driver
//   - declares functions of the NVMe low level driver
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.1.0
//   - new DMA status type is added (HOST_DMA_ASSIST_STATUS)
//	 - DMA partial done check functions are added
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#ifndef __HOST_LLD_H_
#define __HOST_LLD_H_

#define HOST_IP_ADDR						(XPAR_NVME_CTRL_0_BASEADDR)

#include "lld_pcie.h"
#include "lld_hdma.h"
#include "lld_nvme.h"

#define DEV_IRQ_MASK_REG_ADDR				(HOST_IP_ADDR + 0x4)
#define DEV_IRQ_CLEAR_REG_ADDR				(HOST_IP_ADDR + 0x8)
#define DEV_IRQ_STATUS_REG_ADDR				(HOST_IP_ADDR + 0xC)

#define PCIE_STATUS_REG_ADDR				(HOST_IP_ADDR + 0x100)
#define PCIE_FUNC_REG_ADDR					(HOST_IP_ADDR + 0x104)

#if 0
#define NVME_STATUS_REG_ADDR				(HOST_IP_ADDR + 0x200)
#define HOST_DMA_FIFO_CNT_REG_ADDR			(HOST_IP_ADDR + 0x204)
#define NVME_ADMIN_QUEUE_SET_REG_ADDR		(HOST_IP_ADDR + 0x21C)
#define NVME_IO_SQ_SET_REG_ADDR				(HOST_IP_ADDR + 0x220)
#define NVME_IO_CQ_SET_REG_ADDR				(HOST_IP_ADDR + 0x260)

#define NVME_CMD_FIFO_REG_ADDR				(HOST_IP_ADDR + 0x300)
#define NVME_CPL_FIFO_REG_ADDR				(HOST_IP_ADDR + 0x304)
#define HOST_DMA_CMD_FIFO_REG_ADDR			(HOST_IP_ADDR + 0x310)

#define NVME_CMD_SRAM_ADDR					(HOST_IP_ADDR + 0x10000)




#define HOST_DMA_DIRECT_TYPE				(1)
#define HOST_DMA_AUTO_TYPE					(0)

#define HOST_DMA_TX_DIRECTION				(1)
#define HOST_DMA_RX_DIRECTION				(0)

#define ONLY_CPL_TYPE						(0)
#define AUTO_CPL_TYPE						(1)
#define CMD_SLOT_RELEASE_TYPE				(2)
#define P_SLOT_TAG_WIDTH					(10) //slot_modified
#endif

#pragma pack(push, 1)

typedef struct _DEV_IRQ_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int pcieLink			:1;
			unsigned int busMaster			:1;
			unsigned int pcieIrq			:1;
			unsigned int pcieMsi			:1;
			unsigned int pcieMsix			:1;
			unsigned int nvmeCcEn			:1;
			unsigned int nvmeCcShn			:1;
			unsigned int mAxiWriteErr		:1;
			unsigned int mAxiReadErr		:1;
			unsigned int pcieMreqErr		:1;
			unsigned int pcieCpldErr		:1;
			unsigned int pcieCpldLenErr		:1;
			unsigned int reserved0			:20;
		};
	};
} DEV_IRQ_REG;

typedef struct _PCIE_STATUS_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int ltssm				:6;
			unsigned int reserved0			:2;
			unsigned int pcieLinkUp			:1;
			unsigned int reserved1			:23;
		};
	};
} PCIE_STATUS_REG;

typedef struct _PCIE_FUNC_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int busMaster			:1;
			unsigned int msiEnable			:1;
			unsigned int msixEnable			:1;
			unsigned int irqDisable			:1;
			unsigned int msiVecNum			:3;
			unsigned int reserved0			:25;
		};
	};
} PCIE_FUNC_REG;


#pragma pack(pop)



#endif	//__HOST_LLD_H_
