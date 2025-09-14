#include <inttypes.h>

#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_si(char *args) {
	uint32_t step = 1;
	if (args != NULL) {
		if (sscanf(args, "%u", &step) != 1 || step == 0) {
			printf("Invalid number of steps: %s\n", args);
			return 0;
		}
	}
	cpu_exec(step);
	return 0;
}

static int cmd_info(char *args) {
	// print registers when args is "r"
	if (args == NULL) {
		printf("Usage: info r/w\n");
		return 0;
	}

	if (strcmp(args, "r") == 0) {
		// Print registers
		int i;
		for (i = 0; i < 8; i++) {
			printf("%s: 0x%08x (%u)\n", regsl[i], reg_l(i), reg_l(i));
		}
		// Print eip and eflags
		printf("eip: 0x%08x (%u)\n", cpu.eip, cpu.eip);
		printf("eflags: 0x%08x (%u)\n", cpu.eflags.val, cpu.eflags.val);
		// Print individual flags
		printf("CF: %d\n", cpu.eflags.CF);
		printf("PF: %d\n", cpu.eflags.PF);
		printf("AF: %d\n", cpu.eflags.AF);
		printf("ZF: %d\n", cpu.eflags.ZF);
		printf("SF: %d\n", cpu.eflags.SF);
		printf("TF: %d\n", cpu.eflags.TF);
		printf("IF: %d\n", cpu.eflags.IF);
		printf("DF: %d\n", cpu.eflags.DF);
		printf("OF: %d\n", cpu.eflags.OF);
		printf("IOPL: %d\n", cpu.eflags.IOPL);
		printf("NT: %d\n", cpu.eflags.NT);
	} else if (strcmp(args, "w") == 0) {
		// Print watchpoints
		print_wp();
	} else {
		printf("Unknown argument '%s'\n", args);
	}

	return 0;
}

static int cmd_x(char *args) {
	if (args == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	char *n_str = strtok(args, " ");
	char *expr_str = strtok(NULL, "");

	if (n_str == NULL || expr_str == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	int n = atoi(n_str);
	if (n <= 0) {
		printf("Invalid number of words: %s\n", n_str);
		return 0;
	}
	uint32_t addr;

	bool success;
	addr = expr(expr_str, &success);
	if (!success) {
		printf("Invalid expression: %s\n", expr_str);
		return 0;
	}

	int i;
	for (i = 0; i < n; i++) {
		uint32_t data = swaddr_read(addr + i * 4, 4);
		printf("0x%08x: 0x%08x\n", addr + i * 4, data);
	}

	return 0;
}

static int cmd_p(char *args) {
	if (args == NULL) {
		printf("Usage: p EXPR\n");
		return 0;
	}

	bool success;
	uint32_t result = expr(args, &success);
	if (!success) {
		printf("Invalid expression: %s\n", args);
		return 0;
	}

	printf("%u\n", result);
	return 0;
}

static int cmd_w(char *args) {
	if (args == NULL) {
		printf("Usage: w EXPR\n");
		return 0;
	}
	
	// Test if expression is valid
	bool success;
	uint32_t value = expr(args, &success);
	if (!success) {
		printf("Invalid expression: %s\n", args);
		return 0;
	}
	
	// Create new watchpoint
	WP *wp = new_wp();
	if (wp == NULL) {
		return 0;
	}
	
	// Store expression and initial value
	strcpy(wp->expr, args);
	wp->old_value = value;
	
	printf("Hardware watchpoint %d: %s\n", wp->NO, wp->expr);
	return 0;
}

static int cmd_d(char *args) {
	if (args == NULL) {
		printf("Usage: d N\n");
		return 0;
	}
	
	int no = atoi(args);
	WP *wp = find_wp(no);
	if (wp == NULL) {
		printf("No watchpoint number %d.\n", no);
		return 0;
	}
	
	free_wp(wp);
	printf("Delete watchpoint %d.\n", no);
	return 0;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "Step N instructions", cmd_si },
	{ "info", "Print the register/watchpoint state", cmd_info },
	{ "x", "Scan memory", cmd_x },
	{ "p", "Evaluate expression", cmd_p },
	{ "w", "Set watchpoint", cmd_w },
	{ "d", "Delete watchpoint", cmd_d },

	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
