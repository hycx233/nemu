#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum {
	NOTYPE = 256,
	EQ,			// 等于
	NEQ,		// 不等于
	AND,		// 逻辑与
	OR,			// 逻辑或
	NOT,		// 逻辑非（单目运算符）
	NUMBER,		// 数字
	PLUS,		// 加号
	MINUS,		// 减号
	NEG,		// 负号（单目运算符）
	MULTIPLY,	// 乘号
	DIVIDE,		// 除号
	LPAREN,		// 左括号
	RPAREN,		// 右括号
	REGISTER	// 寄存器
	/* TODO: Add more token types */
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +", NOTYPE},								// spaces
	{"&&", AND},								// logical and
	{"\\|\\|", OR},								// logical or  
	{"==", EQ},									// equal
	{"!=", NEQ},								// not equal
	{"!", NOT},									// logical not
	{"\\+", PLUS},								// plus
	{"-", MINUS},								// minus
	{"\\*", MULTIPLY},							// multiply
	{"/", DIVIDE},								// divide
	{"\\(", LPAREN},							// left parenthesis
	{"\\)", RPAREN},							// right parenthesis
	{"\\$[a-zA-Z][a-zA-Z0-9]*", REGISTER},		// register like "$eax", "$ebx"
	{"0[xX][0-9a-fA-F]+", NUMBER},				// hexadecimal number like "0x1F"
	{"[0-9]+", NUMBER}							// decimal number
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

// Get register value by register name
static uint32_t get_register_value(const char *reg_name, bool *success) {
	// Skip the leading '$' symbol
	const char *name = reg_name + 1;
	int i;
	
	// Check special register first
	if (strcmp(name, "eip") == 0) {
		return cpu.eip;
	}
	
	// Check 32-bit registers
	for (i = 0; i < 8; i++) {
		if (strcmp(name, regsl[i]) == 0) {
			return reg_l(i);
		}
	}
	
	// Check 16-bit registers  
	for (i = 0; i < 8; i++) {
		if (strcmp(name, regsw[i]) == 0) {
			return reg_w(i);
		}
	}
	
	// Check 8-bit registers
	for (i = 0; i < 8; i++) {
		if (strcmp(name, regsb[i]) == 0) {
			return reg_b(i);
		}
	}
	
	// Invalid register name
	*success = false;
	return 0;
}

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
					case NOTYPE:
						// ignore spaces
						break;
					case NUMBER:
					case REGISTER:
						tokens[nr_token].type = rules[i].token_type;
						memset(tokens[nr_token].str, 0, sizeof(tokens[nr_token].str));
						// ensure not to overflow
						int copy_len = substr_len > 31 ? 31 : substr_len;
						strncpy(tokens[nr_token].str, substr_start, copy_len);
						tokens[nr_token].str[copy_len] = '\0';
						nr_token++;
						break;
					case PLUS:
					case MULTIPLY:
					case DIVIDE:
					case LPAREN:
					case RPAREN:
					case EQ:
					case NEQ:
					case AND:
					case OR:
					case NOT:
						tokens[nr_token].type = rules[i].token_type;
						tokens[nr_token].str[0] = '\0';
						nr_token++;
						break;
					case MINUS:
						// Check if it's negative sign or minus operator
						// It's negative if previous token is not number/register/right parenthesis
						if (nr_token == 0 || 
							(tokens[nr_token-1].type != NUMBER && tokens[nr_token-1].type != RPAREN && tokens[nr_token-1].type != REGISTER)) {
							tokens[nr_token].type = NEG;
						} else {
							tokens[nr_token].type = MINUS;
						}
						tokens[nr_token].str[0] = '\0';
						nr_token++;
						break;
					default: panic("please implement me");
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true;
}

