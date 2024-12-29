/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NVM Express device driver tracepoints
 * Copyright (c) 2018 Johannes Thumshirn, SUSE Linux GmbH
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvme

#if !defined(_TRACE_NVME_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVME_H

#include <linux/nvme.h>
#include <linux/tracepoint.h>
#include <linux/trace_seq.h>

#include "nvme.h"

const char *nvme_trace_parse_admin_cmd(struct trace_seq *p, u8 opcode,
		u8 *cdw10);
const char *nvme_trace_parse_nvm_cmd(struct trace_seq *p, u8 opcode,
		u8 *cdw10);
const char *nvme_trace_parse_fabrics_cmd(struct trace_seq *p, u8 fctype,
		u8 *spc);

#define parse_nvme_cmd(qid, opcode, fctype, cdw10)			\
	((opcode) == nvme_fabrics_command ?				\
	 nvme_trace_parse_fabrics_cmd(p, fctype, cdw10) :		\
	((qid) ?							\
	 nvme_trace_parse_nvm_cmd(p, opcode, cdw10) :			\
	 nvme_trace_parse_admin_cmd(p, opcode, cdw10)))

const char *nvme_trace_disk_name(struct trace_seq *p, char *name);
const char *nvme_trace_process_name(struct trace_seq *p, char *name);
#define __print_disk_name(name)				\
	nvme_trace_disk_name(p, name)

#define __print_process_name(name)				\
	nvme_trace_process_name(p, name)

#ifndef TRACE_HEADER_MULTI_READ
static inline void __assign_disk_name(char *name, struct gendisk *disk)
{
	if (disk)
		memcpy(name, disk->disk_name, DISK_NAME_LEN);
	else
		memset(name, 0, DISK_NAME_LEN);
}
#endif

//opimq
TRACE_EVENT(nvme_getrq,
	    TP_PROTO(struct request *req),
	    TP_ARGS(req),
	    TP_STRUCT__entry(
		__field(unsigned int, stream_id_1) //OPIMQ Debugging
		__field(unsigned int, stream_id_2) //OPIMQ Debugging
		__field(unsigned int, epoch_id_1) //OPIMQ Debugging
		__field(unsigned int, epoch_id_2) //OPIMQ Debugging
		__field(unsigned int, is_jc) //OPIMQ Debugging
		__field(unsigned int, tx_id) //OPIMQ Debugging
		__field(int, bflag) //OPIMQ Debugging
		__field(u64, pos)	
		__field(unsigned int, data_len)	
		),


	    TP_fast_assign(
		__entry->stream_id_1 = req->stream_id_1;
		__entry->stream_id_2 = req->stream_id_2;
		__entry->epoch_id_1 = req->epoch_id_1;
		__entry->epoch_id_2 = req->epoch_id_2;
		__entry->is_jc = req->is_jc;
		__entry->tx_id = req->tx_id;
		__entry->bflag = req->barrier_flag;
		__entry->pos = blk_rq_pos(req);
		__entry->data_len = blk_rq_bytes(req);
		),

	    TP_printk("sid1:%d sid2:%d eid1:%d eid2:%d tx_id: %d bflag: %d is_jc:%d lba:%llu len:%d",
			__entry->stream_id_1, __entry->stream_id_2,
			  __entry->epoch_id_1, __entry->epoch_id_2, __entry->tx_id, __entry->bflag, __entry->is_jc, __entry->pos, __entry->data_len)
);


