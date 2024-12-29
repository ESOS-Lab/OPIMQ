//////////////////////////////////////////////////////////////////////////////////
// nvme_io_cmd.c for Cosmos+ OpenSSD
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
// Module Name: NVMe IO Command Handler
// File Name: nvme_io_cmd.c
//
// Version: v1.0.1
//
// Description:
//   - handles NVMe IO command
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.1
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include "../../debug.h"
#include "io_access.h"

#include "../../hal/lld/lld_nvme.h"
#include "../../hal/hal_host.h"

#include "nvme.h"
#include "nvme_io_cmd.h"
#include "nvme_main.h"

#include "../../ftl/ftl_config.h"
#include "../../request_format.h"
#include "../../request_transform.h"
#include "../../request_schedule.h"
#include "../../request_allocation.h"

#include "xil_exception.h"

#include "../../data_buffer.h"
#include "../../ftl/address_translation.h"
#include "../../ftl/barrier_ftl.h"

//Juwon Added
#include "xtime_l.h"
#include "xparameters.h"


static void _sliceIOCommandToReqEntries(NVME_COMMAND_ENTRY *p_cmdEntry)
{
	unsigned int cmdSlotTag = p_cmdEntry->cmdSlotTag;
	NVME_IO_COMMAND *p_nvmeIOCmd = (NVME_IO_COMMAND*)p_cmdEntry->cmdDword;

	unsigned int reqCode, tempNumOfNvmeBlock, nvmeDmaStartIndex;

	if (NVME_OPC_NVM_WRITE == p_nvmeIOCmd->OPC)
	{
		reqCode = REQ_CODE_WRITE;
	}
	else if (NVME_OPC_NVM_READ == p_nvmeIOCmd->OPC)
	{
		reqCode = REQ_CODE_READ;
	}
	else
	{
		assert(!"[WARNING] Not supported command code [WARNING]");
	}

	IO_WRITE_COMMAND_DW12 cdw12;
	cdw12.dword = p_nvmeIOCmd->dword[12];

	unsigned int startLba = p_nvmeIOCmd->dword[10];
	unsigned int numOfLba = (cdw12.NLB + 1);
	unsigned int endLba = ((startLba + numOfLba) - 1);
	unsigned int lbaOffset = (startLba % NVME_BLOCKS_PER_SLICE);

	nvmeDmaStartIndex = 0;

	unsigned int startLsa = (startLba / NVME_BLOCKS_PER_SLICE); // if 2TB over, start lba high(dw11) is included
	unsigned int currentLsa = startLsa;
	unsigned int endLsa = (endLba / NVME_BLOCKS_PER_SLICE);

	//first transform
	unsigned int reqSlotTag = REQ_SLOT_TAG_NONE;
	SSD_REQ_FORMAT* p_reqEntry = NULL;

	do
	{
		if (NVME_BLOCKS_PER_SLICE < numOfLba)
		{
			tempNumOfNvmeBlock = NVME_BLOCKS_PER_SLICE;
		}
		else
		{
			tempNumOfNvmeBlock = numOfLba;
		}

		if (currentLsa != startLsa)
		{
			lbaOffset = 0;
		}

		reqSlotTag = RequestAllocation_GetFreeReqEntry();
		p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

		p_reqEntry->reqType = REQ_TYPE_SLICE;
		p_reqEntry->reqCode = reqCode;
		p_reqEntry->nvmeCmdSlotTag = cmdSlotTag;
		p_reqEntry->logicalSliceAddr = currentLsa;
		p_reqEntry->nvmeDmaInfo.startIndex = nvmeDmaStartIndex;
		p_reqEntry->nvmeDmaInfo.nvmeBlockOffset = lbaOffset;
		p_reqEntry->nvmeDmaInfo.numOfNvmeBlock = tempNumOfNvmeBlock;

		//xil_printf("IO Command[tag:%u, reqSlotTag:%u], lsa:0x%X, dmastartidx:0x%X, lbaoffset:0x%X, numlba:0x%X\r\n",
		//		cmdSlotTag, reqSlotTag, currentLsa, nvmeDmaStartIndex, lbaOffset, tempNumOfNvmeBlock);

		if (REQ_CODE_WRITE == reqCode)
		{
			p_reqEntry->nvmeDmaInfo.fua = cdw12.FUA;
//#if (SUPPORT_BARRIER_FTL == 1)
			p_reqEntry->stream_id_1 = p_nvmeIOCmd->stream_id_1;
			p_reqEntry->stream_id_2 = p_nvmeIOCmd->stream_id_2;
			p_reqEntry->epoch_id_1 = p_nvmeIOCmd->epoch_id_1;
			p_reqEntry->epoch_id_2 = p_nvmeIOCmd->epoch_id_2;
			p_reqEntry->barrier_flag = 0;
//#endif
#if (SUPPORT_BARRIER_FTL == 1)
#if (BARRIER_IN_DMA == 0)
			uint32_t sid1 = p_reqEntry->stream_id_1 ;
			uint32_t sid2 = p_reqEntry->stream_id_2;
			uint32_t eid1 = p_reqEntry->epoch_id_1;
			uint32_t eid2 = p_reqEntry->epoch_id_2;
			uint8_t is_last_slice = !(endLsa >= currentLsa);
			p_reqEntry->barrier_flag = (is_last_slice)? cdw12.barrier_flag: 0;
			if (sid1){
				barrier_set_epoch_state(sid1, eid1, p_reqEntry->barrier_flag, 1);
				if (sid2){
					barrier_set_epoch_state(sid2, eid2, p_reqEntry->barrier_flag, 1);
				}
			}
#endif
#endif
			

		}

		RequestAllocation_MoveToSliceReqQ(reqSlotTag);

		currentLsa++;
		nvmeDmaStartIndex += tempNumOfNvmeBlock;
		numOfLba -= tempNumOfNvmeBlock;

		p_cmdEntry->totalReqEntryCnt++;
	} while (endLsa >= currentLsa);

#if (SUPPORT_BARRIER_FTL == 1)

	if (REQ_CODE_WRITE == reqCode)
	{
		p_reqEntry->barrier_flag = cdw12.barrier_flag;
#if (PRINT_DEBUG_MAP == 1)
		//if (p_reqEntry->stream_id_1 > 0)
		//	xil_printf("[ OPIMQ ] sid1:%u eid1:%u sid2:%u eid2:%u barrier_flag:%u\r\n",
		//					p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1,
		//					p_reqEntry->stream_id_2, p_reqEntry->epoch_id_2, 
		//					p_reqEntry->barrier_flag );
		
#endif


	}
#else
	if (REQ_CODE_WRITE == reqCode)
	{
		//if (p_reqEntry->stream_id_1 > 0)
		//xil_printf("[ OPIMQ ] sid1:%u eid1:%u sid2:%u eid2:%u barrier_flag:%u\r\n",
		//					p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1,
		//					p_reqEntry->stream_id_2, p_reqEntry->epoch_id_2, 
		//					p_reqEntry->barrier_flag );
							//0, 0,
							//0, 0, 0);
	}

#endif
}



