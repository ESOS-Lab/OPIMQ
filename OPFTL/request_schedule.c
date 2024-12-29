//////////////////////////////////////////////////////////////////////////////////
// request_schedule.c for Cosmos+ OpenSSD
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
// File Name: request_schedule.c
//
// Version: v1.0.0
//
// Description:
//	 - decide request execution sequence
//   - issue NAND request to NAND storage controller
//   - check request execution result
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include "xil_printf.h"
#include "memory_map.h"
#include "debug.h"
#include "hal/hal_host.h"

P_COMPLETE_FLAG_TABLE completeFlagTablePtr;
P_STATUS_REPORT_TABLE statusReportTablePtr;
P_ERROR_INFO_TABLE eccErrorInfoTablePtr;
P_RETRY_LIMIT_TABLE retryLimitTablePtr;

P_DIE_STATE_TABLE dieStateTablePtr;
P_WAY_PRIORITY_TABLE wayPriorityTablePtr;


static unsigned int _CheckEccErrorInfo(unsigned int chNo, unsigned int wayNo)
{
	unsigned int errorInfo0, reqSlotTag; // errorInfo1

	reqSlotTag = RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo);//nandReqQ[chNo][wayNo].headReq;

	errorInfo0 = eccErrorInfoTablePtr->errorInfo[chNo][wayNo][0];
	//errorInfo1 = eccErrorInfoTablePtr->errorInfo[chNo][wayNo][1];

	if (V2FCrcValid(eccErrorInfoTablePtr->errorInfo[chNo][wayNo]))
	//if (V2FPageDecodeSuccess(&eccErrorInfoTablePtr->errorInfo[chNo][wayNo][1]))
	{
		if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning == REQ_OPT_NAND_ECC_WARNING_ON)
			if(V2FWorstChunkErrorCount(&errorInfo0)> BIT_ERROR_THRESHOLD_PER_CHUNK)
				return ERROR_INFO_WARNING;

		return ERROR_INFO_PASS;
	}

	return ERROR_INFO_FAIL;
}

static unsigned int _CheckReqStatus(unsigned int chNo, unsigned int wayNo)
{
	unsigned int reqSlotTag, completeFlag, statusReport, errorInfo, readyBusy, status;
	unsigned int* statusReportPtr;

	reqSlotTag = RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo);//nandReqQ[chNo][wayNo].headReq;

	if(dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt == REQ_STATUS_CHECK_OPT_COMPLETION_FLAG)
	{
		completeFlag = completeFlagTablePtr->completeFlag[chNo][wayNo];

		if(V2FTransferComplete(completeFlag))
		{
			if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc == REQ_OPT_NAND_ECC_ON)
			{
				errorInfo = _CheckEccErrorInfo(chNo, wayNo);

				if (errorInfo == ERROR_INFO_WARNING)
					return REQ_STATUS_WARNING;
				else if (errorInfo == ERROR_INFO_FAIL){
					//xil_printf("%s: fail 1 !\n", __func__);
					return REQ_STATUS_FAIL;
				}
			}
			return REQ_STATUS_DONE;
		}
	}
	else if(dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt == REQ_STATUS_CHECK_OPT_CHECK)
	{
		statusReportPtr = (unsigned int*)(&statusReportTablePtr->statusReport[chNo][wayNo]);

		V2FStatusCheckAsync(&chCtlReg[chNo], wayNo, statusReportPtr);

		dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_REPORT;
	}
	else if(dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt == REQ_STATUS_CHECK_OPT_REPORT)
	{
		statusReport = statusReportTablePtr->statusReport[chNo][wayNo];

		if(V2FRequestReportDone(statusReport))
		{
			status = V2FEliminateReportDoneFlag(statusReport);
			if(V2FRequestComplete(status))
			{
				if (V2FRequestFail(status)){
					//xil_printf("%s: fail 2 !\n", __func__);
					return REQ_STATUS_FAIL;
				}

				dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_NONE;
				return REQ_STATUS_DONE;
			}
			else
				dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_CHECK;
		}
	}
	else if(dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt == REQ_STATUS_CHECK_OPT_NONE)
	{
		readyBusy = V2FReadyBusyAsync(&chCtlReg[chNo]);

		if(V2FWayReady(readyBusy, wayNo))
			return REQ_STATUS_DONE;
	}
	else
		assert(!"[WARNING] wrong request status check option [WARNING]");

	return REQ_STATUS_RUNNING;
}