TRACE_EVENT(nvme_setrq,
	    TP_PROTO(struct request *req),
	    TP_ARGS(req),
	    TP_STRUCT__entry(
		__field(unsigned int, stream_id_1) //OPIMQ Debugging
		__field(unsigned int, stream_id_2) //OPIMQ Debugging
		__field(unsigned int, epoch_id_1) //OPIMQ Debugging
		__field(unsigned int, epoch_id_2) //OPIMQ Debugging
		__field(unsigned int, is_jc) //OPIMQ Debugging
		__field(unsigned int, tx_id) //OPIMQ Debugging
		__field(int, bflag) //OPIMQ Debugging
		__field(u64, pos)	
		__field(unsigned int, data_len)	
		),


	    TP_fast_assign(
		__entry->stream_id_1 = req->stream_id_1;
		__entry->stream_id_2 = req->stream_id_2;
		__entry->epoch_id_1 = req->epoch_id_1;
		__entry->epoch_id_2 = req->epoch_id_2;
		__entry->is_jc = req->is_jc;
		__entry->tx_id = req->tx_id;
		__entry->bflag = req->barrier_flag;
		__entry->pos = blk_rq_pos(req);
		__entry->data_len = blk_rq_bytes(req);
		),

	    TP_printk("sid1:%d sid2:%d eid1:%d eid2:%d tx_id: %d bflag: %d is_jc:%d lba:%llu len:%d",
			__entry->stream_id_1, __entry->stream_id_2,
			  __entry->epoch_id_1, __entry->epoch_id_2, __entry->tx_id, __entry->bflag, __entry->is_jc, __entry->pos, __entry->data_len)
);


TRACE_EVENT(nvme_setup_cmd,
	    TP_PROTO(struct request *req, struct nvme_command *cmd),
	    TP_ARGS(req, cmd),
	    TP_STRUCT__entry(
		__array(char, disk, DISK_NAME_LEN)
		__field(int, ctrl_id)
		__field(int, qid)	
		__field(u8, opcode)

#ifdef ENABLE_OPIMQ	
		__field(unsigned int, stream_id_1) //OPIMQ Debugging
		__field(unsigned int, stream_id_2) //OPIMQ Debugging
		__field(unsigned int, epoch_id_1) //OPIMQ Debugging
		__field(unsigned int, epoch_id_2) //OPIMQ Debugging
		__field(unsigned int, is_jc) //OPIMQ Debugging
		__field(unsigned int, tx_id) //OPIMQ Debugging
		__field(int, bflag) //OPIMQ Debugging
		__field(int, cpu) //OPIMQ Debugging
		__field(int, pid) //OPIMQ Debugging
		__field(u16, cid)
		__field(u8, fctype)
		__field(u64, pos)	
		__field(unsigned int, data_len)	
		__array(u8, cdw10, 24) 
#else
		
		__field(u8, flags)
		__field(u8, fctype)
		__field(u16, cid)
		__field(u32, nsid)
		__field(bool, metadata)
		__array(u8, cdw10, 24) 
#endif
		),


	    TP_fast_assign(
		__entry->ctrl_id = nvme_req(req)->ctrl->instance;
		__assign_disk_name(__entry->disk, req->q->disk);
		__entry->qid = nvme_req_qid(req);
		__entry->opcode = cmd->common.opcode;
		__entry->cpu = current_thread_info()->cpu;
#ifdef ENABLE_OPIMQ
		__entry->stream_id_1 = req->stream_id_1;
		__entry->stream_id_2 = req->stream_id_2;
		__entry->epoch_id_1 = req->epoch_id_1;
		__entry->epoch_id_2 = req->epoch_id_2;
		__entry->is_jc = req->is_jc;
		__entry->tx_id = req->tx_id;
		__entry->pid = current->pid;
		//__entry->cur_qid = current->queue_id;
	//	__entry->queue_num = current->queue_num;
		__entry->bflag = req->barrier_flag;
		__entry->fctype = cmd->fabrics.fctype;
		__entry->pos = blk_rq_pos(req);
		__entry->data_len = blk_rq_bytes(req);
		__entry->cid = cmd->common.command_id;

		memcpy(__entry->cdw10, &cmd->common.cdws,
			sizeof(__entry->cdw10));
#else
		
		__entry->flags = cmd->common.flags;
		__entry->cid = cmd->common.command_id;
		__entry->nsid = le32_to_cpu(cmd->common.nsid);
		__entry->metadata = !!blk_integrity_rq(req);
		__entry->fctype = cmd->fabrics.fctype;
		__assign_disk_name(__entry->disk, req->q->disk);
		memcpy(__entry->cdw10, &cmd->common.cdws,
			sizeof(__entry->cdw10));
#endif
		),


#ifdef ENABLE_OPIMQ
/*
	    TP_printk("cpu:%d qid:%d lba:%llu len:%d cmd=(%s)",
		      __entry->cpu, 
		      __entry->qid,  __entry->pos, __entry->data_len, 
				show_opcode_name(__entry->qid, __entry->opcode, __entry->fctype))
*/

	    TP_printk("cpu:%d qid:%d\t sid1:%d eid1:%d sid2:%d eid2:%d bflag: %d\t cmd_type: %s",
		      __entry->cpu, 
		      __entry->qid, __entry->stream_id_1, __entry->epoch_id_1,
			  __entry->stream_id_2, __entry->epoch_id_2, __entry->bflag, 
				show_opcode_name(__entry->qid, __entry->opcode, __entry->fctype))

/*
	    TP_printk("PID %d cpuid %d QID %d cmdid %u lba:%llu len:%d cmd=(%s)",
		      __entry->pid, __entry->cpu,
		      __entry->qid, __entry->cid,  __entry->pos, __entry->data_len, 
				show_opcode_name(__entry->qid, __entry->opcode, __entry->fctype))
*/
#else
		TP_printk("nvme%d: %sqid=%d, cmdid=%u, nsid=%u, flags=0x%x, meta=0x%x, cmd=(%s %s)",
		      __entry->ctrl_id, __print_disk_name(__entry->disk),
		      __entry->qid, __entry->cid, __entry->nsid,
		      __entry->flags, __entry->metadata,
		      show_opcode_name(__entry->qid, __entry->opcode,
				__entry->fctype),
		      parse_nvme_cmd(__entry->qid, __entry->opcode,
				__entry->fctype, __entry->cdw10))

#endif

);

