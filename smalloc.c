#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include "smalloc.h"

sm_container_ptr sm_first = 0x0 ;
sm_container_ptr sm_last = 0x0 ;
sm_container_ptr sm_unused_containers = 0x0 ;

void sm_container_split(sm_container_ptr hole, size_t size)
{
	sm_container_ptr remainder = hole->data + size ;

	remainder->data = ((void *)remainder) + sizeof(sm_container_t) ;
	remainder->dsize = hole->dsize - size - sizeof(sm_container_t) ;
	remainder->status = Unused ;
	remainder->next = hole->next ;
	hole->next = remainder ;

	if (hole == sm_last)
		sm_last = remainder ;
}

void * sm_retain_more_memory(int size)
{
	sm_container_ptr hole ;
	int pagesize = getpagesize() ;
	int n_pages = 0 ;

	n_pages = (sizeof(sm_container_t) + size + sizeof(sm_container_t)) / pagesize  + 1 ;
	hole = (sm_container_ptr) sbrk(n_pages * pagesize) ;
	if (hole == 0x0)
		return 0x0 ;

	hole->data = ((void *) hole) + sizeof(sm_container_t) ;
	hole->dsize = n_pages * getpagesize() - sizeof(sm_container_t) ;
	hole->status = Unused ;

	return hole ;
}

void * smalloc(size_t size)
{
	sm_container_ptr hole = 0x0 ;

	sm_container_ptr itr = 0x0, best_fit = 0x0 ;
	for (itr = sm_unused_containers ; itr != 0x0 ; itr = itr->next_unused) {
		sm_container_ptr temp = 0x0;
		if (size == itr->dsize) {
			// a hole of the exact size (best case)
			temp = itr->next_unused;
			itr->next_unused = 0x0;
			itr->status = Busy ;
			itr->prev_unused->next_unused = temp;
			return itr->data ;
		}
		else if (size + sizeof(sm_container_t) < itr->dsize) {
			if(best_fit == 0x0)
				best_fit = itr;

			// a hole large enought to split
			if(best_fit->dsize > itr->dsize){
				best_fit = itr;
			}
		}
	}
	hole = best_fit;

	bool new_element = false;
	if (hole == 0x0) {
		hole = sm_retain_more_memory(size) ;
		new_element = true;

		if (hole == 0x0)
			return 0x0 ;

		if (sm_first == 0x0) {
			sm_first = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
		else {
			sm_last->next = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
	}
	sm_container_split(hole, size) ; // hole->next is the unused one
	hole->dsize = size ;
	hole->status = Busy ;

	/* Task 2-1 */
	if(new_element == true){ // new memory is allocated
		sm_container_ptr now = 0x0;
		sm_container_ptr prev = 0x0;
		if(sm_unused_containers == 0x0){
			sm_unused_containers = hole->next;
			hole->next->next_unused = 0x0;
			hole->next->prev_unused = sm_unused_containers;
		}else{
			now = sm_unused_containers;
			while(now->next_unused != 0x0){
				prev = now;
				now = now->next_unused;
				now->prev_unused = prev;
			}
			now->next_unused = hole->next;
			hole->next->next_unused = 0x0;
			hole->next->prev_unused = now;
		}
	}else{ // existing memory is allocated
		sm_container_ptr now = sm_unused_containers;
		bool matching = false;
		while(now->next_unused != 0x0){
			if(now == hole){
				matching = true;
				break;
			}
			now = now->next_unused;
		}

		sm_container_ptr temp = 0x0;
		if(matching == true){
			temp = now->next_unused;
			now->next_unused = 0x0;
			now->prev_unused->next_unused = hole->next;
			hole->next->next_unused = temp;
		}
	}


	return hole->data ;
}



void sfree(void * p)
{
	sm_container_ptr itr , temp, prev;
	for (itr = sm_first ; itr->next != 0x0 ; itr = itr->next) {
		if (itr->data == p) {
			itr->status = Unused ;
			temp = sm_unused_containers;
			while(temp->next_unused != 0x0){
				temp = temp->next_unused;
			}
			temp->next_unused = itr;
			itr->next_unused = 0x0;
			itr->prev_unused = temp;
			break ;
		}
	}

	// /* Task 2-2 */
	// itr = sm_first;
	// while(itr->next != 0x0){
	// 	if (itr->data == p){
	// 		break;
	// 	}
	// 	prev = itr;
	// 	itr = itr->next;
	// }
	//
	// if((prev + prev->dsize == itr) && (prev->status == Unused) && (itr->status == Unused)){
	// 	prev->dsize = prev-> dsize + sizeof(sm_container_t) + itr->dsize;
	// 	prev->next = itr->next;
	// }else if((itr + itr->dsize == itr->next) && (itr->status == Unused) && (itr->next->status == Unused)){
	// 	itr->dsize = itr->dsize + sizeof(sm_container_t) + itr->next->dsize;
	// 	itr->next = itr->next->next;
	// }

}


/* Task 1.1 */
void print_sm_uses(){
	size_t total_mem_size = 0, busy_mem_size = 0, unused_mem_size = 0;
	sm_container_ptr itr = 0x0 ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next) {
		total_mem_size += itr->dsize + sizeof(sm_container_t);
		if(itr->status == Busy){
			busy_mem_size += itr->dsize + sizeof(sm_container_t);
		}else if(itr->status == Unused){
			unused_mem_size += itr->dsize + sizeof(sm_container_t);
		}
	}
	printf("\n==================== sm_uses ====================\n") ;
	fprintf(stderr, "\tTotal: %zu, Busy: %zu, Unused: %zu\n", total_mem_size, busy_mem_size, unused_mem_size);
	printf("=================================================\n\n") ;
}



void print_sm_containers()
{
	sm_container_ptr itr ;
	int i = 0 ;

	printf("==================== sm_containers ====================\n") ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next, i++) {
		char * s ;
		printf("%3d:%p:%s:", i, itr->data, itr->status == Unused ? "Unused" : "  Busy") ;
		printf("%8d:", (int) itr->dsize) ;

		for (s = (char *) itr->data ;
			 s < (char *) itr->data + (itr->dsize > 8 ? 8 : itr->dsize) ;
			 s++)
			printf("%02x ", *s) ;
		printf("\n") ;
	}
	printf("=======================================================\n") ;

}