static unsigned int _GenerateNandRowAddr(unsigned int reqSlotTag)
{
	unsigned int rowAddr, lun, virtualBlockNo, tempBlockNo, phyBlockNo, tempPageNo, dieNo;

	if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_VSA)
	{
		dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		virtualBlockNo = Vsa2VblockTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		phyBlockNo = Vblock2PblockOfTbsTranslation(virtualBlockNo);
		lun =  phyBlockNo / TOTAL_BLOCKS_PER_LUN;
		tempBlockNo = phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].remappedPhyBlock % TOTAL_BLOCKS_PER_LUN;
		tempPageNo = Vsa2VpageTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);

		//if(BITS_PER_FLASH_CELL == SLC_MODE)
		//	tempPageNo = Vpage2PlsbPageTranslation(tempPageNo);
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_PHY_ORG)
	{
		dieNo = Pcw2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalCh, reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalWay);
		if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace == REQ_OPT_BLOCK_SPACE_TOTAL)
		{
			lun =  reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock / TOTAL_BLOCKS_PER_LUN;

			tempBlockNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock % TOTAL_BLOCKS_PER_LUN;
			tempPageNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalPage;
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace == REQ_OPT_BLOCK_SPACE_MAIN)
		{
			lun =  reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock / MAIN_BLOCKS_PER_LUN;
			tempBlockNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalBlock % MAIN_BLOCKS_PER_LUN + lun * TOTAL_BLOCKS_PER_LUN;

			//phyBlock remap
			tempBlockNo = phyBlockMapPtr->phyBlock[dieNo][tempBlockNo].remappedPhyBlock % TOTAL_BLOCKS_PER_LUN;
			tempPageNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalPage;
		}
	}
	else
	{
		assert(!"[WARNING] wrong nand addr option [WARNING]");
	}

	if(lun == 0)
	{
		rowAddr = LUN_0_BASE_ADDR + tempBlockNo * PAGES_PER_MLC_BLOCK + tempPageNo;
	}
	else
	{
		rowAddr = LUN_1_BASE_ADDR + tempBlockNo * PAGES_PER_MLC_BLOCK + tempPageNo;
	}

	return rowAddr;
}

static void _PutToNandReadTriggerList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].readTriggerTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].readTriggerTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].readTriggerTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerTail = wayNo;
	}
}

static void _PutToNandWriteList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].writeTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].writeTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].writeTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].writeTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].writeTail = wayNo;
	}
}

static void _PutToNandReadTransferList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].readTransferTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].readTransferTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].readTransferTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].readTransferTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].readTransferTail = wayNo;
	}
}

static void _PutToNandEraseList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].eraseTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].eraseTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].eraseTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].eraseTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].eraseTail = wayNo;
	}
}

static void _PutToNandWayPriorityTable(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
	{
		_PutToNandReadTriggerList(chNo, wayNo);
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ_TRANSFER)
	{
		_PutToNandReadTransferList(chNo, wayNo);
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_WRITE)
	{
		_PutToNandWriteList(chNo, wayNo);
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_ERASE)
	{
		_PutToNandEraseList(chNo, wayNo);
	}
	else if((reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_RESET)|| (reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_SET_FEATURE))
	{
		_PutToNandWriteList(chNo, wayNo);
	}
	else
	{
		assert(!"[WARNING] wrong reqCode [WARNING]");
	}
}