TRACE_EVENT(nvme_cmd_dc,
	    TP_PROTO(struct request *req, char dc),
	    TP_ARGS(req, dc),
	    TP_STRUCT__entry(
		__field(int, qid)	

		__field(unsigned int, stream_id_1) //OPIMQ Debugging
		__field(unsigned int, stream_id_2) //OPIMQ Debugging
		__field(unsigned int, epoch_id_1) //OPIMQ Debugging
		__field(unsigned int, epoch_id_2) //OPIMQ Debugging
		__field(unsigned int, is_jc) //OPIMQ Debugging
		__field(int, bflag) //OPIMQ Debugging
		__field(int, cpu) //OPIMQ Debugging
		__field(u64, pos)	
		__field(unsigned int, data_len)	
		__field(char, dc)
		),


	    TP_fast_assign(
		__entry->qid = nvme_req_qid(req);
		__entry->stream_id_1 = req->stream_id_1;
		__entry->stream_id_2 = req->stream_id_2;
		__entry->epoch_id_1 = req->epoch_id_1;
		__entry->epoch_id_2 = req->epoch_id_2;
		__entry->is_jc = req->is_jc;
		__entry->cpu = current->cpu_id;
		__entry->bflag = req->barrier_flag;
		__entry->pos = blk_rq_pos(req);
		__entry->data_len = blk_rq_bytes(req);
		__entry->dc = dc;
		),

	    TP_printk("DC:%c cpu:%d qid:%d sid1:%d sid2:%d eid1:%d eid2:%d bflag: %d is_jc:%d lba:%llu len:%d",
		      __entry->dc, __entry->cpu,
		      __entry->qid, __entry->stream_id_1, __entry->stream_id_2,
			  __entry->epoch_id_1, __entry->epoch_id_2, __entry->bflag, __entry->is_jc, __entry->pos, __entry->data_len)
);

