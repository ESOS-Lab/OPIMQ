/*
 * barrier_ftl.c
 *
 *  Created on: 2021. 8. 10.
 *      Author: Jieun Kim, Suik Park
 */
#include "xil_printf.h"
#include "../debug.h"

#include "../memory_map.h"
#include "../request_schedule.h"

#include "barrier_ftl.h"
#include "address_translation.h"

barrier_context_t g_barrierContext;

#if (SUPPORT_BARRIER_FTL == 1)
#define N_STREAM_OFFS (7)
#define N_STREAM_MASK ((1 << N_STREAM_OFFS) - 1)

#define N_EPOCH_OFFS (8)
#define N_EPOCH_MASK ((1 << N_EPOCH_OFFS) - 1)

uint32_t get_stream_index (unsigned int sid) {
	return sid & N_STREAM_MASK;
}
uint32_t get_epoch_index (unsigned int eid) {
	return eid & N_EPOCH_MASK;
}


/*
Juwon Added. update last idx, updated count. 
*/
void update_epoch_info(uint32_t sid, uint32_t eid, void* func)
{
	uint32_t sidx = get_stream_index (sid);
	uint32_t eidx = get_stream_index (eid);

	if (sid > 0){
		barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
		epoch_entry_t* epoch_entry;
		epoch_entry = &b_stream->epoch_list[eidx];
		if ((b_stream->stream_id != sid) || (epoch_entry->epoch_id != eid)){
			b_stream->stream_id = sid;
			epoch_entry->epoch_id = eid;
			//return;
			//xil_printf("[JWDBG] %s: sid or eid does not match!!, stream id: %d sid: %d epoch id: %d eid: %d                                 \n",
			//	 __func__, b_stream->stream_id, sid, epoch_entry->epoch_id, eid);
			//assert(0);
	  	}
	
		epoch_entry->num_updated_pages += 1;
		
#if (PRINT_DEBUG_MAP == 1)
		xil_printf("[Mapped] sid: %u eid: %u pg %u/%u leid: %u s0: %u s1: %u s2: %u s3: %u mappable: %u %s \n", sid, eid, 
				epoch_entry->num_updated_pages, epoch_entry->num_total_pages, 
				b_stream->last_epoch_id, 
				epoch_entry->state == EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED,
				epoch_entry->state == EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED,
				epoch_entry->state == EPOCH_STATE_CLOSED_DURABLE_UNMAPPED, 
				epoch_entry->state == EPOCH_STATE_CLOSED_DURABLE_MAPPED,
				is_mappable(sid, eid),
				func);
		
#endif
		
		if (epoch_entry->state == EPOCH_STATE_CLOSED_DURABLE_MAPPED){
			//xil_printf("[JWDBG] %s: state strange    whtttttt    !!", __func__);
			//assert(0);
		}
		if ( (epoch_entry->state == EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED) ||
		     (epoch_entry->state == EPOCH_STATE_CLOSED_DURABLE_UNMAPPED) ){
			if (epoch_entry->num_updated_pages == epoch_entry->num_total_pages){
				if ( ((b_stream->last_epoch_id + 1) % N_EPOCH) != eid){
					if (b_stream->last_epoch_id == 0 && eid == 0){
						//xil_printf("[JWDBG] %s: may be init last eid!!", __func__);
						b_stream->epoch_zero_done = 1;
						
					} else {
						//xil_printf("[JWDBG] %s: last eid weird. last_eid: %d eid: %d!!", __func__,
						//	b_stream->last_epoch_id, eid);
					}
				} else {
					b_stream->last_epoch_id = eid;
					epoch_entry->state = EPOCH_STATE_CLOSED_DURABLE_MAPPED; 
					//xil_printf("[UPDATE LEID] new last eid: %u\n", eid);
					// For handling the case that eid 1 passes the condition last eid +1 == eid.
					if (eid == 1)
						b_stream->epoch_zero_done = 1;
					//xil_printf("[ UPDATE last_epoch_id ] new last_eid: %u\n", b_stream->last_epoch_id);
				}
			}
		} 
			
	}
}

void register_first_epoch(barrier_stream_entry_t* b_stream, unsigned int eid)
{
	b_stream->first_epoch_id = eid;
	if (eid > 0)
		b_stream->last_epoch_id = eid-1; // For last_epoch_id + 1 == eid
	else
		b_stream->last_epoch_id = 0; // For is_first_epoch()
	b_stream->is_first = 0;
}

/* Update the epoch state if the slice has barrier flag, 1. 
   Update the total page (slice) counts of corresponding epoch.
   Return the previous epoch state */

enum epoch_state barrier_check_and_set_epoch_state (uint32_t sid, uint32_t eid, uint32_t barrier, unsigned int page_cnt) {
  uint32_t sidx = get_stream_index (sid);
  uint32_t eidx = get_stream_index (eid);
  barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
  epoch_entry_t* epoch_entry;
  unsigned int prev_eidx;

  if (b_stream->stream_id != 0 && b_stream->stream_id != sid) {
	  //xil_printf("\r\n [OPIMQ] Stream Table Initialization is needed SID: %d!!! \r\n", sid);
	  //return 0;
  }

  // Juwon Added. To handle when first eid is not zero. 
  if (b_stream->is_first){
	register_first_epoch(b_stream, eid);
  }

  // Set stream id
  b_stream->stream_id = sid;
  // Set epoch state
  epoch_entry = &b_stream->epoch_list[eidx];
  // Set epoch id
  epoch_entry->epoch_id = eid;

