//////////////////////////////////////////////////////////////////////////////////
// request_transform.c for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//			      Sangjin Lee <sjlee@enc.hanyang.ac.kr>
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
// Module Name: Request Scheduler
// File Name: request_transform.c
//
// Version: v1.0.0
//
// Description:
//	 - transform request information
//   - check dependency between requests
//   - issue host DMA request to host DMA engine
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include <assert.h>
#include "memory_map.h"

#include "host/nvme/nvme.h"
#include "hal/lld/lld_hdma.h"
#include "hal/hal_host.h"

#include "ftl/ftl_config.h"
#include "ftl/barrier_ftl.h"

P_ROW_ADDR_DEPENDENCY_TABLE rowAddrDependencyTablePtr;

static unsigned int _CheckBufDep(unsigned int reqSlotTag)
{
	if(reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq == REQ_SLOT_TAG_NONE)
	{
		return BUF_DEPENDENCY_REPORT_PASS;
	}
	else
	{
		return BUF_DEPENDENCY_REPORT_BLOCKED;
	}
}

static unsigned int _CheckRowAddrDep(unsigned int reqSlotTag, unsigned int checkRowAddrDepOpt)
{
	SSD_REQ_FORMAT* p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	if(REQ_OPT_NAND_ADDR_VSA != p_reqEntry->reqOpt.nandAddr)
	{
		assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");
	}

	unsigned int dieNo,chNo, wayNo, blockNo, pageNo;

	dieNo = Vsa2VdieTranslation(p_reqEntry->nandInfo.virtualSliceAddr);
	chNo =  Vdie2PchTranslation(dieNo);
	wayNo = Vdie2PwayTranslation(dieNo);
	blockNo = Vsa2VblockTranslation(p_reqEntry->nandInfo.virtualSliceAddr);
	pageNo = Vsa2VpageTranslation(p_reqEntry->nandInfo.virtualSliceAddr);

	if (REQ_CODE_READ == p_reqEntry->reqCode)
	{
		if (ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT == checkRowAddrDepOpt)
		{
			if (rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag)
			{
				RequestScheduler_SyncReleaseEraseReq(chNo, wayNo, blockNo);
			}

			if(pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
			{
				return ROW_ADDR_DEPENDENCY_REPORT_PASS;
			}

			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
		}
		else if(ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE == checkRowAddrDepOpt)
		{
			if (pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
			{
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt--;

				return	ROW_ADDR_DEPENDENCY_REPORT_PASS;
			}
		}
		else
		{
			assert(!"[WARNING] Not supported checkRowAddrDepOpt [WARNING]");
		}
	}
	else if (REQ_CODE_WRITE == p_reqEntry->reqCode)
	{
		if (pageNo == rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
		{
			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage++;

			return ROW_ADDR_DEPENDENCY_REPORT_PASS;
		}
	}
	else if (REQ_CODE_ERASE == p_reqEntry->reqCode)
	{
		if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage == p_reqEntry->nandInfo.programmedPageCnt)
		{
			if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt == 0)
			{
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 0;

				return ROW_ADDR_DEPENDENCY_REPORT_PASS;
			}
		}

		if(ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT == checkRowAddrDepOpt)
		{
			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 1;
		}
		else if(ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE == checkRowAddrDepOpt)
		{
			//pass, go to return
		}
		else
		{
			assert(!"[WARNING] Not supported checkRowAddrDepOpt [WARNING]");
		}
	}
	else
	{
		assert(!"[WARNING] Not supported reqCode [WARNING]");
	}

	return ROW_ADDR_DEPENDENCY_REPORT_BLOCKED;
}

static unsigned int _UpdateRowAddrDepTableForBufBlockedReq(unsigned int reqSlotTag)
{
	SSD_REQ_FORMAT* p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	if(REQ_OPT_NAND_ADDR_VSA != p_reqEntry->reqOpt.nandAddr)
	{
		assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");
	}

	unsigned int dieNo, chNo, wayNo, blockNo, pageNo, bufDepCheckReport;

	dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
	chNo =  Vdie2PchTranslation(dieNo);
	wayNo = Vdie2PwayTranslation(dieNo);
	blockNo = Vsa2VblockTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
	pageNo = Vsa2VpageTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);

	if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
	{
		if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag)
		{
			RequestScheduler_SyncReleaseEraseReq(chNo, wayNo, blockNo);

			bufDepCheckReport = _CheckBufDep(reqSlotTag);
			if(bufDepCheckReport == BUF_DEPENDENCY_REPORT_PASS)
			{
				if(pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
				{
					RequestAllocation_MoveToNandReqQ(reqSlotTag, chNo, wayNo);
				}
				else
				{
					rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
					RequestAllocation_MoveToBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				}

				return ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_SYNC;
			}
		}
		rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_ERASE)
	{
		rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 1;
	}

	return ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_DONE;
}

