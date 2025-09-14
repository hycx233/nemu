#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
	char expr[256];
	uint32_t old_value;

} WP;

WP* new_wp();
void free_wp(WP *wp);
WP* find_wp(int no);
void print_wp();
bool check_watchpoints();

#endif