  if (1 == barrier) {
	  epoch_entry->state = EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED;
	  //xil_printf("[CLOSED] sid: %u eid: %u leid: %u\n", sid, eid, b_stream->last_epoch_id);
	  if (epoch_entry->num_updated_pages >= epoch_entry->num_total_pages){
		if ( ((b_stream->last_epoch_id + 1) % N_EPOCH) > eid){ // SInce sometimes barreir flag does not comes in order. 
			if (b_stream->last_epoch_id == 0 && eid == 0){
				//xil_printf("[JWDBG] %s: may be init last eid!!", __func__);
				b_stream->epoch_zero_done = 1;
				
			} else {
				//xil_printf("[JWDBG] %s: last eid weird. last_eid: %d eid: %d!!", __func__,
				//	b_stream->last_epoch_id, eid);
			}
		} else {
			b_stream->last_epoch_id = eid;
			//epoch_entry->state = EPOCH_STATE_CLOSED_DURABLE_MAPPED; 
			//xil_printf("[UPDATE LEID] new last eid: %u\n", eid);
			// For handling the case that eid 1 passes the condition last eid +1 == eid.
			if (eid == 1)
				b_stream->epoch_zero_done = 1;
			//xil_printf("[ UPDATE last_epoch_id ] new last_eid: %u\n", b_stream->last_epoch_id);
		}
	}
	  //b_stream->last_epoch_id = eid;
  } else {
  	if (epoch_entry->state == EPOCH_STATE_NONE)
  	  	epoch_entry->state = EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED;
  }
  //else {
  //	  epoch_entry->state = EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED;
  //}

  // Update total page counts
  epoch_entry->num_total_pages += page_cnt;

  //xil_printf("[ Update Total Pages ] sid %u eid %u total cnt %u in %s\r\n", sid, eid, g_barrierContext.stream[sidx].epoch_list[eidx].num_total_pages, __func__);

  // Check the previous epoch state & return
  if (0 == eid) {
  		return EPOCH_STATE_CLOSED_DURABLE_MAPPED;
  }
  if (0 == eidx) {
  		prev_eidx = N_EPOCH -1 ;
  } else {
  		prev_eidx = eidx - 1;
  }
  
  if (g_barrierContext.stream[sidx].epoch_list[prev_eidx].epoch_id == eid -1) {
  		return g_barrierContext.stream[sidx].epoch_list[prev_eidx].state;
  } else {
  		return EPOCH_STATE_NONE;
  }
}



void barrier_set_epoch_state (uint32_t sid, uint32_t eid, uint32_t barrier, unsigned int page_cnt) {
  uint32_t sidx = get_stream_index (sid);
  uint32_t eidx = get_stream_index (eid);
  barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
  epoch_entry_t* epoch_entry;
  unsigned int prev_eidx;

  if (b_stream->stream_id != 0 && b_stream->stream_id != sid) {
	  //xil_printf("\r\n [OPIMQ] Stream Table Initialization is needed SID: %d!!! \r\n", sid);
	  //return 0;
  }

  // Juwon Added. To handle when first eid is not zero. 
  if (b_stream->is_first){
	register_first_epoch(b_stream, eid);
  }

  // Set stream id
  b_stream->stream_id = sid;
  // Set epoch state
  epoch_entry = &b_stream->epoch_list[eidx];
  // Set epoch id
  epoch_entry->epoch_id = eid;

  if (1 == barrier) {
	  epoch_entry->state = EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED;
	  //xil_printf("[CLOSED] sid: %u eid: %u leid: %u\n", sid, eid, b_stream->last_epoch_id);
	  if (epoch_entry->num_updated_pages >= epoch_entry->num_total_pages){
		if ( ((b_stream->last_epoch_id + 1) % N_EPOCH) > eid){ // SInce sometimes barreir flag does not comes in order. 
			if (b_stream->last_epoch_id == 0 && eid == 0){
				//xil_printf("[JWDBG] %s: may be init last eid!!", __func__);
				b_stream->epoch_zero_done = 1;
				
			} else {
				//xil_printf("[JWDBG] %s: last eid weird. last_eid: %d eid: %d!!", __func__,
				//	b_stream->last_epoch_id, eid);
			}
		} else {
			b_stream->last_epoch_id = eid;
			//epoch_entry->state = EPOCH_STATE_CLOSED_DURABLE_MAPPED; 
			//xil_printf("[UPDATE LEID] new last eid: %u\n", eid);
			// For handling the case that eid 1 passes the condition last eid +1 == eid.
			if (eid == 1)
				b_stream->epoch_zero_done = 1;
			//xil_printf("[ UPDATE last_epoch_id ] new last_eid: %u\n", b_stream->last_epoch_id);
		}
	}
	  //b_stream->last_epoch_id = eid;
  } else {
  	if (epoch_entry->state == EPOCH_STATE_NONE)
  	  	epoch_entry->state = EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED;
  }
  //else {
  //	  epoch_entry->state = EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED;
  //}

  // Update total page counts
  epoch_entry->num_total_pages += page_cnt;

  //xil_printf("[ Update Total Pages ] sid %u eid %u total cnt %u in %s\r\n", sid, eid, g_barrierContext.stream[sidx].epoch_list[eidx].num_total_pages, __func__);
}



enum epoch_state barrier_check_prev_epoch_state (uint32_t sid, uint32_t eid) {
  uint32_t sidx = get_stream_index (sid);
  uint32_t eidx = get_stream_index (eid);
  barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
  epoch_entry_t* epoch_entry;
  unsigned int prev_eidx;

  if (0 == eid) {
  		return EPOCH_STATE_CLOSED_DURABLE_MAPPED;
  }
  if (0 == eidx) {
  		prev_eidx = N_EPOCH -1 ;
  } else {
  		prev_eidx = eidx - 1;
  }
  