static bool check_parentheses(int l, int r, bool *success) {
	if (tokens[l].type != LPAREN || tokens[r].type != RPAREN) return false;

	int balance = 0;
	int i;
	for (i = l; i <= r; i++) {
		if (tokens[i].type == LPAREN) balance++;
		else if (tokens[i].type == RPAREN) balance--;

		if (balance == 0 && i < r) return false;
		if (balance < 0) {
			*success = false;
			return false;
		}
	}

	if (balance != 0) {
		*success = false;
		return false;
	}

	return true;
}

static int find_dominant_op(int l, int r, bool *success) {
	int min_pri = 100, op = -1, balance = 0;
	int i;
	for (i = l; i <= r; i++) {
		if (tokens[i].type == LPAREN) balance++;
		else if (tokens[i].type == RPAREN) balance--;
		if (balance != 0) continue; // skip tokens inside parentheses

		int pri = -1;
		switch (tokens[i].type) {
			case OR: pri = 0; break;   // || has lowest precedence
			case AND: pri = 1; break;  // && has higher precedence than ||
			case EQ:
			case NEQ: pri = 2; break;  // == != have higher precedence than logical ops
			case PLUS:
			case MINUS: pri = 3; break;
			case MULTIPLY:
			case DIVIDE: pri = 4; break;
			case NEG:
			case NOT: pri = 5; break; // unary operators have highest precedence
			default: break;
		}
		// Find the rightmost operator with the lowest precedence
		if (pri != -1) {
			if (tokens[i].type == NEG) {
				// For unary operators, find the rightmost one
				if (pri <= min_pri) {
					min_pri = pri;
					op = i;
				}
			} else {
				// For binary operators, find the rightmost one  
				if (pri <= min_pri) {
					min_pri = pri;
					op = i;
				}
			}
		}
	}
	if (op == -1) {
		*success = false;
	}
	return op;
}

static uint32_t eval(int l, int r, bool *success) {
	if (!*success) {
		return 0;
	}

	if (l > r) {
		*success = false;
		return 0;
	}

	if (l == r) {
		if (tokens[l].type == NUMBER) {
			// Detect base: hex if starts with 0x/0X, otherwise decimal
			int base = 10;
			if (strlen(tokens[l].str) >= 2 && 
				tokens[l].str[0] == '0' && 
				(tokens[l].str[1] == 'x' || tokens[l].str[1] == 'X')) {
				base = 16;
			}
			return (uint32_t)strtoul(tokens[l].str, NULL, base);
		} else if (tokens[l].type == REGISTER) {
			return get_register_value(tokens[l].str, success);
		} else {
			*success = false;
			return 0;
		}
	}

	if (check_parentheses(l, r, success)) {
		return eval(l + 1, r - 1, success);
	}

	int op = find_dominant_op(l, r, success);
	if (!*success) return 0;
	
	// Handle unary operators
	if (tokens[op].type == NEG) {
		uint32_t val = eval(op + 1, r, success);
		if (!*success) return 0;
		return -val;
	}
	
	if (tokens[op].type == NOT) {
		uint32_t val = eval(op + 1, r, success);
		if (!*success) return 0;
		return !val;  // logical NOT: 0 if val != 0, 1 if val == 0
	}
	
	uint32_t val1 = eval(l, op - 1, success);
	if (!*success) return 0;
	uint32_t val2 = eval(op + 1, r, success);
	if (!*success) return 0;

	switch (tokens[op].type) {
		case PLUS: return val1 + val2;
		case MINUS: return val1 - val2;
		case MULTIPLY: return val1 * val2;
		case DIVIDE:
			if (val2 == 0) {
				*success = false;
				return 0;
			}
			return val1 / val2;
		case EQ: return val1 == val2;
		case NEQ: return val1 != val2;
		case AND: return val1 && val2;  // logical AND: 1 if both non-zero, 0 otherwise
		case OR: return val1 || val2;   // logical OR: 1 if either non-zero, 0 if both zero
		default:
			*success = false;
			return 0;
	}
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}
	*success = true;
	return eval(0, nr_token - 1, success);
}
