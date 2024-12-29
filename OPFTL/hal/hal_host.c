/*
 * hal_host.c

 *
 *  Created on: 2021. 8. 17.
 *      Author: Park
 */
#include "xil_printf.h"
#include <string.h>

#include "../debug.h"
#include "lld/lld_nvme.h"
#include "lld/lld_hdma.h"

#include "hal_host.h"

#include "../host/nvme/io_access.h"
#include "../host/nvme/nvme.h"
#include "../host/nvme/nvme_main.h"

#include "../ftl/barrier_ftl.h"

#include "../request_allocation.h"
#include "../request_schedule.h"
#include "../data_buffer.h"


void hal_host_issue_hdma_req(unsigned int reqSlotTag)
{
	unsigned int devAddr, dmaIndex, numOfNvmeBlock, fua;

	SSD_REQ_FORMAT* p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	dmaIndex = p_reqEntry->nvmeDmaInfo.startIndex;
	devAddr = BufferManagement_GetDataBufAddr(reqSlotTag);
	numOfNvmeBlock = 0;

	if(p_reqEntry->reqCode == REQ_CODE_RxDMA)
	{
		unsigned int b_reqAutoComplete = NVME_COMMAND_AUTO_COMPLETION_ON;
		fua = p_reqEntry->nvmeDmaInfo.fua;

		if (BRANCH_UNLIKELY(TRUE == fua))
		{
			b_reqAutoComplete = NVME_COMMAND_AUTO_COMPLETION_OFF;
		}

		while(numOfNvmeBlock < p_reqEntry->nvmeDmaInfo.numOfNvmeBlock)
		{
			//xil_printf("[HDMA req] write cmdtag:%u, reqSlotTag:%u, startdmaIdx:0x%X, dmaoffset:0x%X, buf_addr:0x%X, fua:0x%X\r\n",
			//		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag, reqSlotTag, dmaIndex, numOfNvmeBlock, devAddr, fua);

			lld_hdma_set_auto_rx_dma(p_reqEntry->nvmeCmdSlotTag, dmaIndex, devAddr, b_reqAutoComplete);

			numOfNvmeBlock++;
			dmaIndex++;
			devAddr += BYTES_PER_NVME_BLOCK;
		}

		HOST_DMA_FIFO_CNT_REG dma_tail_fifo;
		dma_tail_fifo.dword = lld_hdma_get_dma_tail_fifo_cnt();

		p_reqEntry->nvmeDmaInfo.reqTail = dma_tail_fifo.autoDmaRx;
		p_reqEntry->nvmeDmaInfo.overFlowCnt = lld_hdma_get_auto_dma_rx_overflow_cnt();
	}
	else if(p_reqEntry->reqCode == REQ_CODE_TxDMA)
	{
		while(numOfNvmeBlock < p_reqEntry->nvmeDmaInfo.numOfNvmeBlock)
		{
			//xil_printf("[HDMA req] read cmdtag:%u, reqSlotTag:%u, startdmaIdx:0x%X, dmaoffset:0x%X, buf_addr:0x%X\r\n",
			//					reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag, reqSlotTag, dmaIndex, numOfNvmeBlock, devAddr);

			lld_hdma_set_auto_tx_dma(p_reqEntry->nvmeCmdSlotTag, dmaIndex, devAddr, NVME_COMMAND_AUTO_COMPLETION_ON);

			numOfNvmeBlock++;
			dmaIndex++;
			devAddr += BYTES_PER_NVME_BLOCK;
		}

		HOST_DMA_FIFO_CNT_REG dma_tail_fifo;
		dma_tail_fifo.dword = lld_hdma_get_dma_tail_fifo_cnt();

		p_reqEntry->nvmeDmaInfo.reqTail =  dma_tail_fifo.autoDmaTx;
		p_reqEntry->nvmeDmaInfo.overFlowCnt = lld_hdma_get_auto_dma_tx_overflow_cnt();
	}
	else
	{
		assert(!"[WARNING] Not supported reqCode [WARNING]");
	}

	NVME_COMMAND_ENTRY* p_cmdEntry = nvme_command_context_get_command_entry(p_reqEntry->nvmeCmdSlotTag);

	p_cmdEntry->hdmaWaitReqEntryCnt++;
}