static void _PutToNandIdleList(unsigned int chNo, unsigned int wayNo)
{
	ASSERT(USER_WAYS > wayNo);

	if(wayPriorityTablePtr->wayPriority[chNo].idleTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].idleTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].idleTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].idleTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].idleHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].idleTail = wayNo;
	}
}

static void _PutToNandStatusCheckList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].statusCheckTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].statusCheckTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].statusCheckTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckTail = wayNo;
	}
}

static void _SelectivGetFromNandIdleList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].idleTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].idleHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;

		ASSERT(USER_WAYS > wayPriorityTablePtr->wayPriority[chNo].idleHead);
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].idleHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].idleTail = WAY_NONE;
	}
}

static void _PutToNandStatusReportList(unsigned int chNo, unsigned int wayNo)
{
	if(wayPriorityTablePtr->wayPriority[chNo].statusReportTail != WAY_NONE)
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayPriorityTablePtr->wayPriority[chNo].statusReportTail;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayPriorityTablePtr->wayPriority[chNo].statusReportTail].nextWay = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].statusReportTail = wayNo;
	}
	else
	{
		dieStateTablePtr->dieState[chNo][wayNo].prevWay =  WAY_NONE;
		dieStateTablePtr->dieState[chNo][wayNo].nextWay =  WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusReportHead = wayNo;
		wayPriorityTablePtr->wayPriority[chNo].statusReportTail = wayNo;
	}
}

static void _SelectivGetFromNandStatusReportList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusReportTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusReportHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].statusReportHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusReportTail = WAY_NONE;
	}
}

static void _SelectiveGetFromNandReadTriggerList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].readTriggerHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerTail = WAY_NONE;
	}
}

static void _SelectiveGetFromNandWriteList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].writeHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeTail = WAY_NONE;
	}
}

static void _SelectiveGetFromNandReadTransferList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].readTransferHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferTail = WAY_NONE;
	}
}

static void _SelectiveGetFromNandEraseList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].eraseHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseTail = WAY_NONE;
	}
}

static void _SelectiveGetFromNandStatusCheckList(unsigned int chNo, unsigned int wayNo)
{
	if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay == WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay != WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].prevWay].nextWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckTail = dieStateTablePtr->dieState[chNo][wayNo].prevWay;
	}
	else if((dieStateTablePtr->dieState[chNo][wayNo].nextWay != WAY_NONE) && (dieStateTablePtr->dieState[chNo][wayNo].prevWay == WAY_NONE))
	{
		dieStateTablePtr->dieState[chNo][dieStateTablePtr->dieState[chNo][wayNo].nextWay].prevWay = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckHead = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
	}
	else
	{
		wayPriorityTablePtr->wayPriority[chNo].statusCheckHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckTail = WAY_NONE;
	}

}