static void _EvictDataBufEntry(unsigned int originReqSlotTag)
{
	unsigned int reqSlotTag, virtualSliceAddr, dataBufEntryIdx;

	dataBufEntryIdx = reqPoolPtr->reqPool[originReqSlotTag].dataBufInfo.entry;

	DATA_BUF_ENTRY* p_dataBufEntry = BufferManagement_GetDataBufEntry(dataBufEntryIdx);
	//xil_printf("[JWDBG] %s: try eviction\n", __func__);	
	if(p_dataBufEntry->dirty == DATA_BUF_DIRTY)
	{
		//xil_printf("[JWDBG] %s: do eviction\n", __func__);	
		reqSlotTag = RequestAllocation_GetFreeReqEntry();
		/* If the buffer entry to be evicted is allocated for epoch, check the corresponding mappable state.
		   If the mappable state is update, then update mapping table.
		   Otherwise, call AddrTransSuspen. Do not update the mapping information, and insert it to the suspension array
		   of current stream. */

#if (SUPPORT_BARRIER_FTL == 1)
		if (p_dataBufEntry->stream_id_1) {
			//xil_printf("\r\n [ Evict Buffer %u ] SID %u EID %u Old_SID %u Old_EID %u\r\n", dataBufEntryIdx,
			//		reqPoolPtr->reqPool[originReqSlotTag].stream_id_1, reqPoolPtr->reqPool[originReqSlotTag].epoch_id_1,
			//		p_dataBufEntry->stream_id_1, p_dataBufEntry->epoch_id_1 );

			uint32_t sid1 = p_dataBufEntry->stream_id_1;
			uint32_t eid1 = p_dataBufEntry->epoch_id_1;
			uint32_t mappable_1 = p_dataBufEntry->mappable_1;
			uint32_t sid2 = p_dataBufEntry->stream_id_2;
			uint32_t eid2 = p_dataBufEntry->epoch_id_2;
			uint32_t mappable_2 = p_dataBufEntry->mappable_2;

			//xil_printf("\r\n [ Flush Page by Eviction ] Buf_index %u SID %u EID %u\r\n", dataBufEntryIdx,
			//		p_dataBufEntry->stream_id_1,  p_dataBufEntry->epoch_id_1);

			if ( (mappable_1 || is_mappable(sid1, eid1)) 
				&& (mappable_2 || is_mappable(sid2, eid2))) {  // This case, slice can be mappable in both stream1 and stream2.
				virtualSliceAddr = AddrTransWrite(p_dataBufEntry->logicalSliceAddr);
				//xil_printf("%s bef update epoch info 1\n", __func__);
				update_epoch_info(sid1, eid1, __func__);
				//xil_printf("%s bef update epoch info 2\n", __func__);
				update_epoch_info(sid2, eid2, __func__);
			} else if ( (mappable_1 || is_mappable(sid1, eid1)) 
				&& !(mappable_2 || is_mappable(sid2, eid2))) { 	// Case 2: Suspend mapping update (Stream 2)
				virtualSliceAddr = AddrTransSuspen(p_dataBufEntry->logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
				barrier_insert_suspension_array_dual_stream(p_dataBufEntry->logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 2);	
#endif
			} else if ( (mappable_2 || is_mappable(sid2, eid2))
				&& !(mappable_1 || is_mappable(sid1, eid1))) { // Case 1	: Suspend mapping update (Stream 1
				virtualSliceAddr = AddrTransSuspen(p_dataBufEntry->logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
				barrier_insert_suspension_array_dual_stream(p_dataBufEntry->logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 1);	
#endif
			} else {	//Case 0: Suspend both mapping update
				virtualSliceAddr = AddrTransSuspen(p_dataBufEntry->logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
				barrier_insert_suspension_array_dual_stream(p_dataBufEntry->logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 0);	
#endif
			}
		} else {
			virtualSliceAddr =  AddrTransWrite(p_dataBufEntry->logicalSliceAddr);
		}	
#else
		virtualSliceAddr =  AddrTransWrite(p_dataBufEntry->logicalSliceAddr);
#endif
		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
		reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = reqPoolPtr->reqPool[originReqSlotTag].nvmeCmdSlotTag;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = p_dataBufEntry->logicalSliceAddr;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
		reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntryIdx;

		/* Update NandInfo for flush the data buffer. */
#if (SUPPORT_BARRIER_FTL == 1)
		reqPoolPtr->reqPool[reqSlotTag].stream_id_1 = p_dataBufEntry->stream_id_1;
		reqPoolPtr->reqPool[reqSlotTag].stream_id_2 = p_dataBufEntry->stream_id_2;
		reqPoolPtr->reqPool[reqSlotTag].epoch_id_1 = p_dataBufEntry->epoch_id_1;
		reqPoolPtr->reqPool[reqSlotTag].epoch_id_2 = p_dataBufEntry->epoch_id_2;
		reqPoolPtr->reqPool[reqSlotTag].mappable_1 = p_dataBufEntry->mappable_1;
		reqPoolPtr->reqPool[reqSlotTag].mappable_2 = p_dataBufEntry->mappable_2;
#endif
		BufferManagement_UpdateBufEntryInfoBlockingReq(dataBufEntryIdx, reqSlotTag);
		reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

		SelectLowLevelReqQ(reqSlotTag);

		p_dataBufEntry->dirty = DATA_BUF_CLEAN;
#if (SUPPORT_BARRIER_FTL == 1)
		p_dataBufEntry->stream_id_1 = 0;
		p_dataBufEntry->stream_id_2 = 0;
		p_dataBufEntry->epoch_id_1 = 0;
		p_dataBufEntry->epoch_id_2 = 0;
		p_dataBufEntry->mappable_1 = 0;
		p_dataBufEntry->mappable_2 = 0;
#endif
	}
}

static void _DataReadFromNand(unsigned int originReqSlotTag)
{
	unsigned int reqSlotTag, virtualSliceAddr;

	virtualSliceAddr =  AddrTransRead(reqPoolPtr->reqPool[originReqSlotTag].logicalSliceAddr);

	if(virtualSliceAddr != VSA_FAIL)
	{
		reqSlotTag = RequestAllocation_GetFreeReqEntry();

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
		reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = reqPoolPtr->reqPool[originReqSlotTag].nvmeCmdSlotTag;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = reqPoolPtr->reqPool[originReqSlotTag].logicalSliceAddr;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;

		reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = reqPoolPtr->reqPool[originReqSlotTag].dataBufInfo.entry;
		BufferManagement_UpdateBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
		reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

		SelectLowLevelReqQ(reqSlotTag);
	}
}

static void _moveSliceEntriesForNextOperation()
{
	//SP: move io(read write) slice entries to other queue (dma, nand, etc)
	unsigned int reqSlotTag, dataBufEntry;
	//xil_printf("%s start\n", __func__);
	int tmp = 1000, cnt;	
	cnt = 0;
	unsigned int ret = 0;
	while (0 != (ret = RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_SLICE, 0, 0)))
	{
		//if (cnt > 0)
			//xil_printf("ret: %d \n", ret);
		//cnt += 1;
		reqSlotTag = RequestAllocation_GetReqEntryFromSliceReqQ();
		if (REQ_SLOT_TAG_FAIL == reqSlotTag)
		{
			//xil_printf("ret: %d !!\n", ret);
			return;
		}

#if (SUPPORT_BARRIER_FTL == 1)
		dataBufEntry = BufferManagement_CheckBufHit(reqSlotTag);
		//xil_printf("hit done\n");
		if (dataBufEntry != DATA_BUF_FAIL) {
			if (dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1 > 0) {
				//xil_printf("ssamg 1\n");
				//data buffer miss, allocate a new buffer entry
		
				dataBufEntry = BufferManagement_AllocBuf();
				reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;

				assert(dataBufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT);
				//clear the allocated data buffer entry being used by a previous request
				//xil_printf("evict 1\n");
				_EvictDataBufEntry(reqSlotTag);
				//xil_printf("evict 1 done\n");

				//update meta-data of the allocated data buffer entry
				dataBufMapPtr->dataBuf[dataBufEntry].logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
				BufferManagement_AddBufToHashList(dataBufEntry);

				if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
				{
					_DataReadFromNand(reqSlotTag);
				}
				else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE)
				{
					if(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock != NVME_BLOCKS_PER_SLICE) // for read modify write
					{
					_DataReadFromNand(reqSlotTag);
					}
				}
				//xil_printf("ssamg 2\n");
			} else {
				//data buffer hit
				reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;
			}
		} else {
			//data buffer miss, allocate a new buffer entry
				//xil_printf("ssamg 3\n");
			dataBufEntry = BufferManagement_AllocBuf();
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;

			assert(dataBufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT);

			//clear the allocated data buffer entry being used by a previous request
				//xil_printf("evict 2 \n");
			_EvictDataBufEntry(reqSlotTag);
				//xil_printf("evict 2 done\n");

			//update meta-data of the allocated data buffer entry
			dataBufMapPtr->dataBuf[dataBufEntry].logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
			BufferManagement_AddBufToHashList(dataBufEntry);

			if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
			{
				_DataReadFromNand(reqSlotTag);
			}
		    else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE){
		    	if(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock != NVME_BLOCKS_PER_SLICE) // for read modify write
		    	{
		    		_DataReadFromNand(reqSlotTag);
		    	}
		    }
				//xil_printf("ssamg 4\n");
		}
#else
		//allocate a data buffer entry for this request
		dataBufEntry = BufferManagement_CheckBufHit(reqSlotTag);
		if (dataBufEntry != DATA_BUF_FAIL)
		{
			assert(dataBufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT);
			//data buffer hit
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;
		}
		else
		{
			//data buffer miss, allocate a new buffer entry
			dataBufEntry = BufferManagement_AllocBuf();
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;

			assert(dataBufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT);

			//clear the allocated data buffer entry being used by a previous request
			_EvictDataBufEntry(reqSlotTag);

			//update meta-data of the allocated data buffer entry
			dataBufMapPtr->dataBuf[dataBufEntry].logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
			BufferManagement_AddBufToHashList(dataBufEntry);

			if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
			{
				_DataReadFromNand(reqSlotTag);
			}
			else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE)
			{
				if(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock != NVME_BLOCKS_PER_SLICE) // for read modify write
				{
					_DataReadFromNand(reqSlotTag);
				}
			}
		}
#endif

#if (SUPPORT_BARRIER_FTL == 1)
		//xil_printf("hoe\n");	
		if (reqPoolPtr->reqPool[reqSlotTag].stream_id_1 > 0) {
			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1 = reqPoolPtr->reqPool[reqSlotTag].stream_id_1;
			dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_1 = reqPoolPtr->reqPool[reqSlotTag].epoch_id_1;
			
			if (reqPoolPtr->reqPool[reqSlotTag].stream_id_2 > 0) {
				dataBufMapPtr->dataBuf[dataBufEntry].stream_id_2 = reqPoolPtr->reqPool[reqSlotTag].stream_id_2;
				dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_2 = reqPoolPtr->reqPool[reqSlotTag].epoch_id_2;
			} else {
				dataBufMapPtr->dataBuf[dataBufEntry].stream_id_2 = 0;
				dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_2 = 0;

			}

			//xil_printf("[Buffer Allocation] sid %u eid %u sid2 %u eid2 %u alloc_buf_index: %u\r\n",
			//			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1, 
			//			dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_1, 
			//			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_2, 
			//			dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_2, 
			//			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry);

				//dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1 = reqPoolPtr->reqPool[reqSlotTag].stream_id1;
				//dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_1 = reqPoolPtr->reqPool[reqSlotTag].epoch_id1;
		} else {
			// Juwon Added
			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1 = 0;
			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_2 = 0;
			dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_1 = 0;
			dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_2 = 0;
			//xil_printf("\r\n [Buffer INIT] sid %u eid %u alloc_buf_index: %u\r\n",
			//			dataBufMapPtr->dataBuf[dataBufEntry].stream_id_1, dataBufMapPtr->dataBuf[dataBufEntry].epoch_id_1, reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry);

		}
#endif

		//transform this slice request to nvme request
		if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE)
		{
			dataBufMapPtr->dataBuf[dataBufEntry].dirty = DATA_BUF_DIRTY;
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_RxDMA;
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
		{
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_TxDMA;
		}
		else
		{
			assert(!"[WARNING] Not supported reqCode. [WARNING]");
		}

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NVME_DMA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;

		//xil_printf("gasa 1 done\n");
		BufferManagement_UpdateBufEntryInfoBlockingReq(dataBufEntry, reqSlotTag);
		//xil_printf("gasa 2 done\n");
		SelectLowLevelReqQ(reqSlotTag);
		//xil_printf("gasa 3 done\n");
	}
	//xil_printf("shit ret: %d\n", ret);
}