  if (g_barrierContext.stream[sidx].epoch_list[prev_eidx].epoch_id == eid -1) {
  		return g_barrierContext.stream[sidx].epoch_list[prev_eidx].state;
  } else {
  		return EPOCH_STATE_NONE;
  }
}




/*
enum epoch_state barrier_check_prev_epoch_state(unsigned int sid, unsigned int eid, uint32_t barrier, unsigned int page_cnt)
{
	uint32_t sidx = get_stream_index(sid);
	uint32_t eidx = get_epoch_index(eid);
	uint32_t prev_eidx;
	epoch_entry_t* prev_epoch_entry;

	barrier_stream_entry_t* stream_entry = &g_barrierContext.stream[sidx];
  	epoch_entry_t* epoch_entry;

  	if (stream_entry->is_first){
		register_first_epoch(stream_entry, eid);
  	}

  	// Set stream id
  	stream_entry->stream_id = sid;
 	// Set epoch state
 	epoch_entry = &stream_entry->epoch_list[eidx];
 	// Set epoch id
 	epoch_entry->epoch_id = eid;
  	epoch_entry->num_total_pages += page_cnt;
  	//xil_printf("[ Update Total Pages ] sid %u eid %u total cnt %u \r\n", 
	//		sid, eid, g_barrierContext.stream[sidx].epoch_list[eidx].num_total_pages);

  	if (1 == barrier) {
		  epoch_entry->state = EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED;
  	} 
	else {
		if (epoch_entry->state == EPOCH_STATE_NONE)
		  	epoch_entry->state = EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED;
  	}

	if (0 == eid)
	{
		return EPOCH_STATE_CLOSED_DURABLE_MAPPED;
	}
	if (0 == eidx) {
		prev_eidx = N_EPOCH -1 ;
	} else {
		prev_eidx = eidx - 1;
	}

	prev_epoch_entry = &stream_entry->epoch_list[prev_eidx];
	if (prev_epoch_entry->epoch_id == eid -1) {
		return prev_epoch_entry->state;
	} else {
		return EPOCH_STATE_NONE;
	}
}
*/

/*
void barrier_increase_total_page_count (unsigned int sid, unsigned int eid, unsigned int page_cnt) {
	uint32_t sidx = get_stream_index (sid);
	uint32_t eidx = get_stream_index (eid);

	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
	epoch_entry_t* epoch_entry = &b_stream->epoch_list[eidx];
	epoch_entry->num_total_pages += page_cnt;
}
*/
unsigned int barrier_check_total_page_count (unsigned int sid, unsigned int eid) {
	uint32_t sidx = get_stream_index (sid);
	uint32_t eidx = get_stream_index (eid);

	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
	epoch_entry_t* epoch_entry = &b_stream->epoch_list[eidx];

	return epoch_entry->num_total_pages;
}




void barrier_increase_durable_page_count(unsigned int sid, unsigned int eid, int page_cnt)
{
	uint32_t sidx = get_stream_index (sid);
	uint32_t eidx = get_stream_index (eid);

	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
	epoch_entry_t* epoch_entry = &b_stream->epoch_list[eidx];
	epoch_entry->num_durable_pages += page_cnt;
	//xil_printf("[ Increase Durable Page Count ] sid %u eid %u cnt %u \r\n", sid, eid, g_barrierContext.stream[sidx].epoch_list[eidx].num_durable_pages);
}

/*void barrier_increase_updated_page_count(unsigned int sid, unsigned int eid, int page_cnt)
{
	uint32_t sidx = get_stream_index (sid);
	uint32_t eidx = get_stream_index (eid);
	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
	epoch_entry_t* epoch_entry = &b_stream->epoch_list[eidx];
	epoch_entry->num_updated_pages += page_cnt;
	xil_printf("[ Increase Updated Page Count ] sid %u eid %u cnt %u \r\n", sid, eid, g_barrierContext.stream[sidx].epoch_list[eidx].num_updated_pages);

	// Update State
	if (epoch_entry->num_total_pages == epoch_entry->num_updated_pages) {
		if (epoch_entry->state != EPOCH_STATE_CLOSED_DURABLE_UNMAPPED){
			xil_printf("[JWDBG] %s: why state is not closed durable unmapped?\n", __func__);
			assert(0);
		}
		epoch_entry->state = EPOCH_STATE_CLOSED_DURABLE_MAPPED;
		xil_printf("[ PERSISTED ] sid %u eid %u           \r\n", sid, eid);
		// Update last persisted epoch id
		if (b_stream->last_epoch_id < eid) {
			b_stream->last_epoch_id = eid;
		}
	}
}*/


int map_suspended_entry(barrier_stream_entry_t* stream_entry, uint32_t hidx);

