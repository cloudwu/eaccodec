#include <stdint.h>
#include <string.h>
#include "eaccodec.h"

//#define DEBUG_LOG

static const int alpha_modifier[16][8] = {
	{-3,-6,-9 ,-15,2,5,8,14}, //0  
	{-3,-7,-10,-13,2,6,9,12}, //1  
	{-2,-5,-8 ,-13,1,4,7,12}, //2  
	{-2,-4,-6 ,-13,1,3,5,12}, //3  
	{-3,-6,-8 ,-12,2,5,7,11}, //4  
	{-3,-7,-9 ,-11,2,6,8,10}, //5  
	{-4,-7,-8 ,-11,3,6,7,10}, //6  
	{-3,-5,-8 ,-11,2,4,7,10}, //7  
	{-2,-6,-8 ,-10,1,5,7,9 }, //8  
	{-2,-5,-8 ,-10,1,4,7,9 }, //9  
	{-2,-4,-8 ,-10,1,3,7,9 }, //10 
	{-2,-5,-7 ,-10,1,4,6,9 }, //11 
	{-3,-4,-7 ,-10,2,3,6,9 }, //12 
	{-1,-2,-3 ,-10,0,1,2,9 }, //13 
	{-4,-6,-8 ,-9 ,3,5,7,8 }, //14 
	{-3,-5,-7 ,-9 ,2,4,6,8 }, //15 
};

struct alpha_stat {
	uint8_t alpha[16][2];
	int number;
};

static void
count_alpha(const uint8_t alpha[16], struct alpha_stat *stat) {
	uint8_t c[256];
	memset(c, 0, sizeof(c));
	int i;
	int number = 0;
	for (i=0;i<16;i++) {
		++c[alpha[i]];
	}
	for (i=0;i<256;i++) {
		if (c[i] > 0) {
			stat->alpha[number][0] = i;
			stat->alpha[number][1] = c[i];
			++number;
		}
	}
	stat->number = number;
}

static inline void
fill_bits(uint8_t data[8], uint64_t v) {
	data[2] = (v >> 40) & 0xff;
	data[3] = (v >> 32) & 0xff;
	data[4] = (v >> 24) & 0xff;
	data[5] = (v >> 16) & 0xff;
	data[6] = (v >> 8) & 0xff;
	data[7] = (v >> 0) & 0xff;
}

static void
two_alpha(const uint8_t alpha[16], uint8_t data[0], uint8_t c1, uint8_t c2) {
	int multiplier, idx1, idx2, base;
	// use table 0
	if (c1 == 0) {
		idx1 = 3;	// use table[0][3] as c1  (-15)
		if (c2 >= 14) {
			idx2 = 7;	// use table[0][7] as c2 (14)
			multiplier = c2 / 14;	// max of table 0 is 14
			if (multiplier > 15)
				multiplier = 15;
			base = c2 - 14 * multiplier;
		} else {
			idx2 = 4;	// use table[0][4] as c2 (2)
			multiplier = c2 / 2;
			base = c2 - 2 * multiplier;
		}
	} else {
		// c2 == 255
		idx2 = 7;	// use table[0][7] as c2 (14)
		if (c1 <= 255 - 15) {
			idx1 = 3;	// use table[0][3] as c1 (-15)
			multiplier = (255 - c1) / 15;
			if (multiplier > 15)
				multiplier = 15;
			base = c1 + 15 * multiplier;
		} else {
			idx1 = 0;	// use table[0][0] as c1 (-3)
			multiplier = (255 - c1) / 3;
			base = c1 + 3 * multiplier;
		}
	}
	data[0] = base;
	data[1] = multiplier << 4;
	uint64_t v = 0;
	int i;
	for (i=0;i<16;i++) {
		if (alpha[i] == c1) {
			v |= idx1;
		} else {
			v |= idx2;
		}
		v <<= 3;
	}
	fill_bits(data, v);
}

struct alpha_args {
	int table;
	int multiplier;
	int base;
	int diff;
	uint8_t index[16];
};

static inline int
clamp(int v) {
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return v;
}

