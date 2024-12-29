/*
 * lld_nvme.c
 *
 *  Created on: 2021. 8. 15.
 *      Author: Park
 */


#include "xil_printf.h"
#include "../../host/nvme/io_access.h"
#include "lld_nvme.h"
#include "../../host/nvme/nvme.h"
#include "../../host/nvme/nvme_main.h"

void lld_pcie_async_reset(unsigned int rstCnt)
{
	NVME_STATUS_REG nvmeReg;

	nvmeReg.rstCnt = rstCnt;
	xil_printf("rstCnt= %X \r\n",rstCnt);
	IO_WRITE32(NVME_STATUS_REG_ADDR, nvmeReg.dword);

}

void lld_pcie_set_link_width(unsigned int linkNum)
{
	NVME_STATUS_REG nvmeReg;

	nvmeReg.linkNum = linkNum;
	nvmeReg.linkEn = 1;
	xil_printf("linkNum= %X \r\n",linkNum);
	IO_WRITE32(NVME_STATUS_REG_ADDR, nvmeReg.dword);

}


unsigned int lld_nvme_get_cc_en()
{
	NVME_STATUS_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);

	return (unsigned int)nvmeReg.ccEn;
}



void lld_nvme_set_csts_rdy(unsigned int rdy)
{
	NVME_STATUS_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);
	nvmeReg.cstsRdy = rdy;

	IO_WRITE32(NVME_STATUS_REG_ADDR, nvmeReg.dword);
}

void lld_nvme_set_csts_shst(unsigned int shst)
{
	NVME_STATUS_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);
	nvmeReg.cstsShst = shst;

	IO_WRITE32(NVME_STATUS_REG_ADDR, nvmeReg.dword);
}

void lld_nvme_set_admin_queue(unsigned int sqValid, unsigned int cqValid, unsigned int cqIrqEn)
{
	NVME_ADMIN_QUEUE_SET_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_ADMIN_QUEUE_SET_REG_ADDR);
	nvmeReg.sqValid = sqValid;
	nvmeReg.cqValid = cqValid;
	nvmeReg.cqIrqEn = cqIrqEn;

	IO_WRITE32(NVME_ADMIN_QUEUE_SET_REG_ADDR, nvmeReg.dword);
}

#if 0
unsigned int lld_nvme_get_cmd(NVME_COMMAND_ENTRY* p_command_list)
{
	NVME_CMD_FIFO_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_CMD_FIFO_REG_ADDR);

	if(nvmeReg.cmdValid == 1)
	{
		unsigned int addr;
		unsigned int idx;
		*qID = nvmeReg.qID;

		p_command_list[nvmeReg.cmdSlotTag].qID = nvmeReg.qID;
		p_command_list[nvmeReg.cmdSlotTag].cmdSlotTag = nvmeReg.cmdSlotTag;
		p_command_list[nvmeReg.cmdSlotTag].cmdSeqNum = nvmeReg.cmdSeqNum;
		//xil_printf("nvmeReg.cmdSlotTag = 0x%X\r\n", nvmeReg.cmdSlotTag);
		addr = NVME_CMD_SRAM_ADDR + (nvmeReg.cmdSlotTag * 64);

		for(idx = 0; idx < 16; idx++)
		{
			p_command_list[nvmeReg.cmdSlotTag].cmdDword[idx] = IO_READ32(addr + (idx * 4));
		}
	}

	return (unsigned int)nvmeReg.cmdValid;
}
#endif
#if 1
unsigned int lld_nvme_get_cmd(unsigned short *qID, unsigned short *cmdSlotTag, unsigned int *cmdSeqNum, unsigned int *cmdDword)
{
	NVME_CMD_FIFO_REG nvmeReg;

	nvmeReg.dword = IO_READ32(NVME_CMD_FIFO_REG_ADDR);

	if(nvmeReg.cmdValid == 1)
	{
		unsigned int addr;
		unsigned int idx;
		*qID = nvmeReg.qID;
		*cmdSlotTag = nvmeReg.cmdSlotTag;
		*cmdSeqNum = nvmeReg.cmdSeqNum;
		//xil_printf("nvmeReg.cmdSlotTag = 0x%X\r\n", nvmeReg.cmdSlotTag);
		addr = NVME_CMD_SRAM_ADDR + (nvmeReg.cmdSlotTag * 64);
		for(idx = 0; idx < 16; idx++)
		{
			*(cmdDword + idx) = IO_READ32(addr + (idx * 4));
		}
	}

	return (unsigned int)nvmeReg.cmdValid;
}
#endif
void lld_nvme_set_auto_cpl(unsigned int cmdSlotTag, unsigned int specific, unsigned int statusFieldWord)
{
	NVME_CPL_FIFO_REG nvmeReg;

	nvmeReg.specific = specific;
	nvmeReg.cmdSlotTag = cmdSlotTag;
	nvmeReg.statusFieldWord = statusFieldWord;
	nvmeReg.cplType = AUTO_CPL_TYPE;

	//IO_WRITE32(NVME_CPL_FIFO_REG_ADDR, nvmeReg.dword[0]);
	IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 4), nvmeReg.dword[1]);
	IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 8), nvmeReg.dword[2]);
}

