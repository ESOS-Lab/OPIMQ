/*
 * barrier_ftl.h
 *
 *  Created on: 2021. 8. 10.
 *      Author: Jieun Kum, Suik Park
 */

#ifndef _BARRIER_FTL_H_
#define _BARRIER_FTL_H_

/*
 * barrier_ftl.c
 *
 *  Created on: 2021. 8. 10.
 *      Author: Jieun Kim, Suik Park
 */

typedef enum epoch_state
{
	EPOCH_STATE_NONE,
	EPOCH_STATE_ACTIVE_TRANSIENT_UNMAPPED, // write service (without barrier flag)
	EPOCH_STATE_CLOSED_TRANSIENT_UNMAPPED, // write service (with barrier flag)
	EPOCH_STATE_CLOSED_DURABLE_UNMAPPED, // all pages is flushed
	EPOCH_STATE_CLOSED_DURABLE_MAPPED, // all pages is mapped, persist
} epoch_state_t;

// epoch attribute


//EPOCH_STATE_NOT_ARRIVE_BARRIER
// data of epoch x arrived but barrier flag is not arrived yet.
//
//EPOCH_STATE_ARRIVE_BARRIER
// data of epoch x arrived with barrier flag
//
//EPOCH_STATE_HOLD_WITH_BARRIER
// data of epoch x arrived with barrier flag
// but, barrier flag of epoch x-1 is not arrived yet.
// when flush operation, in order epochs with barrier flag is flushed.
//EPOCH_STATE_COMPLETE


// if barrier flag with double stream, it may be not able to guarantee durable order.
//
// ex> sid1 1000, eid1 10, sid2 2000, eid2 20 with barrier flag
// if eid1 9 barrier was arrived and eid2 19 barrier was not arrived yet
// this data will be flushed for sid1 durable order.

#define N_STREAM 				128
#define N_EPOCH 				256
//#define N_SUSPENSION			256
//#define N_SUSPENSION			2048
//#define N_SUSPENSION			8192
#define N_SUSPENSION			16384
#define INVALID_EPOCH_ID		(0xFFFF)
#define INVALID_STREAM_ID		(0)

#define INVALID_NUM_PAGES		(0xFFFFFFFF)

typedef struct _mapping_wait_entry
{
	unsigned int valid;	// Does not needed if do not use insert sort.
	unsigned int lpn;
	unsigned int ppn;
	unsigned int epoch_id;
	unsigned int pair_stream_id; // Must be deleted
	unsigned int pair_epoch_id; // Must be delted
	uint8_t	pair_exist; // Pair exist. But does not mean pair is on suspension list. 1
	void* pair_entry_pointer; //mapping_wait_entry_t //pair exists and on the suspension list. 
} mapping_wait_entry_t;

typedef struct _epoch_entry
{
	epoch_state_t state;
	unsigned int epoch_id;
	unsigned int num_total_pages;
	unsigned int num_updated_pages; // mapping
	unsigned int num_durable_pages; // flush
} epoch_entry_t;

typedef struct _barrier_stream_entry
{
	uint8_t is_first;	// Juwon added. To handle when first sid is not 0. 
	uint8_t epoch_zero_done;  // For handling the case that eid 1 passes the condition last eid +1 == eid.
	unsigned int first_epoch_id; // Juwon added. To handle when first sid is not 0. 
	unsigned int stream_id;
	unsigned int valid_length;
	unsigned int last_epoch_id; // latest epoch_id to be persisted with barrier_flag
	unsigned int head_idx;
	unsigned int tail_idx;
	unsigned long head_iter;
	unsigned long tail_iter;
	unsigned int epoch_cnt;
	int suspended_entry_cnt;
	mapping_wait_entry_t suspension_list[N_SUSPENSION];
	epoch_entry_t epoch_list[N_EPOCH];

} barrier_stream_entry_t;

typedef struct _barrier_context
{
	uint32_t valid_stream_count;
	uint32_t valid_stream_bitmap[N_STREAM/32];
	barrier_stream_entry_t stream[N_STREAM];
	uint32_t g_suspended_entry_cnt;
} barrier_context_t;


//void barrier_push_epoch(unsigned int stream_id, unsigned int epoch_id);
//unsigned int barrier_pop_epoch(unsigned int stream_id);
//unsigned int barrier_get_epoch_count(unsigned int stream_idx);
//unsigned int barrier_get_stream_id(unsigned int stream_idx);
//void barrier_flush_operation();
//void barrier_init();

// Jieun add
uint32_t get_stream_index (unsigned int sid);
uint32_t get_epoch_index (unsigned int eid);
enum epoch_state barrier_check_and_set_epoch_state (uint32_t sid, uint32_t eid, uint32_t barrier, unsigned int page_cnt);
void barrier_set_epoch_state (uint32_t sid, uint32_t eid, uint32_t barrier, unsigned int page_cnt);
enum epoch_state barrier_check_prev_epoch_state(uint32_t sid, uint32_t eid);
void barrier_increase_total_page_count (unsigned int sid, unsigned int eid, unsigned int page_cnt);
unsigned int barrier_check_total_page_count (unsigned int sid, unsigned int eid);
void barrier_increase_durable_page_count(unsigned int sid, unsigned int eid, int page_cnt);
//void barrier_increase_updated_page_count(unsigned int sid, unsigned int eid, int page_cnt);
//void barrier_update_state(unsigned int sid, unsigned int eid);
void barrier_insert_suspension_array(unsigned int logicalSliceAddr, unsigned int virtualSliceAddr, unsigned int sid, unsigned int eid);
void barrier_insert_suspension_array_dual_stream(unsigned int logicalSliceAddr, unsigned int virtualSliceAddr, unsigned int sid1, unsigned int eid1, unsigned int sid2, unsigned int eid2, unsigned int mappable_case);
void barrier_search_suspension_list();
void barrier_search_suspension_list_dual_stream();

void barrier_update_double_stream(uint32_t sid1, uint32_t eid1, uint32_t sid2, uint32_t eid2);
void barrier_add_epoch_entry(uint32_t sid, uint32_t eid, uint32_t barrier);
void barrier_add_epoch_entry2(uint32_t sid, uint32_t eid, uint32_t barrier);
uint32_t barrier_pop_epoch_entry(uint32_t sidx);
uint32_t barrier_pop_epoch_entry2(uint32_t sidx);
enum epoch_state barrier_get_prev_epoch_state(unsigned int sid, unsigned int eid);
void barrier_flush_operation();
void barrier_add_page_count(unsigned int sid, unsigned int eid, unsigned int page_cnt);
void barrier_increase_durable_count(unsigned int sid, unsigned int eid);
void barrier_init();

//Juwon added
void update_epoch_info(uint32_t sid, uint32_t eid, void* func);
uint8_t is_mappable(uint32_t sid, uint32_t eid);


#endif /* _BARRIER_FTL_H_ */