static void _IssueNandReq(unsigned int chNo, unsigned int wayNo)
{
	unsigned int reqSlotTag, rowAddr;
	void* dataBufAddr;
	void* spareDataBufAddr;
	unsigned int* errorInfo;
	unsigned int* completion;

	reqSlotTag  = RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo);//nandReqQ[chNo][wayNo].headReq;
	rowAddr = _GenerateNandRowAddr(reqSlotTag);
	dataBufAddr = (void*)BufferManagement_GetDataBufAddr(reqSlotTag);
	spareDataBufAddr = (void*)BufferManagement_GetSpareDataBufAddr(reqSlotTag);

	switch(reqPoolPtr->reqPool[reqSlotTag].reqCode)
	{
		case REQ_CODE_READ:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_CHECK;

			V2FReadPageTriggerAsync(&chCtlReg[chNo], wayNo, rowAddr);
			break;
		case REQ_CODE_READ_TRANSFER:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_COMPLETION_FLAG;

			errorInfo = (unsigned int*)(&eccErrorInfoTablePtr->errorInfo[chNo][wayNo]);
			completion = (unsigned int*)(&completeFlagTablePtr->completeFlag[chNo][wayNo]);

			if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc == REQ_OPT_NAND_ECC_ON)
			{
				V2FReadPageTransferAsync(&chCtlReg[chNo], wayNo, dataBufAddr, spareDataBufAddr, errorInfo, completion, rowAddr);
			}
			else
			{
				V2FReadPageTransferRawAsync(&chCtlReg[chNo], wayNo, dataBufAddr, completion);
			}
			break;
		case REQ_CODE_WRITE:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_CHECK;

			V2FProgramPageAsync(&chCtlReg[chNo], wayNo, rowAddr, dataBufAddr, spareDataBufAddr);
			break;
		case REQ_CODE_ERASE:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_CHECK;

			V2FEraseBlockAsync(&chCtlReg[chNo], wayNo, rowAddr);
			break;
		case REQ_CODE_RESET:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_NONE;

			V2FResetSync(&chCtlReg[chNo], wayNo);
			break;
		case REQ_CODE_SET_FEATURE:
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_NONE;

			V2FEnterToggleMode(&chCtlReg[chNo], wayNo, TEMPORARY_PAY_LOAD_ADDR);
			break;
		default:
			assert(!"[WARNING] not defined nand req [WARNING]");
			break;
	}
}

static void _ExecuteNandReq(unsigned int chNo, unsigned int wayNo, unsigned int reqStatus)
{
	unsigned int reqSlotTag, rowAddr, phyBlockNo;
	unsigned char* badCheck ;

	reqSlotTag = RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo);//nandReqQ[chNo][wayNo].headReq;

	switch(dieStateTablePtr->dieState[chNo][wayNo].dieState)
	{
		case DIE_STATE_IDLE:
			_IssueNandReq(chNo, wayNo);
			dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_EXE;
			break;
		case DIE_STATE_EXE:
			if(REQ_STATUS_DONE == reqStatus)
			{
				if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
				{
					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ_TRANSFER;
				}
				else
				{
					retryLimitTablePtr->retryLimit[chNo][wayNo] = RETRY_LIMIT;

					RequestAllocation_GetEntryFromNandReqQ(chNo, wayNo, reqPoolPtr->reqPool[reqSlotTag].reqCode);
				}

				dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_IDLE;
			}
			else if(REQ_STATUS_FAIL == reqStatus)
			{
				if((REQ_CODE_READ == reqPoolPtr->reqPool[reqSlotTag].reqCode)
						|| (REQ_CODE_READ_TRANSFER == reqPoolPtr->reqPool[reqSlotTag].reqCode))
				{
					if(retryLimitTablePtr->retryLimit[chNo][wayNo] > 0)
					{
						retryLimitTablePtr->retryLimit[chNo][wayNo]--;

						if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ_TRANSFER)
						{
							reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
						}

						dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_IDLE;
						return;
					}
				}

				if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
				{
					xil_printf("Read Trigger FAIL on      ");
				}
				else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ_TRANSFER)
				{
					xil_printf("Read Transfer FAIL on     ");
				}
				else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_WRITE)
				{
					xil_printf("Write FAIL on             ");
				}
				else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_ERASE)
				{
					xil_printf("Erase FAIL on             ");
				}

				rowAddr = _GenerateNandRowAddr(reqSlotTag);

				xil_printf("ch %x way %x rowAddr %x / completion %x statusReport %x \r\n", chNo, wayNo, rowAddr, completeFlagTablePtr->completeFlag[chNo][wayNo],statusReportTablePtr->statusReport[chNo][wayNo]);

				if (REQ_OPT_NAND_ECC_OFF == reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc)
				{
					if (REQ_OPT_DATA_BUF_ADDR == reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat)
					{
						//Request fail in the bad block detection process
						badCheck = (unsigned char*)reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.addr;
						*badCheck = PSEUDO_BAD_BLOCK_MARK;
					}
				}

				//grown bad block information update
				phyBlockNo = ((rowAddr % LUN_1_BASE_ADDR) / PAGES_PER_MLC_BLOCK) + ((rowAddr / LUN_1_BASE_ADDR)* TOTAL_BLOCKS_PER_LUN);

				UpdatePhyBlockMapForGrownBadBlock(Pcw2VdieTranslation(chNo, wayNo), phyBlockNo);

				retryLimitTablePtr->retryLimit[chNo][wayNo] = RETRY_LIMIT;

				RequestAllocation_GetEntryFromNandReqQ(chNo, wayNo, reqPoolPtr->reqPool[reqSlotTag].reqCode);

				dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_IDLE;
			}
			else if(REQ_STATUS_WARNING == reqStatus)
			{
				rowAddr = _GenerateNandRowAddr(reqSlotTag);
				xil_printf("ECC Uncorrectable Soon on ch %x way %x rowAddr %x / completion %x statusReport %x \r\n", chNo, wayNo, rowAddr, completeFlagTablePtr->completeFlag[chNo][wayNo],statusReportTablePtr->statusReport[chNo][wayNo]);

				//grown bad block information update
				phyBlockNo = ((rowAddr % LUN_1_BASE_ADDR) / PAGES_PER_MLC_BLOCK) + ((rowAddr / LUN_1_BASE_ADDR)* TOTAL_BLOCKS_PER_LUN);
				UpdatePhyBlockMapForGrownBadBlock(Pcw2VdieTranslation(chNo, wayNo), phyBlockNo);

				retryLimitTablePtr->retryLimit[chNo][wayNo] = RETRY_LIMIT;

				RequestAllocation_GetEntryFromNandReqQ(chNo, wayNo, reqPoolPtr->reqPool[reqSlotTag].reqCode);

				dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_IDLE;
			}
			else if(reqStatus == REQ_STATUS_RUNNING)
			{
				break;
			}
			else
			{
				assert(!"[WARNING] wrong req status [WARNING]");
			}
			break;
	}
}