//	uint32_t sid, uint32_t head_iter, uint32_t tail_iter, 
//					uint32_t hidx, uint32_t tidx)
void suspension_array_overflow_check(barrier_stream_entry_t* stream_entry, int need_room)
{
	uint32_t hidx, tidx, sid;
	unsigned long head_iter, tail_iter;
	sid = stream_entry->stream_id;	
	tail_iter = stream_entry->tail_iter;

	if (need_room){
		 tidx = (stream_entry->tail_idx + 1) % N_SUSPENSION;

		if (tidx < stream_entry->tail_idx)
			tail_iter = tail_iter + 1;

	} else {
		tidx = stream_entry->tail_idx;
	}

INIT: 
	hidx = stream_entry->head_idx;
	head_iter = stream_entry->head_iter;

	if (!(head_iter <= tail_iter))
		xil_printf("%s: cond 1 fail. sid: %u head iter: %u tail iter: %u \n", 
			__func__, sid, head_iter, tail_iter);
	assert(head_iter <= tail_iter);
	if (!(tail_iter - head_iter <= 1))
		xil_printf("%s: cond 2 fail. sid %u head iter: %u tail iter: %u \n", 
			__func__, sid, head_iter, tail_iter);
	assert(tail_iter - head_iter <= 1);

	if (tail_iter - head_iter == 1){
		if (!(tidx < hidx)){
			if (map_suspended_entry(stream_entry, hidx))
				goto INIT;
			xil_printf("%s: cond 3 fail. sid %u head iter: %u tail iter: %u hidx: %u tidx: %u \n", 
				__func__, sid, head_iter, tail_iter, hidx, tidx);
			xil_printf("print all sentry. last eid: %u\n", stream_entry->last_epoch_id);
			for (int i = 0; i < N_SUSPENSION; i += 1){
				int i_ = (i + hidx) % N_SUSPENSION;	
				mapping_wait_entry_t* sentry = &stream_entry->suspension_list[i_];
				if (sentry->valid){
					xil_printf("hidx: %u eid: %u\n", i_, sentry->epoch_id);
				}
			}
			
			assert(tidx < hidx);
		}
	}
	if (tail_iter == head_iter){
		if (!(tidx >= hidx))
			xil_printf("%s: cond 4 fail. sid %u head iter: %u tail iter: %u \n", 
				__func__, sid, head_iter, tail_iter);
		assert(tidx >= hidx);
	}
}

void barrier_insert_suspension_array_dual_stream(unsigned int logicalSliceAddr, unsigned int virtualSliceAddr, unsigned int sid1, unsigned int eid1, unsigned int sid2, unsigned int eid2, unsigned int mappable_case) {
	
	uint32_t sidx1 = get_stream_index (sid1);
	uint32_t sidx2 = get_stream_index (sid2);
	barrier_stream_entry_t* stream_entry_1 = &g_barrierContext.stream[sidx1];
	barrier_stream_entry_t* stream_entry_2 = &g_barrierContext.stream[sidx2];

	uint32_t hidx_1 = stream_entry_1->head_idx,
		 tidx_1 = stream_entry_1->tail_idx,
		 hidx_2 = stream_entry_2->head_idx,
		 tidx_2 = stream_entry_2->tail_idx,
		 next_idx_1 = (tidx_1+1) % N_SUSPENSION,
		 next_idx_2 = (tidx_2+1) % N_SUSPENSION;

	unsigned long head_iter_1 = stream_entry_1->head_iter,
		      next_tail_iter_1 = stream_entry_1->tail_iter,
		      head_iter_2 = stream_entry_2->head_iter,
		      next_tail_iter_2 = stream_entry_2->tail_iter;

	if (next_idx_1 < tidx_1)
		next_tail_iter_1 += 1;

	if (next_idx_2 < tidx_2)
		next_tail_iter_2 += 1;
	//xil_printf("%s: hello shival 1\n", __func__);	
	/* suspension array overflow check */
	//suspension_array_overflow_check(sid1, head_iter_1, next_tail_iter_1, hidx_1, next_idx_1);
	//suspension_array_overflow_check(sid2, head_iter_2, next_tail_iter_2, hidx_2, next_idx_2);
	suspension_array_overflow_check(stream_entry_1, 1);
	suspension_array_overflow_check(stream_entry_2, 1);
	//xil_printf("%s: hello shival 2\n", __func__);	

	mapping_wait_entry_t *sentry1 = &(stream_entry_1->suspension_list[next_idx_1]);
	mapping_wait_entry_t *sentry2 = &(stream_entry_2->suspension_list[next_idx_2]);
	sentry1->pair_entry_pointer = NULL;
	sentry2->pair_entry_pointer = NULL;
	//xil_printf("%s: hello shival 3\n", __func__);	
	/* Just Insert. Not sorting */
	if (mappable_case == 1) { // Suspend only stream 1
		//xil_printf("%s: hello shival 3-1\n", __func__);	
		sentry1->epoch_id = eid1;
		sentry1->lpn = logicalSliceAddr;
		sentry1->ppn =  virtualSliceAddr;
		sentry1->valid =  1;
		//stream_entry_1->valid_length++;	
		sentry1->pair_stream_id = sid2;
		sentry1->pair_epoch_id = eid2;
		/*if mappable_2 is true, stream 2 possibly does not exist. so check sid2. */	
		sentry1->pair_exist = (sid2 > 0)? 1: 0;
		sentry1->pair_entry_pointer = NULL;

		stream_entry_1->tail_idx = next_idx_1;
		stream_entry_1->tail_iter = next_tail_iter_1;

		//xil_printf("[Insert Suspension] sid %u eid %u | mappable pair: sid: %u eid: %u in %s\n", 
		//	sid1, eid1, sid2, eid2, __func__);

		stream_entry_1->suspended_entry_cnt += 1;
		g_barrierContext.g_suspended_entry_cnt += 1;

#if (PRINT_DEBUG_SP == 1)
		xil_printf("[INSERT] sid: %u eid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
			sid1, eid1, 
			stream_entry_1->head_iter, stream_entry_1->head_idx, 
			stream_entry_1->tail_iter, stream_entry_1->tail_idx );
#endif
		
	} else if (mappable_case == 2) { // Suspend only stream 2
		sentry2->epoch_id = eid2;
		sentry2->lpn = logicalSliceAddr;
		sentry2->ppn =  virtualSliceAddr;
		sentry2->valid =  1;
		//stream_entry_2->valid_length++;			
		sentry2->pair_stream_id = sid1;
		sentry2->pair_epoch_id = eid1;
		sentry2->pair_exist = 1;
		sentry2->pair_entry_pointer = NULL;

		stream_entry_2->tail_idx = next_idx_2;
		stream_entry_2->tail_iter = next_tail_iter_2;

		//xil_printf("[Insert Suspension] sid %u eid %u | mappable pair: sid: %u eid: %u in %s\n", 
		//	sid2, eid2, sid1, eid1, __func__);
		stream_entry_2->suspended_entry_cnt += 1;
		g_barrierContext.g_suspended_entry_cnt += 1;
#if (PRINT_DEBUG_SP == 1)
		xil_printf("[INSERT] sid: %u eid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
			sid2, eid2, 
			stream_entry_2->head_iter, stream_entry_2->head_idx, 
			stream_entry_2->tail_iter, stream_entry_2->tail_idx );
#endif
	} else if (mappable_case == 0) { // Suspend in both streams
		// Insert entry in stream 1
		//xil_printf("%s: hello shival 3-3\n", __func__);	
		sentry1->epoch_id = eid1;
		sentry1->lpn = logicalSliceAddr;
		sentry1->ppn =  virtualSliceAddr;
		sentry1->valid =  1;
		sentry1->pair_stream_id = sid2;
		sentry1->pair_epoch_id = eid2;
		//stream_entry_1->valid_length++;			

		// Update peer pointer: save the suspension entry in stream 2's address.
		sentry1->pair_exist = 1;
		sentry1->pair_entry_pointer = sentry2;

		stream_entry_1->tail_iter = next_tail_iter_1;
		stream_entry_1->tail_idx = next_idx_1;
#if (PRINT_DEBUG_SP == 1)
		xil_printf("[INSERT] sid: %u eid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
			sid1, eid1, 
			stream_entry_1->head_iter, stream_entry_1->head_idx, 
			stream_entry_1->tail_iter, stream_entry_1->tail_idx );
#endif

		// Insert entry in stream 2
		sentry2->epoch_id = eid2;
		sentry2->lpn = logicalSliceAddr;
		sentry2->ppn =  virtualSliceAddr;
		sentry2->valid =  1;
		sentry2->pair_stream_id = sid1;
		sentry2->pair_epoch_id = eid1;
		//stream_entry_2->valid_length++;			

		// Update peer pointer: save the suspension entry in stream 1's address.
		sentry2->pair_exist = 1;
		sentry2->pair_entry_pointer = sentry1;

		stream_entry_2->tail_iter = next_tail_iter_2;
		stream_entry_2->tail_idx = next_idx_2;
#if (PRINT_DEBUG_SP == 1)
		xil_printf("[INSERT] sid: %u eid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
			sid2, eid2, 
			stream_entry_2->head_iter, stream_entry_2->head_idx, 
			stream_entry_2->tail_iter, stream_entry_2->tail_idx );
#endif
		//xil_printf("[Insert Suspension] sid %u eid %u & sid: %u eid: %u in %s\n", 
		//	sid1, eid1, sid2, eid2, __func__);
		stream_entry_1->suspended_entry_cnt += 1;
		stream_entry_2->suspended_entry_cnt += 1;
		g_barrierContext.g_suspended_entry_cnt += 2;
	 } else{
		xil_printf("[JWDBG] %s: invalid arg: mapping_case: %d\n", __func__, mappable_case);
		assert(0);
	}
	
	//if (stream_entry_1->valid_length == N_SUSPENSION || stream_entry_2->valid_length == N_SUSPENSION){
	//	xil_printf("[BarrierFTL] Suspension List is exceeded! sid1 %u eid1 %u         \r\n", sid1, eid1);
	//	return;
	//}
}