static void
calc_diff(const struct alpha_stat *stat, struct alpha_args *args) {
	static const int index_order[8] = { 3,2,1,0,4,5,6,7 };
	int i;
	int idx = 0;
	const int * modifier = alpha_modifier[args->table];
	int diff = 0;
	for (i=0;i<stat->number;i++) {
		int alpha = stat->alpha[i][0];
		int v = clamp(args->base + modifier[index_order[idx]] * args->multiplier) - alpha;
		v *= v;
		while (idx < 7) {
			int v2 = clamp(args->base + modifier[index_order[idx+1]] * args->multiplier) - alpha;
			v2 *= v2;
			if (v2 < v) {
				v = v2;
				++idx;
			} else {
				break;
			}
		}
		args->index[i] = index_order[idx];
		diff += v * stat->alpha[i][1];
		if (diff >= args->diff)
			return;
	}
	args->diff = diff;
#ifdef DEBUG_LOG
	printf("calc diff : table = %d, multiplier = %d, base = %d, diff=%d\n", 
		args->table, args->multiplier, args->base, diff);
	for (i=0;i<stat->number;i++) {
		printf("\t%d * %d : (%d/%d)\n", stat->alpha[i][0], stat->alpha[i][1], 
			modifier[args->index[i]], clamp(args->base + modifier[args->index[i]] * args->multiplier));
	}
#endif
}

static void
map_alpha(const uint8_t alpha[16], const struct alpha_stat *stat, const struct alpha_args *args, uint8_t data[8]) {
	data[0] = args->base;
	data[1] = args->multiplier << 4 | args->table;
	uint64_t v = 0;
	int i,j;
	for (i=0;i<16;i++) {
		int a = alpha[i];
		v <<= 3;
		for (j=0;j<stat->number;j++) {
			if (a == stat->alpha[j][0]) {
				v |= args->index[j];
				break;
			}
		}
	}
	fill_bits(data, v);
}

// See: https://www.khronos.org/registry/DataFormat/specs/1.1/dataformat.1.1.html#Section-r11eac
// 8 bytes encode as big endian
// 8bit codebase (first byte), 4bit multiplier, 4bit table index [0,15], 48bit 4x4 index, 3bits per pixel.

void
eac_encode(const uint8_t alpha[16], uint8_t data[8]) {
	struct alpha_stat stat;
	count_alpha(alpha, &stat);
	if (stat.number == 1) {
		// only one alpha
		data[0] = stat.alpha[0][0];
		memset(data+1, 0, 7); // multiplier is 0, table index no used
		return;
	}
	if (stat.number == 2) {
		uint8_t c1 = stat.alpha[0][0];
		uint8_t c2 = stat.alpha[1][0];
		if (c1 == 0 || c2 == 255) {
			// some special cases
			two_alpha(alpha, data, c1, c2);
			return;
		}
	}
	int dist = stat.alpha[stat.number-1][0] - stat.alpha[0][0];
	struct alpha_args args, best;
	memset(&best, 0, sizeof(best));
	args.diff = best.diff = 0x7fffffff;	// huge diff
	for (args.table = 0;args.table < 16; args.table++ ) {
		int modifier_dist = alpha_modifier[args.table][7] - alpha_modifier[args.table][3];
		int multiplier_start = (dist + modifier_dist - 1)/modifier_dist;
		int multiplier_end = multiplier_start * 2;
		if (multiplier_end > 16)
			multiplier_end = 16;
		for (args.multiplier = multiplier_start; args.multiplier < multiplier_end; args.multiplier++) {
			int base_start = (dist+1)/2 + stat.alpha[0][0] - 4 * args.multiplier;
			int base_end = base_start + 8 * args.multiplier;
			if (base_start < 0)
				base_start = 0;
			if (base_end > 256)
				base_end = 256;
			for (args.base = base_start; args.base < base_end; args.base++) {
				calc_diff(&stat, &args);
				if (args.diff < best.diff) {
					best = args;
				}
			}
		}
	}
	map_alpha(alpha, &stat, &best, data);
}

void
eac_decode(const uint8_t data[8], uint8_t alpha[16]) {
	int base = data[0];
	int multiplier = data[1] >> 4;
	int table = data[1] & 0xf;
#ifdef DEBUG_LOG
	printf("base = %d, multiplier = %d, table = %d\n", base, multiplier, table);
#endif
	int i;
	uint64_t v = 0;
	for (i=2;i<8;i++) {
		v <<= 8;
		v |= data[i];
	}
	const int * modifier = alpha_modifier[table];
	for (i=0;i<16;i++) {
		int idx = (v >> (15 - i) * 3) & 7;
		alpha[i] = clamp(base + modifier[idx] * multiplier);
	}
}
