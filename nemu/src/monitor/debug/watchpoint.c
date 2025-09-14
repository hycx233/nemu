#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "nemu.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp() {
	if (free_ == NULL) {
		printf("No free watchpoint available\n");
		return NULL;
	}
	
	WP *wp = free_;
	free_ = free_->next;
	
	wp->next = head;
	head = wp;
	
	return wp;
}

void free_wp(WP *wp) {
	// Remove from active list
	if (head == wp) {
		head = head->next;
	} else {
		WP *prev = head;
		while (prev && prev->next != wp) {
			prev = prev->next;
		}
		if (prev) {
			prev->next = wp->next;
		}
	}
	
	// Add to free list
	wp->next = free_;
	free_ = wp;
}

WP* find_wp(int no) {
	WP *wp = head;
	while (wp) {
		if (wp->NO == no) {
			return wp;
		}
		wp = wp->next;
	}
	return NULL;
}

void print_wp() {
	WP *wp = head;
	if (!wp) {
		printf("No watchpoints.\n");
		return;
	}
	
	printf("Num     Type           Disp Enb Address    What\n");
	while (wp) {
		printf("%-8d%-15s%-5s%-4s%-11s%s\n", 
			wp->NO, "hw watchpoint", "keep", "y", "           ", wp->expr);
		wp = wp->next;
	}
}

bool check_watchpoints() {
	WP *wp = head;
	bool hit = false;
	
	while (wp) {
		bool success = true;
		uint32_t new_value = expr(wp->expr, &success);
		
		if (success && new_value != wp->old_value) {
			printf("Hardware watchpoint %d: %s\n", wp->NO, wp->expr);
			printf("Old value = 0x%08x\n", wp->old_value);
			printf("New value = 0x%08x\n", new_value);
			printf("Hint watchpoint %d at address 0x%08x\n", wp->NO, cpu.eip);
			wp->old_value = new_value;
			hit = true;
		}
		
		wp = wp->next;
	}
	
	return hit;
}