void hal_host_handle_hdma_result()
{
	unsigned int reqSlotTag, prevReq;
	unsigned int rxDone, txDone;

	reqSlotTag = RequestAllocation_PeekReqEntryFromNvmeDmaReqQ();
	rxDone = 0;
	txDone = 0;

	NVME_COMMAND_ENTRY* p_cmdEntry = NULL;
	SSD_REQ_FORMAT* p_reqEntry = NULL;

	while(reqSlotTag != REQ_SLOT_TAG_NONE)
	{
		p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);
		p_cmdEntry = nvme_command_context_get_command_entry(p_reqEntry->nvmeCmdSlotTag);

		prevReq = p_reqEntry->prevReq;

		if (REQ_CODE_RxDMA == p_reqEntry->reqCode)
		{
			if(!rxDone)
			{
				rxDone = lld_hdma_check_auto_rx_dma_partial_done(p_reqEntry->nvmeDmaInfo.reqTail , p_reqEntry->nvmeDmaInfo.overFlowCnt);
			}

			if(rxDone)
			{
				//xil_printf("[HDMA resp] write cmdtag:%u, reqSlotTag:%u\r\n",
				//		p_reqEntry->nvmeCmdSlotTag, reqSlotTag);

				//unsigned int* p_buf = (unsigned int*)BufferManagement_GetDataBufAddr(p_reqEntry->dataBufInfo.entry);

				//xil_printf("[buf_idx:u, addr:0x%X] 0x%X, 0x%X, 0x%X, 0x%X\r\n",
				//		p_reqEntry->dataBufInfo.entry, p_buf, *p_buf, *(p_buf+1), *(p_buf+2), *(p_buf+3));

				RequestAllocation_SelectiveGetFromNvmeDmaReqQ(reqSlotTag);

				p_cmdEntry->hdmaCompleteReqEntryCnt++;

				if (p_cmdEntry->totalReqEntryCnt == p_cmdEntry->hdmaCompleteReqEntryCnt)
				{
					if (FALSE == p_reqEntry->nvmeDmaInfo.fua)
					{
						nvme_command_context_decrease_write_outstanding_count();
						nvme_command_context_increase_complete_command_count();
					}
				}
			}
		}
		else //if (REQ_CODE_TxDMA == p_reqEntry->reqCode)
		{
			if(!txDone)
			{
				txDone = lld_hdma_check_auto_tx_dma_partial_done(p_reqEntry->nvmeDmaInfo.reqTail , p_reqEntry->nvmeDmaInfo.overFlowCnt);
			}

			if(txDone)
			{
				//xil_printf("[HDMA resp] read cmdtag:%u, reqSlotTag:%u\r\n",
				//									reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag, reqSlotTag);

				//unsigned int* p_buf = (unsigned int*)BufferManagement_GetDataBufAddr(p_reqEntry->dataBufInfo.entry);

				//				xil_printf("[buf_idx:u, addr:0x%X] 0x%X, 0x%X, 0x%X, 0x%X\r\n",
				//						p_reqEntry->dataBufInfo.entry, p_buf, *p_buf, *(p_buf+1), *(p_buf+2), *(p_buf+3));

				/* Barrier FTL 
					DMA from Host is completed 
				 	Set the epoch state */
				RequestAllocation_SelectiveGetFromNvmeDmaReqQ(reqSlotTag);

				p_cmdEntry->hdmaCompleteReqEntryCnt++;

				if (p_cmdEntry->totalReqEntryCnt == p_cmdEntry->hdmaCompleteReqEntryCnt)
				{
					nvme_command_context_decrease_read_outstanding_count();
					nvme_command_context_increase_complete_command_count();
				}
			}
		}

		reqSlotTag = prevReq;
	}
}

unsigned int hal_host_set_nvme_ccen(void)
{
	unsigned int ccEnSet = lld_nvme_get_cc_en();

	if(1 == ccEnSet)
	{
		lld_nvme_set_admin_queue(1, 1, 1);
		lld_nvme_set_csts_rdy(1);
	}

	return ccEnSet;
}

unsigned int hal_host_clear_nvme_ccen(void)
{
	unsigned int ccEnSet = lld_nvme_get_cc_en();

	if(0 == ccEnSet)
	{
		lld_nvme_set_csts_shst(0);
		lld_nvme_set_csts_rdy(0);
	}

	return ccEnSet;
}

unsigned int hal_host_nvme_shutdown(void)
{
	NVME_STATUS_REG nvmeReg;
	nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);

	if (0 != nvmeReg.ccShn)
	{
		lld_nvme_set_csts_shst(1);

		for(unsigned int qID = 0; qID < 8; qID++)
		{
			lld_nvme_set_io_cq(qID, 0, 0, 0, 0, 0, 0);
			lld_nvme_set_io_sq(qID, 0, 0, 0, 0, 0);
		}

		lld_nvme_set_admin_queue(0, 0, 0);
		lld_nvme_set_csts_shst(2);

		return 1;
	}

	return 0;
}

unsigned int hal_host_fetch_nvme_command(NVME_COMMAND_ENTRY* p_command_list)
{
	NVME_COMMAND_ENTRY cmd_entry;

	cmd_entry.totalReqEntryCnt = 0;
	cmd_entry.hdmaWaitReqEntryCnt = 0;
	cmd_entry.hdmaCompleteReqEntryCnt = 0;
	cmd_entry.NandWaitReqEntryCnt = 0;
	cmd_entry.NandCompleteReqEntryCnt = 0;

	unsigned int b_fetch_command = lld_nvme_get_cmd(&cmd_entry.qID, &cmd_entry.cmdSlotTag, &cmd_entry.cmdSeqNum, cmd_entry.cmdDword);

	if (FALSE != b_fetch_command)
	{
		memcpy((void*)&p_command_list[cmd_entry.cmdSlotTag], (void*)&cmd_entry, sizeof(NVME_COMMAND_ENTRY));

		return cmd_entry.cmdSlotTag;
	}

	return INVALID_CMD_SLOT_TAG;
}

void hal_host_completion_nvme_command(unsigned int cmd_slot_tag, unsigned int cmd_specific, unsigned short status_field)
{
	NVME_COMPLETION nvmeCPL = {0,};

	nvmeCPL.specific = cmd_specific;
	nvmeCPL.statusFieldWord = status_field;

	lld_nvme_set_auto_cpl(cmd_slot_tag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
}