void lld_nvme_set_slot_release(unsigned int cmdSlotTag)
{
	NVME_CPL_FIFO_REG nvmeReg;

	nvmeReg.cmdSlotTag = cmdSlotTag;
	nvmeReg.cplType = CMD_SLOT_RELEASE_TYPE;

	//IO_WRITE32(NVME_CPL_FIFO_REG_ADDR, nvmeReg.dword[0]);
	//IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 4), nvmeReg.dword[1]);
	IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 8), nvmeReg.dword[2]);
}

void lld_nvme_set_manual_cpl(unsigned int sqId, unsigned int cid, unsigned int specific, unsigned int statusFieldWord)
{
	NVME_CPL_FIFO_REG nvmeReg;

	nvmeReg.cid = cid;
	nvmeReg.sqId = sqId;
	nvmeReg.specific = specific;
	nvmeReg.statusFieldWord = statusFieldWord;
	nvmeReg.cplType = ONLY_CPL_TYPE;

	IO_WRITE32(NVME_CPL_FIFO_REG_ADDR, nvmeReg.dword[0]);
	IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 4), nvmeReg.dword[1]);
	IO_WRITE32((NVME_CPL_FIFO_REG_ADDR + 8), nvmeReg.dword[2]);
}

void lld_nvme_set_io_sq(unsigned int ioSqIdx, unsigned int valid, unsigned int cqVector, unsigned int qSzie, unsigned int pcieBaseAddrL, unsigned int pcieBaseAddrH)
{
	NVME_IO_SQ_SET_REG nvmeReg;
	unsigned int addr;

	nvmeReg.valid = valid;
	nvmeReg.cqVector = cqVector;
	nvmeReg.sqSize = qSzie;
	nvmeReg.pcieBaseAddrL = pcieBaseAddrL;
	nvmeReg.pcieBaseAddrH = pcieBaseAddrH;

	addr = NVME_IO_SQ_SET_REG_ADDR + (ioSqIdx * 8);
	IO_WRITE32(addr, nvmeReg.dword[0]);
	IO_WRITE32((addr + 4), nvmeReg.dword[1]);
}

void lld_nvme_set_io_cq(unsigned int ioCqIdx, unsigned int valid, unsigned int irqEn, unsigned int irqVector, unsigned int qSzie, unsigned int pcieBaseAddrL, unsigned int pcieBaseAddrH)
{
	NVME_IO_CQ_SET_REG nvmeReg;
	unsigned int addr;

	nvmeReg.valid = valid;
	nvmeReg.irqEn = irqEn;
	nvmeReg.irqVector = irqVector;
	nvmeReg.cqSize = qSzie;
	nvmeReg.pcieBaseAddrL = pcieBaseAddrL;
	nvmeReg.pcieBaseAddrH = pcieBaseAddrH;

	addr = NVME_IO_CQ_SET_REG_ADDR + (ioCqIdx * 8);
	IO_WRITE32(addr, nvmeReg.dword[0]);
	IO_WRITE32((addr + 4), nvmeReg.dword[1]);

}