static void _processReadCommand(NVME_COMMAND_ENTRY *p_cmdEntry)
{
	NVME_IO_COMMAND *p_readCommand = (NVME_IO_COMMAND*)p_cmdEntry->cmdDword;
	//IO_READ_COMMAND_DW12 readInfo12;
	//IO_READ_COMMAND_DW13 readInfo13;
	//IO_READ_COMMAND_DW15 readInfo15;
	unsigned int startLba[2];
	//unsigned int nlb;

	//readInfo12.dword = p_readCommand->dword[12];
	//readInfo13.dword = nvmeIOCmd->dword[13];
	//readInfo15.dword = nvmeIOCmd->dword[15];

	startLba[0] = p_readCommand->dword[10];
	startLba[1] = p_readCommand->dword[11];
	//nlb = readInfo12.NLB;

	//SP: modify error completion (not assert)
	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((p_readCommand->PRP1[0] & 0x3) == 0 && (p_readCommand->PRP2[0] & 0x3) == 0); //error
	ASSERT(p_readCommand->PRP1[1] < 0x10000 && p_readCommand->PRP2[1] < 0x10000);

	_sliceIOCommandToReqEntries(p_cmdEntry);
	//ReqTransNvmeToSlice(p_cmdEntry);
}


static void _processWriteCommand(NVME_COMMAND_ENTRY *p_cmdEntry)
{
	NVME_IO_COMMAND *p_writeCommand = (NVME_IO_COMMAND*)p_cmdEntry->cmdDword;
	IO_WRITE_COMMAND_DW12 writeInfo12;
	//IO_WRITE_COMMAND_DW13 writeInfo13;
	//IO_WRITE_COMMAND_DW15 writeInfo15;
	unsigned int startLba[2];

	writeInfo12.dword = p_writeCommand->dword[12];
	//writeInfo13.dword = p_writeCommand->dword[13];
	//writeInfo15.dword = p_writeCommand->dword[15];

	// for test
	//writeInfo12.FUA = 1;
	//p_writeCommand->dword[12] = writeInfo12.dword;

	if (1 == writeInfo12.FUA)
	{
		xil_printf("\n[cmdSlotTag:%u] slb:0x%X, nlb:0x%X, fua:0x%X\r\n", p_cmdEntry->cmdSlotTag, p_writeCommand->dword[10], (unsigned short)p_writeCommand->dword[12], writeInfo12.FUA);
	}

	startLba[0] = p_writeCommand->dword[10];
	startLba[1] = p_writeCommand->dword[11];

	//SP: modify error completion (not assert)
	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((p_writeCommand->PRP1[0] & 0xF) == 0 && (p_writeCommand->PRP2[0] & 0xF) == 0);
	ASSERT(p_writeCommand->PRP1[1] < 0x10000 && p_writeCommand->PRP2[1] < 0x10000);
/*
#if (SUPPORT_BARRIER_FTL == 1)
	if (INVALID_STREAM_ID != p_writeCommand->stream_id_1)
	{
		xil_printf("W [tag:%u] slb:0x%X, nlb:0x%X, fua:0x%X\r\n", p_cmdEntry->cmdSlotTag, p_writeCommand->dword[10], (unsigned short)p_writeCommand->dword[12], writeInfo12.FUA);
		xil_printf("bflag:%u, sid1:0x%X, eid1:0x%X, sid2:0x%X, eid2:0x%X, DW2 0x%X, DW3 0x%X\r\n",
				writeInfo12.barrier_flag,
				p_writeCommand->stream_id_1,
				p_writeCommand->epoch_id_1,
				p_writeCommand->stream_id_2,
				p_writeCommand->epoch_id_2,
				p_writeCommand->dword[2],
				p_writeCommand->dword[3]
				);
	}
#endif
*/
	_sliceIOCommandToReqEntries(p_cmdEntry);
	//ReqTransNvmeToSlice(p_cmdEntry);

}

