#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>

enum {
	NOTYPE = 256,
	EQ,			// 等于
	NUMBER,		// 数字
	PLUS,		// 加号
	MINUS,		// 减号
	MULTIPLY,	// 乘号
	DIVIDE,		// 除号
	LPAREN,		// 左括号
	RPAREN		// 右括号
	/* TODO: Add more token types */
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +", NOTYPE},			// spaces				" "
	{"\\+", PLUS},			// plus					"+"
	{"-", MINUS},			// minus				"-"
	{"\\*", MULTIPLY},		// multiply				"*"
	{"/", DIVIDE},			// divide				"/"
	{"\\(", LPAREN},			// left parenthesis		"("
	{"\\)", RPAREN},			// right parenthesis	")"
	{"[0-9]+", NUMBER},		// decimal number		"[0-9]+"
	{"==", EQ}				// equal				"=="
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
						tokens[nr_token].type = NUMBER;
						memset(tokens[nr_token].str, 0, sizeof(tokens[nr_token].str));
						// ensure not to overflow
						int copy_len = substr_len > 31 ? 31 : substr_len;
						strncpy(tokens[nr_token].str, substr_start, copy_len);
						tokens[nr_token].str[copy_len] = '\0';
						nr_token++;
						break;
					case PLUS:
					case MINUS:
					case MULTIPLY:
					case DIVIDE:
					case LPAREN:
					case RPAREN:
					case EQ:
						tokens[nr_token].type = rules[i].token_type;
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
			case EQ: pri = 1; break;
			case PLUS:
			case MINUS: pri = 2; break;
			case MULTIPLY:
			case DIVIDE: pri = 3; break;
			default: break;
		}
		// find the rightmost operator with the lowest precedence
		if (pri != -1 && pri <= min_pri) {
			min_pri = pri;
			op = i;
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
			return (uint32_t)strtoul(tokens[l].str, NULL, 10);
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