void InitDependencyTable()
{
	unsigned int blockNo, wayNo, chNo;
	rowAddrDependencyTablePtr = (P_ROW_ADDR_DEPENDENCY_TABLE)ROW_ADDR_DEPENDENCY_TABLE_ADDR;

	int tmp =10000, cnt = 0;
	for(blockNo=0 ; blockNo<MAIN_BLOCKS_PER_DIE ; blockNo++)
	{
		for(wayNo=0 ; wayNo<USER_WAYS ; wayNo++)
		{
			for(chNo=0 ; chNo<USER_CHANNELS ; chNo++)
			{
				if (cnt % tmp == 0)
					xil_printf("%s: bno: %u\/%u wno: %u\/%u chno: %u\/%u \n", __func__,
						blockNo, MAIN_BLOCKS_PER_DIE,
						wayNo, USER_WAYS, 
						chNo, USER_CHANNELS);
				cnt ++;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 0;
			}
		}
	}
}



void FlushWriteDataToNand(void)
{
	RequestScheduler_SyncAllLowLevelReqDone();

	unsigned int reqSlotTag;
	unsigned int virtualSliceAddr;

	for(unsigned int entryIteration = 0; entryIteration < AVAILABLE_DATA_BUFFER_ENTRY_COUNT; entryIteration++)
	{
		if(dataBufMapPtr->dataBuf[entryIteration].dirty == DATA_BUF_DIRTY)
		{
			// Put to the Tail of LRU list
			if(entryIteration != dataBufLruList.tailEntry)
			{
				if(dataBufMapPtr->dataBuf[entryIteration].prevEntry != DATA_BUF_IDX_INVALID)
				{
					dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[entryIteration].prevEntry].nextEntry = dataBufMapPtr->dataBuf[entryIteration].nextEntry;
				}

				if(dataBufMapPtr->dataBuf[entryIteration].nextEntry != DATA_BUF_IDX_INVALID)
				{
					dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[entryIteration].nextEntry].prevEntry = dataBufMapPtr->dataBuf[entryIteration].prevEntry;
				}

				dataBufMapPtr->dataBuf[dataBufLruList.tailEntry].nextEntry = entryIteration;
				dataBufMapPtr->dataBuf[entryIteration].prevEntry = dataBufLruList.tailEntry;
				dataBufLruList.tailEntry = entryIteration;
			}
#if (SUPPORT_BARRIER_FTL == 1)
			//Debug
			uint32_t sid1 = dataBufMapPtr->dataBuf[entryIteration].stream_id_1;
			uint32_t sid2 = dataBufMapPtr->dataBuf[entryIteration].stream_id_2;
			uint32_t eid1 = dataBufMapPtr->dataBuf[entryIteration].epoch_id_1;
			uint32_t eid2 = dataBufMapPtr->dataBuf[entryIteration].epoch_id_2;

			if (sid1 > 0) { /* Check the stream id is valid. */
				//uint32_t eid1 = dataBufMapPtr->dataBuf[entryIteration].epoch_id_1;
				uint32_t mappable_1 = dataBufMapPtr->dataBuf[entryIteration].mappable_1;
				uint32_t mappable_2 = dataBufMapPtr->dataBuf[entryIteration].mappable_2;
				//xil_printf("\r\n [ Flush Page ] Buf_index %u SID %u EID %u\r\n", entryIteration,
				//		dataBufMapPtr->dataBuf[entryIteration].stream_id_1, 
				//		dataBufMapPtr->dataBuf[entryIteration].epoch_id_1);

				/* Need to implement & Debug */
					
				if ((mappable_1 || is_mappable(sid1, eid1)) 
					&& (mappable_2 || is_mappable(sid2, eid2))) {  // This case, slice can be mappable in both stream1 and stream2.
					virtualSliceAddr = AddrTransWrite(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
					//xil_printf("%s bef update epoch info 1\n", __func__);
					update_epoch_info(sid1, eid1, __func__);
					//xil_printf("%s bef update epoch info 2\n", __func__);
					update_epoch_info(sid2, eid2, __func__);
					//xil_printf("%s: [Mapped ] Buf_index %u SID1 %u EID1 %u SID2: %u EID2: %u \n",
					//	__func__,  entryIteration,
					//	dataBufMapPtr->dataBuf[entryIteration].stream_id_1, 
					//	dataBufMapPtr->dataBuf[entryIteration].epoch_id_1,
					//	dataBufMapPtr->dataBuf[entryIteration].stream_id_2, 
					//	dataBufMapPtr->dataBuf[entryIteration].epoch_id_2);
				} else if ( (mappable_1 || is_mappable(sid1, eid1)) 
					&& !(mappable_2 || is_mappable(sid2, eid2))) { 	// Case 2: Suspend mapping update (Stream 2)
					virtualSliceAddr = AddrTransSuspen(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
					//xil_printf("AHHHHH 4\n");
					barrier_insert_suspension_array_dual_stream(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 2);	
					//xil_printf("AHHHHH 4-1\n");
#endif
				} else if ( !(mappable_1 || is_mappable(sid1, eid1)) 
					&& (mappable_2 || is_mappable(sid2, eid2))) { // Case 1	: Suspend mapping update (Stream 1
					virtualSliceAddr = AddrTransSuspen(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
					//xil_printf("AHHHHH 5\n");
					barrier_insert_suspension_array_dual_stream(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 1);	
					//xil_printf("AHHHHH 5-1\n");
#endif
				} else {	//Case 0: Suspend both mapping update
					virtualSliceAddr = AddrTransSuspen(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
#if (SUPPORT_INSERT_SUSPENSION_LIST == 1)
					//xil_printf("AHHHHH 6\n");
					barrier_insert_suspension_array_dual_stream(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr, virtualSliceAddr, sid1, eid1, sid2, eid2, 0);	
					//xil_printf("AHHHHH 6-1\n");
#endif
				}
			} else {
				virtualSliceAddr = AddrTransWrite(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
			}
#else
			virtualSliceAddr = AddrTransWrite(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);
#endif

			//virtualSliceAddr = AddrTransWrite(dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr);

			// Update Hash List
			BufferManagement_SelectiveGetFromHashList(entryIteration, 0);

			// Juwon Added
			DATA_BUF_ENTRY* p_dataBufEntry_ = BufferManagement_GetDataBufEntry(entryIteration);
			p_dataBufEntry_->hashPrevEntry = DATA_BUF_IDX_INVALID;
			p_dataBufEntry_->hashNextEntry = DATA_BUF_IDX_INVALID;

			// Generate NandRequest
			reqSlotTag = RequestAllocation_GetFreeReqEntry();
			reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
			//reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = cmdSlotTag; // SP: need to check correct setting always
			reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = dataBufMapPtr->dataBuf[entryIteration].logicalSliceAddr;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
			reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = entryIteration;
#if (SUPPORT_BARRIER_FTL == 1)
			reqPoolPtr->reqPool[reqSlotTag].stream_id_1 = dataBufMapPtr->dataBuf[entryIteration].stream_id_1;
			reqPoolPtr->reqPool[reqSlotTag].stream_id_2 = dataBufMapPtr->dataBuf[entryIteration].stream_id_2;
			reqPoolPtr->reqPool[reqSlotTag].epoch_id_1 = dataBufMapPtr->dataBuf[entryIteration].epoch_id_1;
			reqPoolPtr->reqPool[reqSlotTag].epoch_id_2 = dataBufMapPtr->dataBuf[entryIteration].epoch_id_2;
			reqPoolPtr->reqPool[reqSlotTag].mappable_1 = dataBufMapPtr->dataBuf[entryIteration].mappable_1;
			reqPoolPtr->reqPool[reqSlotTag].mappable_2 = dataBufMapPtr->dataBuf[entryIteration].mappable_2;
#endif
			BufferManagement_UpdateBufEntryInfoBlockingReq(entryIteration, reqSlotTag);
			reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr; // Fix


			SelectLowLevelReqQ(reqSlotTag);

			dataBufMapPtr->dataBuf[entryIteration].dirty = DATA_BUF_CLEAN;

			// Jieun add
#if (SUPPORT_BARRIER_FTL == 1)
			dataBufMapPtr->dataBuf[entryIteration].stream_id_1 = 0;
			dataBufMapPtr->dataBuf[entryIteration].stream_id_2 = 0;
			dataBufMapPtr->dataBuf[entryIteration].epoch_id_1 = 0;
			dataBufMapPtr->dataBuf[entryIteration].epoch_id_2 = 0;
			dataBufMapPtr->dataBuf[entryIteration].mappable_1 = 0;
			dataBufMapPtr->dataBuf[entryIteration].mappable_2 = 0;
#endif
		}
	}

	RequestScheduler_SyncAllLowLevelReqDone();
}


void SelectLowLevelReqQ(unsigned int reqSlotTag)
{
	unsigned int dieNo, chNo, wayNo, bufDepCheckReport, rowAddrDepCheckReport, rowAddrDepTableUpdateReport;

	bufDepCheckReport = _CheckBufDep(reqSlotTag);

	if (BUF_DEPENDENCY_REPORT_PASS == bufDepCheckReport)
	{
		if (REQ_TYPE_NVME_DMA == reqPoolPtr->reqPool[reqSlotTag].reqType)
		{
			hal_host_issue_hdma_req(reqSlotTag);
			RequestAllocation_MoveToNvmeDmaReqQ(reqSlotTag);
		}
		else if (REQ_TYPE_NAND == reqPoolPtr->reqPool[reqSlotTag].reqType)
		{
			if (REQ_OPT_NAND_ADDR_VSA == reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr)
			{
				dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
				chNo =  Vdie2PchTranslation(dieNo);
				wayNo = Vdie2PwayTranslation(dieNo);
			}
			else if (REQ_OPT_NAND_ADDR_PHY_ORG == reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr)
			{
				chNo =  reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalCh;
				wayNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalWay;
			}
			else
			{
				assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");
			}

			if (REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK == reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck)
			{
				rowAddrDepCheckReport = _CheckRowAddrDep(reqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT);

				if(ROW_ADDR_DEPENDENCY_REPORT_PASS == rowAddrDepCheckReport)
				{
					RequestAllocation_MoveToNandReqQ(reqSlotTag, chNo, wayNo);
				}
				else if (ROW_ADDR_DEPENDENCY_REPORT_BLOCKED == rowAddrDepCheckReport)
				{
					RequestAllocation_MoveToBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				}
				else
				{
					assert(!"[WARNING] Not supported report [WARNING]");
				}
			}
			else if (REQ_OPT_ROW_ADDR_DEPENDENCY_NONE == reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck)
			{
				RequestAllocation_MoveToNandReqQ(reqSlotTag, chNo, wayNo);
			}
			else
			{
				assert(!"[WARNING] Not supported reqOpt [WARNING]");
			}

		}
		else
		{
			assert(!"[WARNING] Not supported reqType [WARNING]");
		}
	}
	else if (BUF_DEPENDENCY_REPORT_BLOCKED == bufDepCheckReport)
	{
		if (REQ_TYPE_NAND == reqPoolPtr->reqPool[reqSlotTag].reqType)
		{
			if (REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK == reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck)
			{
				rowAddrDepTableUpdateReport = _UpdateRowAddrDepTableForBufBlockedReq(reqSlotTag);

				if (ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_DONE == rowAddrDepTableUpdateReport)
				{
					//pass, go to PutToBlockedByBufDepReqQ
				}
				else if(ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_SYNC == rowAddrDepTableUpdateReport)
				{
					return;
				}
				else
				{
					assert(!"[WARNING] Not supported report [WARNING]");
				}
			}
		}

		RequestAllocation_MoveToBlockedByBufDepReqQ(reqSlotTag);
	}
	else
	{
		assert(!"[WARNING] Not supported report [WARNING]");
	}
}


void ReleaseBlockedByBufDepReq(unsigned int reqSlotTag)
{
	unsigned int targetReqSlotTag, dieNo, chNo, wayNo, rowAddrDepCheckReport;

	targetReqSlotTag = REQ_SLOT_TAG_NONE;

	if (REQ_SLOT_TAG_NONE != reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq)
	{
		targetReqSlotTag = reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq;

		reqPoolPtr->reqPool[targetReqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq = REQ_SLOT_TAG_NONE;

		//xil_printf("release bbd q (tag:0x%X), next 0x%X\n", reqSlotTag, targetReqSlotTag);
	}

	if (REQ_OPT_DATA_BUF_ENTRY == reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat)
	{
		if (reqSlotTag == dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail)
		{


#if (SUPPORT_BARRIER_FTL == 1)
	// Jieun add
if (REQ_TYPE_NAND == reqPoolPtr->reqPool[targetReqSlotTag].reqType) {
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].stream_id_1 = 0;
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].stream_id_2 = 0;
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].epoch_id_1 = 0;
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].epoch_id_2 = 0;
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].mappable_1 = 0;
	dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].mappable_2 = 0;
}
#endif


			/*
		#if (SUPPORT_BARRIER_FTL == 1)
			if (REQ_TYPE_NAND == reqPoolPtr->reqPool[reqSlotTag].reqType)
			{
				unsigned int sid, eid;
				sid = dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].stream_id;
				eid = dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].epoch_id;
				if (INVALID_STREAM_ID != sid)
				{
					barrier_increase_durable_count(sid, eid);
					dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].stream_id = INVALID_STREAM_ID;
					dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].epoch_id = INVALID_EPOCH_ID;
				}
			}
		#endif
		*/
			dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail = REQ_SLOT_TAG_NONE;
		}
	}
	else if (REQ_OPT_DATA_BUF_TEMP_ENTRY == reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat)
	{
		if(reqSlotTag == tempDataBufMapPtr->tempDataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail)
		{
			tempDataBufMapPtr->tempDataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail = REQ_SLOT_TAG_NONE;
		}
	}

	if((REQ_SLOT_TAG_NONE != targetReqSlotTag)
			&& (REQ_QUEUE_TYPE_BLOCKED_BY_BUF_DEP == reqPoolPtr->reqPool[targetReqSlotTag].reqQueueType))
	{
		RequestAllocation_SelectiveGetFromBlockedByBufDepReqQ(targetReqSlotTag);

		if (REQ_TYPE_NVME_DMA == reqPoolPtr->reqPool[targetReqSlotTag].reqType)
		{
			//xil_printf("issue nvme req (tag:0x%X)\n", targetReqSlotTag);
			hal_host_issue_hdma_req(targetReqSlotTag);
			RequestAllocation_MoveToNvmeDmaReqQ(targetReqSlotTag);
		}
		else if (REQ_TYPE_NAND == reqPoolPtr->reqPool[targetReqSlotTag].reqType)
		{
			if (REQ_OPT_NAND_ADDR_VSA == reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.nandAddr)
			{
				dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[targetReqSlotTag].nandInfo.virtualSliceAddr);
				chNo =  Vdie2PchTranslation(dieNo);
				wayNo = Vdie2PwayTranslation(dieNo);
			}
			else
			{
				assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");
			}

			if (REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK == reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.rowAddrDependencyCheck)
			{
				rowAddrDepCheckReport = _CheckRowAddrDep(targetReqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE);

				if (ROW_ADDR_DEPENDENCY_REPORT_PASS == rowAddrDepCheckReport)
				{
					RequestAllocation_MoveToNandReqQ(targetReqSlotTag, chNo, wayNo);
				}
				else if (ROW_ADDR_DEPENDENCY_REPORT_BLOCKED == rowAddrDepCheckReport)
				{
					RequestAllocation_MoveToBlockedByRowAddrDepReqQ(targetReqSlotTag, chNo, wayNo);
				}
				else
				{
					assert(!"[WARNING] Not supported report [WARNING]");
				}
			}
			else if (REQ_OPT_ROW_ADDR_DEPENDENCY_NONE == reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.rowAddrDependencyCheck)
			{
				RequestAllocation_MoveToNandReqQ(targetReqSlotTag, chNo, wayNo);
			}
			else
			{
				assert(!"[WARNING] Not supported reqOpt [WARNING]");
			}
		}
	}
}