static void _SchedulingNandReqPerCh(unsigned int chNo)
{
	unsigned int readyBusy, wayNo, reqStatus, nextWay, waitWayCnt;

	waitWayCnt = 0;

	if (WAY_NONE != wayPriorityTablePtr->wayPriority[chNo].idleHead)
	{
		wayNo = wayPriorityTablePtr->wayPriority[chNo].idleHead;

		while(wayNo != WAY_NONE)
		{
			if (0 == RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NAND,chNo,wayNo))//nandReqQ[chNo][wayNo].headReq == REQ_SLOT_TAG_NONE)
			{
				ReleaseBlockedByRowAddrDepReq(chNo, wayNo);
			}

			if (0 != RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NAND,chNo,wayNo))
			{
				nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;

				_SelectivGetFromNandIdleList(chNo, wayNo);
				_PutToNandWayPriorityTable(RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo), chNo, wayNo);
				wayNo = nextWay;
			}
			else
			{
				wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				
				waitWayCnt++;
			}
		}
	}

	if (WAY_NONE != wayPriorityTablePtr->wayPriority[chNo].statusReportHead)
	{
		readyBusy = V2FReadyBusyAsync(&chCtlReg[chNo]);
		wayNo = wayPriorityTablePtr->wayPriority[chNo].statusReportHead;

		while(wayNo != WAY_NONE)
		{
			if(V2FWayReady(readyBusy, wayNo))
			{
				reqStatus = _CheckReqStatus(chNo, wayNo);

				if(reqStatus != REQ_STATUS_RUNNING)
				{
					_ExecuteNandReq(chNo, wayNo, reqStatus);
					nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
					_SelectivGetFromNandStatusReportList(chNo, wayNo);

					if (0 == RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NAND, chNo, wayNo))//(nandReqQ[chNo][wayNo].headReq == REQ_SLOT_TAG_NONE)
					{
						ReleaseBlockedByRowAddrDepReq(chNo, wayNo);
					}

					if (0 != RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NAND, chNo, wayNo))//(nandReqQ[chNo][wayNo].headReq != REQ_SLOT_TAG_NONE)
					{
						_PutToNandWayPriorityTable(RequestAllocation_PeekReqEntryFromNandReqQ(chNo, wayNo), chNo, wayNo);
					}
					else
					{
						_PutToNandIdleList(chNo, wayNo);
						waitWayCnt++;
					}

					wayNo = nextWay;
				}
				else if(dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt  == REQ_STATUS_CHECK_OPT_CHECK)
				{
					nextWay = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
					_SelectivGetFromNandStatusReportList(chNo, wayNo);

					_PutToNandStatusCheckList(chNo, wayNo);
					wayNo = nextWay;
				}
				else
				{
					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
					waitWayCnt++;
				}
			}
			else
			{
				wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				waitWayCnt++;
			}
		}
	}

	if (waitWayCnt != USER_WAYS)
	{
		if(!V2FIsControllerBusy(&chCtlReg[chNo]))
		{
			if(wayPriorityTablePtr->wayPriority[chNo].statusCheckHead != WAY_NONE)
			{
				readyBusy = V2FReadyBusyAsync(&chCtlReg[chNo]);
				wayNo = wayPriorityTablePtr->wayPriority[chNo].statusCheckHead;

				while(wayNo != WAY_NONE)
				{
					if(V2FWayReady(readyBusy, wayNo))
					{
						reqStatus = _CheckReqStatus(chNo, wayNo);

						_SelectiveGetFromNandStatusCheckList(chNo,wayNo);
						_PutToNandStatusReportList(chNo, wayNo);

						if(V2FIsControllerBusy(&chCtlReg[chNo]))
						{
							return;
						}
					}

					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				}
			}

			if( wayPriorityTablePtr->wayPriority[chNo].readTriggerHead != WAY_NONE)
			{
				wayNo = wayPriorityTablePtr->wayPriority[chNo].readTriggerHead;

				while (wayNo != WAY_NONE)
				{
					_ExecuteNandReq(chNo, wayNo, REQ_STATUS_RUNNING);

					_SelectiveGetFromNandReadTriggerList(chNo, wayNo);
					_PutToNandStatusCheckList(chNo, wayNo);

					if(V2FIsControllerBusy(&chCtlReg[chNo]))
						return;

					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				}
			}

			if (wayPriorityTablePtr->wayPriority[chNo].eraseHead != WAY_NONE)
			{
				wayNo = wayPriorityTablePtr->wayPriority[chNo].eraseHead;


				while(wayNo != WAY_NONE)
				{
					_ExecuteNandReq(chNo, wayNo, REQ_STATUS_RUNNING);

					_SelectiveGetFromNandEraseList(chNo, wayNo);
					_PutToNandStatusCheckList(chNo, wayNo);

					if(V2FIsControllerBusy(&chCtlReg[chNo]))
					{
						return;
					}

					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				}
			}

			if(wayPriorityTablePtr->wayPriority[chNo].writeHead != WAY_NONE)
			{
				wayNo = wayPriorityTablePtr->wayPriority[chNo].writeHead;

				while(wayNo != WAY_NONE)
				{
					_ExecuteNandReq(chNo, wayNo, REQ_STATUS_RUNNING);

					_SelectiveGetFromNandWriteList(chNo, wayNo);
					_PutToNandStatusCheckList(chNo, wayNo);

					if(V2FIsControllerBusy(&chCtlReg[chNo]))
					{
						return;
					}

					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				}
			}

			if (wayPriorityTablePtr->wayPriority[chNo].readTransferHead != WAY_NONE)
			{
				wayNo = wayPriorityTablePtr->wayPriority[chNo].readTransferHead;

				while (wayNo != WAY_NONE)
				{
					_ExecuteNandReq(chNo, wayNo, REQ_STATUS_RUNNING);

					_SelectiveGetFromNandReadTransferList(chNo, wayNo);
					_PutToNandStatusReportList(chNo, wayNo);

					if(V2FIsControllerBusy(&chCtlReg[chNo]))
					{
						return;
					}

					wayNo = dieStateTablePtr->dieState[chNo][wayNo].nextWay;
				}
			}
		}
	}
}