// Juwon Added
uint32_t is_first_epoch(uint32_t eid, uint32_t last_eid)
{
	return (eid == 0 && last_eid == 0)? 1: 0;
}

uint8_t is_mappable(uint32_t sid, uint32_t eid)
{
	uint32_t sidx = get_stream_index(sid);
	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];

	uint8_t pass_epoch_1_corner_case = (b_stream->first_epoch_id == 0 && eid == 1)?
				(b_stream->epoch_zero_done) : 1;
	return ( (pass_epoch_1_corner_case && (b_stream->last_epoch_id + 1 >= eid)) 
		|| is_first_epoch(eid, b_stream->last_epoch_id));

}

static uint8_t pass_epoch_one_corner_case(mapping_wait_entry_t* sentry, barrier_stream_entry_t* b_stream) 
{
	if (b_stream->first_epoch_id == 0 && sentry->epoch_id == 1){
		return (b_stream->epoch_zero_done);
	}
	return 1;

}


uint32_t pair_is_mappable(mapping_wait_entry_t* sentry)
{
	uint32_t sid = sentry->pair_stream_id;
	uint32_t eid = sentry->pair_epoch_id;
	uint32_t sidx = get_stream_index (sid);
	barrier_stream_entry_t* b_stream = &g_barrierContext.stream[sidx];
	if ( (pass_epoch_one_corner_case(sentry, b_stream) && b_stream->last_epoch_id + 1 >= eid) 
		|| is_first_epoch(eid, b_stream->last_epoch_id))
		return 1;
	else if (b_stream->last_epoch_id + 1 < eid)
		return 0;
	else{
		return 1;
		/*xil_printf("[JWDBG] %s: sid: %u last eid %u should be les than eid %u\n",
			__func__, sid, b_stream->last_epoch_id, eid);
		assert(0);
		*/
	}

}