TRACE_EVENT(nvme_complete_rq,
	    TP_PROTO(struct request *req),
	    TP_ARGS(req),
	    TP_STRUCT__entry(
		__array(char, disk, DISK_NAME_LEN)
		__field(int, ctrl_id)
		__field(int, qid)
		__field(int, cid)
		__field(u64, result)
		__field(u8, retries)
		__field(u8, flags)
		__field(u16, status)
	    ),
	    TP_fast_assign(
		__entry->ctrl_id = nvme_req(req)->ctrl->instance;
		__entry->qid = nvme_req_qid(req);
		__entry->cid = nvme_req(req)->cmd->common.command_id;
		__entry->result = le64_to_cpu(nvme_req(req)->result.u64);
		__entry->retries = nvme_req(req)->retries;
		__entry->flags = nvme_req(req)->flags;
		__entry->status = nvme_req(req)->status;
		__assign_disk_name(__entry->disk, req->q->disk);
	    ),
	    TP_printk("nvme%d: %sqid=%d, cmdid=%u, res=%#llx, retries=%u, flags=0x%x, status=%#x",
		      __entry->ctrl_id, __print_disk_name(__entry->disk),
		      __entry->qid, __entry->cid, __entry->result,
		      __entry->retries, __entry->flags, __entry->status)

);

#define aer_name(aer) { aer, #aer }

TRACE_EVENT(nvme_async_event,
	TP_PROTO(struct nvme_ctrl *ctrl, u32 result),
	TP_ARGS(ctrl, result),
	TP_STRUCT__entry(
		__field(int, ctrl_id)
		__field(u32, result)
	),
	TP_fast_assign(
		__entry->ctrl_id = ctrl->instance;
		__entry->result = result;
	),
	TP_printk("nvme%d: NVME_AEN=%#08x [%s]",
		__entry->ctrl_id, __entry->result,
		__print_symbolic(__entry->result,
		aer_name(NVME_AER_NOTICE_NS_CHANGED),
		aer_name(NVME_AER_NOTICE_ANA),
		aer_name(NVME_AER_NOTICE_FW_ACT_STARTING),
		aer_name(NVME_AER_NOTICE_DISC_CHANGED),
		aer_name(NVME_AER_ERROR),
		aer_name(NVME_AER_SMART),
		aer_name(NVME_AER_CSS),
		aer_name(NVME_AER_VS))
	)
);

#undef aer_name

TRACE_EVENT(nvme_sq,
	TP_PROTO(struct request *req, __le16 sq_head, int sq_tail, s64 complete),
	TP_ARGS(req, sq_head, sq_tail, complete),
	TP_STRUCT__entry(
		__field(int, ctrl_id)
		__array(char, disk, DISK_NAME_LEN)
		__array(char, pname, TASK_COMM_LEN)
		__field(int, qid)
		__field(int, cpuid)
		__field(int, pid)
		__field(int, cid)
		__field(u16, sq_head)
		__field(u16, sq_tail)
		__field(s64, iolat)
		__field(s64, iolat2)
		__field(u8, fctype)
		__field(u8, opcode)
	),
	TP_fast_assign(
		__entry->ctrl_id = nvme_req(req)->ctrl->instance;
		__assign_disk_name(__entry->disk, req->q->disk);
		memcpy(__entry->pname, req->process_comm, TASK_COMM_LEN);
		__entry->qid = nvme_req_qid(req);
		__entry->cpuid = raw_smp_processor_id();
		__entry->cid = nvme_req(req)->cmd->common.command_id;
		__entry->sq_head = le16_to_cpu(sq_head);
		__entry->sq_tail = sq_tail;
		__entry->iolat = complete - req->start;
		__entry->iolat2 = complete - req->start2;
		__entry->pid = current->pid;
		__entry->fctype = nvme_req(req)->cmd->fabrics.fctype;
		__entry->opcode = nvme_req(req)->cmd->common.opcode;
	),
/*
	TP_printk("nvme%d: %sqid=%d, cmdid=%u, head=%u, tail=%u",
		__entry->ctrl_id, __print_disk_name(__entry->disk),
		__entry->qid, __entry->cid, __entry->sq_head, __entry->sq_tail
	)
*/
	TP_printk("%s PID %d qid %d cpuid %d cmdid %u iolat %llu iolat2 %llu cmd=(%s)",
		__print_process_name(__entry->pname), __entry->pid, __entry->qid, __entry->cpuid, __entry->cid, __entry->iolat, __entry->iolat2, 
		show_opcode_name(__entry->qid, __entry->opcode, __entry->fctype))
);

#endif /* _TRACE_NVME_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

/* This part must be outside protection */
#include <trace/define_trace.h>
