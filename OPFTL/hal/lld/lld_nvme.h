/*
 * lld_nvme.h
 *
 *  Created on: 2021. 8. 15.
 *      Author: Park
 */

#ifndef _LLD_NVME_H_
#define _LLD_NVME_H_

#define HOST_IP_ADDR						(XPAR_NVME_CTRL_0_BASEADDR)

#define NVME_STATUS_REG_ADDR				(HOST_IP_ADDR + 0x200)
#define NVME_ADMIN_QUEUE_SET_REG_ADDR		(HOST_IP_ADDR + 0x21C)
#define NVME_IO_SQ_SET_REG_ADDR				(HOST_IP_ADDR + 0x220)
#define NVME_IO_CQ_SET_REG_ADDR				(HOST_IP_ADDR + 0x260)

#define NVME_CMD_FIFO_REG_ADDR				(HOST_IP_ADDR + 0x300)
#define NVME_CPL_FIFO_REG_ADDR				(HOST_IP_ADDR + 0x304)

#define NVME_CMD_SRAM_ADDR					(HOST_IP_ADDR + 0x10000)


#define ONLY_CPL_TYPE						(0)
#define AUTO_CPL_TYPE						(1)
#define CMD_SLOT_RELEASE_TYPE				(2)

#define P_SLOT_TAG_WIDTH					(10) //slot_modified

//offset: 0x00000200, size: 4
typedef struct _NVME_STATUS_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int ccEn				:1;
			unsigned int ccShn				:2;
			unsigned int reserved0			:1;
			unsigned int cstsRdy			:1;
			unsigned int cstsShst			:2;
			unsigned int rstCnt				:4;
			unsigned int linkNum			:2;
			unsigned int linkEn				:2;
			unsigned int reserved1			:17;
		};
	};
} NVME_STATUS_REG;

//offset: 0x00000300, size: 4
typedef struct _NVME_CMD_FIFO_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int qID				:4;
			unsigned int reserved0			:1;//slot_modified
			unsigned int cmdSlotTag			:P_SLOT_TAG_WIDTH; //slot_modified
			unsigned int reserved2			:1;//slot_modified
			unsigned int cmdSeqNum			:8;
			unsigned int reserved3			:(17-P_SLOT_TAG_WIDTH);//slot_modified
			unsigned int cmdValid			:1;
		};
	};
} NVME_CMD_FIFO_REG;

//offset: 0x00000304, size: 8
typedef struct _NVME_CPL_FIFO_REG
{
	union {
		unsigned int dword[3];
		struct {
			struct
			{
				unsigned int cid				:16;
				unsigned int sqId				:4;
				unsigned int reserved0			:12;
			};

			unsigned int specific;

			unsigned short cmdSlotTag			:P_SLOT_TAG_WIDTH; //slot_modified
			unsigned short reserved1			:(14- P_SLOT_TAG_WIDTH); //slot_modified
			unsigned short cplType				:2;

			union {
				unsigned short statusFieldWord;
				struct
				{
					unsigned short reserved0	:1;
					unsigned short SC			:8;
					unsigned short SCT			:3;
					unsigned short reserved1	:2;
					unsigned short MORE			:1;
					unsigned short DNR			:1;
				}statusField;
			};
		};
	};
} NVME_CPL_FIFO_REG;

//offset: 0x0000021C, size: 4
typedef struct _NVME_ADMIN_QUEUE_SET_REG
{
	union {
		unsigned int dword;
		struct {
			unsigned int cqValid			:1;
			unsigned int sqValid			:1;
			unsigned int cqIrqEn			:1;
			unsigned int reserved0			:29;
		};
	};
} NVME_ADMIN_QUEUE_SET_REG;

//offset: 0x00000220, size: 8
typedef struct _NVME_IO_SQ_SET_REG
{
	union {
		unsigned int dword[2];
		struct {
			unsigned int pcieBaseAddrL;
			unsigned int pcieBaseAddrH		:16;//modified
			unsigned int valid				:1;
			unsigned int cqVector			:4;
			unsigned int reserved1			:3;
			unsigned int sqSize				:8;
		};
	};
} NVME_IO_SQ_SET_REG;


//offset: 0x00000260, size: 8
typedef struct _NVME_IO_CQ_SET_REG
{
	union {
		unsigned int dword[2];
		struct {
			unsigned int pcieBaseAddrL;
			unsigned int pcieBaseAddrH		:16;//modified
			unsigned int valid				:1;
			unsigned int irqVector			:3;
			unsigned int irqEn				:1;
			unsigned int reserved1			:3;
			unsigned int cqSize				:8;
		};
	};
} NVME_IO_CQ_SET_REG;



//offset: 0x00002000, size: 64 * 128
typedef struct _NVME_CMD_SRAM
{
	unsigned int dword[128][16];
} _NVME_CMD_SRAM;


void lld_pcie_async_reset(unsigned int rstCnt);
void lld_pcie_set_link_width(unsigned int linkNum);
unsigned int lld_nvme_get_cc_en();
void lld_nvme_set_csts_rdy(unsigned int rdy);
void lld_nvme_set_csts_shst(unsigned int shst);
void lld_nvme_set_admin_queue(unsigned int sqValid, unsigned int cqValid, unsigned int cqIrqEn);
#if 1
unsigned int lld_nvme_get_cmd(unsigned short *qID, unsigned short *cmdSlotTag, unsigned int *cmdSeqNum, unsigned int *cmdDword);
#endif
//unsigned int lld_nvme_get_cmd(NVME_COMMAND_ENTRY* p_command_list);
void lld_nvme_set_auto_cpl(unsigned int cmdSlotTag, unsigned int specific, unsigned int statusFieldWord);
void lld_nvme_set_slot_release(unsigned int cmdSlotTag);
void lld_nvme_set_manual_cpl(unsigned int sqId, unsigned int cid, unsigned int specific, unsigned int statusFieldWord);
void lld_nvme_set_io_sq(unsigned int ioSqIdx, unsigned int valid, unsigned int cqVector, unsigned int qSzie, unsigned int pcieBaseAddrL, unsigned int pcieBaseAddrH);
void lld_nvme_set_io_cq(unsigned int ioCqIdx, unsigned int valid, unsigned int irqEn, unsigned int irqVector, unsigned int qSzie, unsigned int pcieBaseAddrL, unsigned int pcieBaseAddrH);


#endif /* _LLD_NVME_H_ */