void update_epoch_info_pair(mapping_wait_entry_t* sentry)
{
	uint32_t sid = sentry->pair_stream_id;
	uint32_t eid = sentry->pair_epoch_id;
	//xil_printf("%s bef update epoch info\n", __func__);
	update_epoch_info(sid, eid, __func__);
}

void remove_suspension_entry(uint32_t sid, mapping_wait_entry_t* sentry)
{
	uint32_t sidx = get_stream_index(sid);
	barrier_stream_entry_t* stream_entry = &g_barrierContext.stream[sidx];

	stream_entry->suspended_entry_cnt -= 1;
	assert(stream_entry->suspended_entry_cnt >= 0);
	g_barrierContext.g_suspended_entry_cnt -= 1;
	assert(g_barrierContext.g_suspended_entry_cnt >= 0);
	
	sentry->valid = 0;
	sentry->epoch_id = 0;
	sentry->lpn = 0;
	sentry->ppn = 0;
	sentry->pair_stream_id = 0;
	sentry->pair_epoch_id = 0;
	sentry->pair_exist = 0;
	sentry->pair_entry_pointer = NULL;
}

int map_suspended_entry(barrier_stream_entry_t* stream_entry, uint32_t hidx)
{
	mapping_wait_entry_t* sentry;
	int i, ii, mapped = 0;
	uint8_t is_first;
	int first_unmapped_valid_idx;
	uint32_t cur_eidx;
	uint32_t tidx = stream_entry->tail_idx;
	uint32_t vlen = (hidx <= tidx)? tidx - hidx + 1: N_SUSPENSION - (hidx - tidx - 1);
SCAN:
	first_unmapped_valid_idx = -1;
	is_first = 1;
	for (int i = 0; i < vlen; i += 1){
		ii = (i + hidx) % N_SUSPENSION;	
		sentry = &stream_entry->suspension_list[ii];
#if (PRINT_DEBUG_SP == 1)
		printf("%s: sid: %u eid: %u idx: %u\n", __func__, stream_entry->stream_id, 
								sentry->epoch_id, ii);
#endif
		if (sentry->valid==0){
		//printf("invalid idx: %u\n", ii);
			continue;
		}

		// [JWQ] why epoch_id -1 <= last_epoch_id? why not epoch_id? last epoch id is 
		
		if ( is_first_epoch(sentry->epoch_id, stream_entry->last_epoch_id) 
			|| (pass_epoch_one_corner_case(sentry, stream_entry) && sentry->epoch_id  <=  stream_entry->last_epoch_id + 1) ) {
			if (sentry->pair_exist) {
				// Do not update suspended mapping info.
				// Just update pair's pointer to NULL, which is pointing me.
				if (pair_is_mappable(sentry)){
					update_epoch_info_pair(sentry);
					// remove pair from suspension list
					if (sentry->pair_entry_pointer != NULL)
						remove_suspension_entry(sentry->pair_stream_id, sentry->pair_entry_pointer);
					goto UPDATE_MAP_;
				} else {
					/* NOT MAPPED CASE 1 */
					if (is_first){
						first_unmapped_valid_idx = ii;
						is_first = 0;
					}
					
					//printf("unmapped case 1 sid: %u eid: %u idx: %u\n", stream_entry->stream_id, sentry->epoch_id,  ii);
				}
			} else {
UPDATE_MAP_:
				// Update suspended page mapping info
				UpdateAddrWrite(sentry->lpn, sentry->ppn);
				//xil_printf("%s bef update epoch info\n", __func__);
				update_epoch_info(stream_entry->stream_id, sentry->epoch_id, __func__);
				//xil_printf("whats up shival 2\n");
				cur_eidx = get_epoch_index (sentry->epoch_id);
				//g_barrierContext.stream[sidx].epoch_list[cur_eidx].num_updated_pages += 1;
				// modify header idx
				//if (first_unmapped_valid_idx == ii){
				//	is_first = 1;
				//	first_unmapped_valid_idx = -1;
				//}

				// Update Epoch State
				if ( stream_entry->epoch_list[cur_eidx].num_updated_pages > stream_entry->epoch_list[cur_eidx].num_total_pages) {
					assert(0);
				}


				if ((stream_entry->epoch_list[cur_eidx].num_updated_pages == stream_entry->epoch_list[cur_eidx].num_total_pages) &&
					 ( stream_entry->epoch_list[cur_eidx].state == EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED )) {
					stream_entry->epoch_list[cur_eidx].state = EPOCH_STATE_CLOSED_DURABLE_MAPPED;
				}
				
#if (PRINT_DEBUG_SP == 1)
			        xil_printf("[REMOVE] sid: %u eid: %u idx: %u\n", stream_entry->stream_id, sentry->epoch_id, ii);
#endif
				remove_suspension_entry(stream_entry->stream_id, sentry);
				mapped = 1;
				break;

			}
		} else {
			/* NOT MAPPED CASE 2 */
			//printf("unmapped case 2 sid: %u eid: %u idx: %u\n",stream_entry->stream_id, sentry->epoch_id,  ii);
			if (is_first){
				first_unmapped_valid_idx = ii;
				is_first = 0;
			}
		}
	}
	if (mapped == 0){
		stream_entry->last_epoch_id += 1;
		goto SCAN;

	}
	unsigned int next_hidx;	
	if (first_unmapped_valid_idx != -1){
		next_hidx = first_unmapped_valid_idx;
#if (PRINT_DEBUG_SP == 1)
		printf("udpate_nextidx sid: %u eid: %u nxtidx: %u\n",stream_entry->stream_id, sentry->epoch_id,  next_hidx);
#endif
	} else {
		next_hidx = (stream_entry->head_idx + 1) % N_SUSPENSION;
#if (PRINT_DEBUG_SP == 1)
		xil_printf("[MAP_SUS] sid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
			stream_entry->stream_id,
			stream_entry->head_iter, stream_entry->head_idx, 
			stream_entry->tail_iter, stream_entry->tail_idx );
#endif
	}
	if (stream_entry->head_idx > next_hidx) // head idx iter. 
		stream_entry->head_iter += 1;
	stream_entry->head_idx = next_hidx;
	return 1;
}