#if (MEASURE_LATENCY == 1)
XTime startTime_flush, endTime_flush;
XTime startTime_flushsync, endTime_flushsync;
XTime startTime_flushsearch, endTime_flushsearch;
XTime startTime_write, endTime_write;

void print_latency(XTime startTime, XTime endTime, void *str)
{
	char tmpString[1024];

	sprintf(tmpString, 
	"%s: %lf st: %lf et: %lf \n", str, 
		(double) (endTime - startTime)/COUNTS_PER_SECOND, 
		(double) (startTime)/COUNTS_PER_SECOND,
		(double) (endTime)/COUNTS_PER_SECOND);
	xil_printf("%s", tmpString);
}
#endif

static void _processFlushCommand(void)
{
	//xil_printf("\r\n [ OPIMQ ] Flush Command  \r\n"); //Jieun add
#if (MEASURE_LATENCY == 1)
	XTime_GetTime(&startTime_flushsync);
#endif

	FlushWriteDataToNand();
#if (MEASURE_LATENCY == 1)
	XTime_GetTime(&endTime_flushsync);
#endif
}

void NVMCommand_process(NVME_COMMAND_ENTRY *p_cmdEntry)
{
	NVME_IO_COMMAND *p_nvmCommand = (NVME_IO_COMMAND*)p_cmdEntry->cmdDword;

	unsigned int cmdSlotTag = p_cmdEntry->cmdSlotTag;

	//xil_printf("NVM OPC = 0x%X\r\n", p_nvmCommand->OPC);
	//xil_printf("NSID = 0x%X\r\n", p_nvmCommand->NSID);
	//xil_printf("PRP1[63:32] = 0x%X, PRP1[31:0] = 0x%X\r\n", p_nvmCommand->PRP1[1], p_nvmCommand->PRP1[0]);
	//xil_printf("PRP2[63:32] = 0x%X, PRP2[31:0] = 0x%X\r\n", p_nvmCommand->PRP2[1], p_nvmCommand->PRP2[0]);
	//xil_printf("dword10 = 0x%X\r\n", p_nvmCommand->dword10);
	//xil_printf("dword11 = 0x%X\r\n", p_nvmCommand->dword11);
	//xil_printf("dword12 = 0x%X\r\n", p_nvmCommand->dword12);

#if 0
	if (1 != p_nvmCommand->NSID)
	{
		xil_printf("invalid nsid [tag:%u, nsid:%u]\r\n", cmdSlotTag, p_nvmCommand->NSID);
		hal_host_completion_nvme_command(cmdSlotTag, 0, SC_INVALID_NAMESPACE_OR_FORMAT);

		nvme_command_context_increase_complete_command_count();
		return;
	}
#endif


	switch(p_nvmCommand->OPC)
	{
		case NVME_OPC_NVM_FLUSH:
		{
			//xil_printf("IO Flush Command[tag:%u]\r\n", cmdSlotTag);
#if (MEASURE_LATENCY == 1)
			XTime_GetTime(&startTime_flush);
#endif
			_processFlushCommand();

			hal_host_completion_nvme_command(cmdSlotTag, 0, SC_SUCCESSFUL_COMPLETION);

			nvme_command_context_increase_complete_command_count();

			//Post Processing
			//barrier_search_suspension_list();

#if (MEASURE_LATENCY == 1)
			XTime_GetTime(&endTime_flush);
#endif

#if (SUPPORT_BARRIER_FTL == 1)
#if (SUPPORT_SEARCH_SUSPENSION_LIST == 1)

#if (MEASURE_LATENCY == 1)
			XTime_GetTime(&startTime_flushsearch);
#endif
			barrier_search_suspension_list_dual_stream();
#if (MEASURE_LATENCY == 1)
			XTime_GetTime(&endTime_flushsearch);
#endif

#endif
#endif

#if (MEASURE_LATENCY == 1)
			print_latency(startTime_flush, endTime_flush, "Flush Latency");
			print_latency(startTime_flushsync, endTime_flushsync, "FlushSync Latency");
			print_latency(startTime_flushsearch, endTime_flushsearch, "FlushSearch Latency");
#endif
			//xil_printf("\r\n [ OPIMQ ] Flush DONE  \r\n"); //Jieun add
			//break;
			return;
		}
		case NVME_OPC_NVM_WRITE:
		{
			nvme_command_context_increase_write_outstanding_count();
			//xil_printf("IO Write Command[tag:%u], slb:0x%X, nlb:0x%X\r\n", cmdSlotTag, p_nvmCommand->dword10, (unsigned short)p_nvmCommand->dword12);

#if (MEASURE_LATENCY == 1)
			XTime_GetTime(&startTime_write);
#endif
			_processWriteCommand(p_cmdEntry);
			break;
		}
		case NVME_OPC_NVM_READ:
		{
			nvme_command_context_increase_read_outstanding_count();
			//xil_printf("IO Read Command[tag:%u], slb:0x%X, nlb:0x%X\r\n", cmdSlotTag, p_nvmCommand->dword10, (unsigned short)p_nvmCommand->dword12);
			_processReadCommand(p_cmdEntry);
			break;
		}
		default:
		{
			xil_printf("Not Support IO Command OPC: %X\r\n", p_nvmCommand->OPC);
			ASSERT(0);
			break;
		}
	}

	//Jieun add
	if (p_nvmCommand->OPC != NVME_OPC_NVM_FLUSH){
		//xil_printf("shival 1\n");
		_moveSliceEntriesForNextOperation();
#if (PRINT_DEBUG == 1)
		//xil_printf("shival 2\n");
#endif
	}

#if (MEASURE_LATENCY == 1)
	if (p_nvmCommand->OPC == NVME_OPC_NVM_WRITE){
		XTime_GetTime(&endTime_write);
//		print_latency(startTime_write, endTime_write, "Write Latency");
	}
#endif
	//if (p_nvmCommand->OPC == NVME_OPC_NVM_WRITE)
	//	xil_printf("write DONE\n");

	//xil_printf("%s DONE\n", __func__);

}