void RequestScheduler_SyncReleaseEraseReq(unsigned int chNo, unsigned int wayNo, unsigned int blockNo)
{
	while(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag)
	{
		hal_host_handle_hdma_result();
		RequestScheduler_SchedulingNandReq();
	}
}

void RequestScheduler_SyncAllLowLevelReqDone()
{
	while((RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_NVME_DMA, 0, 0))
			|| notCompletedNandReqCnt
			|| blockedReqCnt)
	{
		hal_host_handle_hdma_result();
		RequestScheduler_SchedulingNandReq();
	}
}

void RequestScheduler_SyncAvailFreeReq()
{
	while(0 == RequestAllocation_GetReqPoolCount(REQ_QUEUE_TYPE_FREE, 0, 0))
	{
		hal_host_handle_hdma_result();
		RequestScheduler_SchedulingNandReq();
	}
}

void RequestScheduler_SchedulingNandReq()
{
	int chNo;

	for(chNo = 0; chNo < USER_CHANNELS; chNo++)
	{
		_SchedulingNandReqPerCh(chNo);
	}
}


void RequestScheduler_Init()
{
	int chNo,wayNo;

	completeFlagTablePtr = (P_COMPLETE_FLAG_TABLE) COMPLETE_FLAG_TABLE_ADDR;
	statusReportTablePtr = (P_STATUS_REPORT_TABLE) STATUS_REPORT_TABLE_ADDR;
	eccErrorInfoTablePtr = (P_ERROR_INFO_TABLE) ERROR_INFO_TABLE_ADDR;
	retryLimitTablePtr = (P_RETRY_LIMIT_TABLE) RETRY_LIMIT_TABLE_ADDR;

	dieStateTablePtr = (P_DIE_STATE_TABLE) DIE_STATE_TABLE_ADDR;
	wayPriorityTablePtr = (P_WAY_PRIORITY_TABLE) WAY_PRIORITY_TABLE_ADDR;

	for(chNo=0; chNo<USER_CHANNELS; ++chNo)
	{
		wayPriorityTablePtr->wayPriority[chNo].idleHead = 0;
		wayPriorityTablePtr->wayPriority[chNo].idleTail = USER_WAYS - 1;
		wayPriorityTablePtr->wayPriority[chNo].statusReportHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusReportTail = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].eraseTail = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTriggerTail = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].writeTail = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].readTransferTail = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckHead = WAY_NONE;
		wayPriorityTablePtr->wayPriority[chNo].statusCheckTail = WAY_NONE;

		for(wayNo=0; wayNo<USER_WAYS; ++wayNo)
		{
			dieStateTablePtr->dieState[chNo][wayNo].dieState = DIE_STATE_IDLE;
			dieStateTablePtr->dieState[chNo][wayNo].reqStatusCheckOpt = REQ_STATUS_CHECK_OPT_NONE;
			dieStateTablePtr->dieState[chNo][wayNo].prevWay = wayNo - 1;
			dieStateTablePtr->dieState[chNo][wayNo].nextWay = wayNo + 1;

			completeFlagTablePtr->completeFlag[chNo][wayNo] = 0;
			statusReportTablePtr->statusReport[chNo][wayNo] = 0;
			retryLimitTablePtr->retryLimit[chNo][wayNo] = RETRY_LIMIT;
		}
		dieStateTablePtr->dieState[chNo][0].prevWay = WAY_NONE;
		dieStateTablePtr->dieState[chNo][USER_WAYS-1].nextWay = WAY_NONE;
	}
}