/* Search all the suspension array of streams exist 
   and update the suspended mapping information if the previous epoch's state id PERSIST.
   last_epoch_id is the largest epoch id, which is PERSIST in the stream.
   */
void barrier_search_suspension_list_dual_stream () {

	unsigned int cur_eidx;
	//xil_printf("[JWDBG] %s: Hello Shival? \r\n", __func__);

	/* Seach all the streams */

	if (g_barrierContext.g_suspended_entry_cnt == 0){
		//xil_printf("[JWDBG] %s: No suspended entry. \n", __func__);
		return;
	}
	
	for (uint32_t sidx = 0; sidx < N_STREAM && g_barrierContext.g_suspended_entry_cnt > 0 ; sidx++) {
		//xil_printf("[JWDBG] %s: sidx: %u\/%u !\n", __func__, sidx, N_STREAM-1);
		barrier_stream_entry_t* stream_entry = &g_barrierContext.stream[sidx];
		if (stream_entry->stream_id > 0 && stream_entry->suspended_entry_cnt) {
			//xil_printf("[JWDBG] %s: valid sid: %u !\n", __func__, stream_entry->stream_id);
			uint32_t hidx = stream_entry->head_idx,
		 		 tidx = stream_entry->tail_idx;
			unsigned long hiter = stream_entry->head_iter,
		      		      titer = stream_entry->tail_iter;
			
			//overflow check
			suspension_array_overflow_check(stream_entry, 0);	
			//suspension_array_overflow_check(stream_entry->stream_id, hiter, titer, hidx, tidx);	

			// Iterate the suspension list
			uint32_t vlen = (hidx <= tidx)? tidx - hidx + 1: N_SUSPENSION - (hidx - tidx - 1);
			uint32_t i, delta;
			int first_unmapped_valid_idx = -1;
			uint8_t is_first = 1; 
			for ( delta = 0; delta < vlen && stream_entry->suspended_entry_cnt > 0 ; delta++) {
				i = (hidx + delta) % N_SUSPENSION;
				//xil_printf("[JWDBG] %s: rotating sarray idx: %u hidx: %u delta: %u vlen: %u !\n", __func__, i, hidx, delta, vlen);
				mapping_wait_entry_t* sentry = &(stream_entry->suspension_list[i]);
				if (sentry->valid == 0)
					continue;
				// [JWQ] why epoch_id -1 <= last_epoch_id? why not epoch_id? last epoch id is 
				
				if ( is_first_epoch(sentry->epoch_id, stream_entry->last_epoch_id) 
					|| (pass_epoch_one_corner_case(sentry, stream_entry) && sentry->epoch_id  <=  stream_entry->last_epoch_id + 1) ) {
					if (sentry->pair_exist) {
						// Do not update suspended mapping info.
						// Just update pair's pointer to NULL, which is pointing me.
						if (pair_is_mappable(sentry)){
							update_epoch_info_pair(sentry);
							// remove pair from suspension list
							if (sentry->pair_entry_pointer != NULL)
								remove_suspension_entry(sentry->pair_stream_id, sentry->pair_entry_pointer);
							goto UPDATE_MAP;
						} else {
							/* NOT MAPPED CASE 1 */
							if (is_first){
								first_unmapped_valid_idx = i;
								is_first = 0;
							}
						}
					} else {
UPDATE_MAP:
						// Update suspended page mapping info
						UpdateAddrWrite(sentry->lpn, sentry->ppn);
						//xil_printf("%s bef update epoch info\n", __func__);
						update_epoch_info(stream_entry->stream_id, sentry->epoch_id, __func__);
						//xil_printf("whats up shival 2\n");
						cur_eidx = get_epoch_index (sentry->epoch_id);
						//g_barrierContext.stream[sidx].epoch_list[cur_eidx].num_updated_pages += 1;
						// modify header idx
						if (first_unmapped_valid_idx == i){
							is_first = 1;
							first_unmapped_valid_idx = -1;
						}
						// Update Last epoch id
						//if (g_barrierContext.stream[sidx].last_epoch_id < g_barrierContext.stream[sidx].suspension_list[i].epoch_id) {
						//	g_barrierContext.stream[sidx].last_epoch_id = g_barrierContext.stream[sidx].suspension_list[i].epoch_id;
						//}

						// Update Epoch State
						if ( stream_entry->epoch_list[cur_eidx].num_updated_pages >  stream_entry->epoch_list[cur_eidx].num_total_pages) {
							//xil_printf("[JWDBG] %s: num_updated_pages overflow! epoch id: %d n_updated_pg %d n_total_pg %d     \n", 
								//__func__, sentry->epoch_id, 
								//g_barrierContext.stream[sidx].epoch_list[cur_eidx].num_updated_pages,
								//g_barrierContext.stream[sidx].epoch_list[cur_eidx].num_total_pages);
							assert(0);
						}


						if ((stream_entry->epoch_list[cur_eidx].num_updated_pages == stream_entry->epoch_list[cur_eidx].num_total_pages) &&
							 ( stream_entry->epoch_list[cur_eidx].state == EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED )) {
							stream_entry->epoch_list[cur_eidx].state = EPOCH_STATE_CLOSED_DURABLE_MAPPED;
							//xil_printf("[JWDBG] %s: not durable but just map it.  unmapped?\n", __func__);
							//assert(0);
						}
						/*if (( stream_entry->epoch_list[cur_eidx].num_updated_pages ==  stream_entry->epoch_list[cur_eidx].num_total_pages) &&
							 (stream_entry->epoch_list[cur_eidx].state == EPOCH_STATE_CLOSED_DURABLE_UNMAPPED )) {
							stream_entry->epoch_list[cur_eidx].state = EPOCH_STATE_CLOSED_DURABLE_MAPPED;

							xil_printf("\r\n [ Process Suspend List: PERSISTED SID %u EID %u last_eid %u valid length %u ] \r\n",
									stream_entry->stream_id,  
									sentry->epoch_id,
									stream_entry->last_epoch_id,  stream_entry->valid_length);
						}*/
			        		//xil_printf("[REMOVE] sid: %u eid: %u idx: %u\n", stream_entry->stream_id, sentry->epoch_id, i);
						remove_suspension_entry(stream_entry->stream_id, sentry);
						//g_barrierContext.stream[sidx].valid_length -= 1;

						//xil_printf("[ Process Suspend List: Update ] SID %u EID %u Total %u num_updated_pages %u \r\n",
						//		stream_entry->stream_id, 
						//		sentry->epoch_id,
						//		stream_entry->epoch_list[cur_eidx].num_total_pages,
						//		stream_entry->epoch_list[cur_eidx].num_updated_pages);

					}
				//}
				//else if (sentry->epoch_id < stream_entry->last_epoch_id ) {
					//xil_printf("[JWDBG] %s: eid must be higher or equal to lasteid! epoch id: %d last eid: %d \n", __func__, g_barrierContext.stream[sidx].suspension_list[i].epoch_id, g_barrierContext.stream[sidx].last_epoch_id );
					//assert(0);
				} else {
					/* NOT MAPPED CASE 2 */
					if (is_first){
						first_unmapped_valid_idx = i;
						is_first = 0;
					}
				}
			}
			if (first_unmapped_valid_idx == -1){
				/* ALL MAPPED CASE. */
				stream_entry->head_idx = 0;
				stream_entry->tail_idx = 0;
				stream_entry->head_iter = 0;
				stream_entry->tail_iter = 0;
#if (PRINT_DEBUG_SP == 1)
				xil_printf("[MAPPED ALL] sid %u in %s\n", stream_entry->stream_id, __func__);
#endif
			} else {
				if (stream_entry->head_idx > first_unmapped_valid_idx) // head idx iter. 
					stream_entry->head_iter += 1;
				stream_entry->head_idx = first_unmapped_valid_idx;
#if (PRINT_DEBUG_SP == 1)
				xil_printf("[SEARCH] sid: %u hiter: %u hidx: %u titer: %u tidx: %u \n", 
					stream_entry->stream_id,
					stream_entry->head_iter, stream_entry->head_idx, 
					stream_entry->tail_iter, stream_entry->tail_idx );
#endif
			}
		}
		//xil_printf("[JWDBG] %s: aft enter shival!\n", __func__);
	}
	//xil_printf("[JWDBG] %s: ggut please !! l!\n", __func__);
}

