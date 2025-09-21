#include "FLOAT.h"

FLOAT F_mul_F(FLOAT a, FLOAT b) {
	long long ret = (long long)a * b;
	return (FLOAT)(ret >> 16);
}

FLOAT F_div_F(FLOAT a, FLOAT b) {
	int op = 1;
	if ((a < 0) == 1) op *= -1,a = -a;
	if ((b < 0) == 1) op *= -1,b = -b;

	int ret = a / b;
	a %= b;

	int i;
	for (i = 0;i < 16;i ++){
		a <<= 1;
		ret <<= 1;
		if (a >= b) a -= b, ret |= 1;
	}
	return op * ret; 
}

FLOAT f2F(float a) {
	int b = *(int *)&a;
	int sign = b & 0x80000000;
	int exp = (b >> 23) & 0xff;
	int last = b & 0x7fffff;	
	
	if(exp == 255) {
		if (sign) return -0x7fffffff;
		else return 0x7fffffff;
	}
	
	if(exp == 0) return 0;

	last |= 1 << 23;
	exp -= 134;	
	if (exp < 0) last >>= -exp;
	if (exp > 0) last <<= exp;

	if (sign) return -last;else return last;
}

FLOAT Fabs(FLOAT a) {
	return a < 0 ? -a : a;
}

/* Functions below are already implemented */

FLOAT sqrt(FLOAT x) {
	FLOAT dt, t = int2F(2);

	do {
		dt = F_div_int((F_div_F(x, t) - t), 2);
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

FLOAT pow(FLOAT x, FLOAT y) {
	/* we only compute x^0.333 */
	FLOAT t2, dt, t = int2F(2);

	do {
		t2 = F_mul_F(t, t);
		dt = (F_div_F(x, t2) - t) / 3;
		t += dt;
	} while(Fabs(dt) > f2F(1e-4));

	return t;
}

