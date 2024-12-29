/*
 * lld_hdma.h
 *
 *  Created on: 2021. 8. 15.
 *      Author: Park
 */

#ifndef _LLD_HDMA_H_
#define _LLD_HDMA_H_

#define HOST_IP_ADDR						(XPAR_NVME_CTRL_0_BASEADDR)

#define HOST_DMA_FIFO_CNT_REG_ADDR			(HOST_IP_ADDR + 0x204)
#define HOST_DMA_CMD_FIFO_REG_ADDR			(HOST_IP_ADDR + 0x310)

#define HOST_DMA_DIRECT_TYPE				(1)
#define HOST_DMA_AUTO_TYPE					(0)

#define HOST_DMA_TX_DIRECTION				(1)
#define HOST_DMA_RX_DIRECTION				(0)



//offset: 0x00000204, size: 4
typedef struct _HOST_DMA_FIFO_CNT_REG
{
	union {
		unsigned int dword;
		struct
		{
			unsigned char directDmaRx;
			unsigned char directDmaTx;
			unsigned char autoDmaRx;
			unsigned char autoDmaTx;
		};
	};
} HOST_DMA_FIFO_CNT_REG;

//offset: 0x0000030C, size: 16
typedef struct _HOST_DMA_CMD_FIFO_REG
{
	union {
		unsigned int dword[5];//slot_modified
		struct
		{
			unsigned int devAddr;
			unsigned int pcieAddrH;
			unsigned int pcieAddrL;
			struct
			{
				unsigned int dmaLen				:13;
				unsigned int autoCompletion		:1;
				unsigned int cmd4KBOffset		:9;
				unsigned int reserved0			:7;//slot_modified
				unsigned int dmaDirection		:1;
				unsigned int dmaType			:1;
			};
			unsigned int cmdSlotTag;//slot_modified
		};
	};
} HOST_DMA_CMD_FIFO_REG;

typedef struct _HOST_DMA_STATUS
{
	HOST_DMA_FIFO_CNT_REG fifoHead;
	HOST_DMA_FIFO_CNT_REG fifoTail;
	unsigned int directDmaTxCnt;
	unsigned int directDmaRxCnt;
	unsigned int autoDmaTxCnt;
	unsigned int autoDmaRxCnt;
} HOST_DMA_STATUS;


typedef struct _HOST_DMA_ASSIST_STATUS
{
	unsigned int autoDmaTxOverFlowCnt;
	unsigned int autoDmaRxOverFlowCnt;
} HOST_DMA_ASSIST_STATUS;

//extern HOST_DMA_STATUS g_hostDmaStatus;
//extern HOST_DMA_ASSIST_STATUS g_hostDmaAssistStatus;


void lld_hdma_set_direct_tx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len);
void lld_hdma_set_direct_rx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len);
void lld_hdma_set_auto_tx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion);
void lld_hdma_set_auto_rx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion);
void lld_hdma_check_direct_tx_dma_done();
void lld_hdma_check_direct_rx_dma_done();
void lld_hdma_check_auto_tx_dma_done();
void lld_hdma_check_auto_rx_dma_done();
unsigned int lld_hdma_check_auto_tx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex);
unsigned int lld_hdma_check_auto_rx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex);
unsigned int lld_hdma_get_dma_tail_fifo_cnt(void);
unsigned int lld_hdma_get_auto_dma_tx_overflow_cnt(void);
unsigned int lld_hdma_get_auto_dma_rx_overflow_cnt(void);

#endif /* _LLD_HDMA_H_ */