void ReleaseBlockedByRowAddrDepReq(unsigned int chNo, unsigned int wayNo)
{
	unsigned int reqSlotTag, nextReq, rowAddrDepCheckReport;

	reqSlotTag = RequestAllocation_PeekReqEntryFromBlockedByRowAddrDepReqQ(chNo, wayNo);//blockedByRowAddrDepReqQ[chNo][wayNo].headReq;

	while (REQ_SLOT_TAG_NONE != reqSlotTag)
	{
		nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

		if(REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK == reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck)
		{
			rowAddrDepCheckReport = _CheckRowAddrDep(reqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE);

			if(ROW_ADDR_DEPENDENCY_REPORT_PASS == rowAddrDepCheckReport)
			{
				RequestAllocation_SelectiveGetFromBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				RequestAllocation_MoveToNandReqQ(reqSlotTag, chNo, wayNo);
			}
			else if(ROW_ADDR_DEPENDENCY_REPORT_BLOCKED == rowAddrDepCheckReport)
			{
				//pass, go to while loop
			}
			else
			{
				assert(!"[WARNING] Not supported report [WARNING]");
			}
		}
		else
		{
			assert(!"[WARNING] Not supported reqOpt [WARNING]");
		}

		reqSlotTag = nextReq;
	}
}