void barrier_init()
{
	xil_printf("[ Barrier init Start. ]\r\n");
	memset((void*)&g_barrierContext, 0x00, sizeof(barrier_context_t));
	g_barrierContext.g_suspended_entry_cnt = 0;
	for (uint32_t sidx = 0; sidx < N_STREAM; sidx++)
		{
			g_barrierContext.stream[sidx].is_first = 1;
			g_barrierContext.stream[sidx].first_epoch_id = 0;
			g_barrierContext.stream[sidx].stream_id = 0;
			g_barrierContext.stream[sidx].valid_length = 0;
			g_barrierContext.stream[sidx].last_epoch_id = 0;
			g_barrierContext.stream[sidx].epoch_zero_done = 0;
			g_barrierContext.stream[sidx].head_idx = 0;
			g_barrierContext.stream[sidx].tail_idx = 0;
			g_barrierContext.stream[sidx].head_iter = 0;
			g_barrierContext.stream[sidx].tail_iter = 0;
			g_barrierContext.stream[sidx].suspended_entry_cnt = 0;

			for (uint32_t eidx = 0; eidx < N_EPOCH; eidx++)
			{
				//g_barrierContext.stream[sidx].epoch_list[eidx].num_updated_pages = INVALID_NUM_PAGES;
				g_barrierContext.stream[sidx].epoch_list[eidx].state = EPOCH_STATE_NONE;
				g_barrierContext.stream[sidx].epoch_list[eidx].epoch_id = 0;
				g_barrierContext.stream[sidx].epoch_list[eidx].num_total_pages = 0;
				g_barrierContext.stream[sidx].epoch_list[eidx].num_updated_pages = 0;
				g_barrierContext.stream[sidx].epoch_list[eidx].num_durable_pages = 0;
			}
			/* Initialize suspension array */
			for (uint32_t i = 0; i < N_SUSPENSION; i++) {
				g_barrierContext.stream[sidx].suspension_list[i].valid = 0;
				g_barrierContext.stream[sidx].suspension_list[i].lpn = 0;
				g_barrierContext.stream[sidx].suspension_list[i].ppn = 0;
				g_barrierContext.stream[sidx].suspension_list[i].epoch_id = 0;
				g_barrierContext.stream[sidx].suspension_list[i].pair_epoch_id = 0;
				g_barrierContext.stream[sidx].suspension_list[i].pair_stream_id = 0;
				//g_barrierContext.stream[sidx].suspension_list[i].pair_entry_pointer= NULL;
			}
		}
}
#endif
