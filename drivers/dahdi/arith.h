/*
 * Handy add/subtract functions to operate on chunks of shorts.
 * Feel free to add customizations for additional architectures
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#ifndef _DAHDI_ARITH_H
#define _DAHDI_ARITH_H

#ifdef CONFIG_DAHDI_MMX
#ifdef DAHDI_CHUNKSIZE
static inline void __ACSS(volatile short *dst, const short *src)
{
	__asm__ __volatile__ (
	        "movq 0(%0), %%mm0;\n"
	        "movq 0(%1), %%mm1;\n"
                "movq 8(%0), %%mm2;\n"
	        "movq 8(%1), %%mm3;\n"
	        "paddsw %%mm1, %%mm0;\n"
	        "paddsw %%mm3, %%mm2;\n"
                "movq %%mm0, 0(%0);\n"
                "movq %%mm2, 8(%0);\n"
	    : "=r" (dst)
	    : "r" (src), "0" (dst)
	    : "memory"
#ifdef CLOBBERMMX
	    , "%mm0", "%mm1", "%mm2", "%mm3"
#endif
      );

}
static inline void __SCSS(volatile short *dst, const short *src)
{
	__asm__ __volatile__ (
	        "movq 0(%0), %%mm0;\n"
	        "movq 0(%1), %%mm1;\n"
                "movq 8(%0), %%mm2;\n"
	        "movq 8(%1), %%mm3;\n"
	        "psubsw %%mm1, %%mm0;\n"
	        "psubsw %%mm3, %%mm2;\n"
                "movq %%mm0, 0(%0);\n"
                "movq %%mm2, 8(%0);\n"
	    : "=r" (dst)
	    : "r" (src), "0" (dst)
	    : "memory"
#ifdef CLOBBERMMX
	    , "%mm0", "%mm1", "%mm2", "%mm3"
#endif
      );

}

#if (DAHDI_CHUNKSIZE == 8)
#define ACSS(a,b) __ACSS(a,b)
#define SCSS(a,b) __SCSS(a,b)
#elif (DAHDI_CHUNKSIZE > 8)
static inline void ACSS(volatile short *dst, const short *src)
{
	int x;
	for (x=0;x<DAHDI_CHUNKSIZE;x+=8)
		__ACSS(dst + x, src + x);
}
static inline void SCSS(volatile short *dst, const short *src)
{
	int x;
	for (x=0;x<DAHDI_CHUNKSIZE;x+=8)
		__SCSS(dst + x, src + x);
}
#else
#error No MMX for DAHDI_CHUNKSIZE < 8
#endif
#endif
static inline int CONVOLVE(const int *coeffs, const short *hist, int len)
{
	int sum;
	/* Divide length by 16 */
	len >>= 4;
	
	/* Clear our accumulator, mm4 */
	
	/* 
	
	   For every set of eight...
	
	   Load 16 coefficients into four registers...
	   Shift each word right 16 to make them shorts...
	   Pack the resulting shorts into two registers...
	   With the coefficients now in mm0 and mm2, load the 
	        history into mm1 and mm3...
	   Multiply/add mm1 into mm0, and mm3 into mm2...
	   Add mm2 into mm0 (without saturation, alas).  Now we have two half-results.
	   Accumulate in mm4 (again, without saturation, alas)
	*/
	__asm__ (
		"pxor %%mm4, %%mm4;\n"
		"mov %1, %%edi;\n"
		"mov %2, %%esi;\n"
		"mov %3, %%ecx;\n"
		"1:"
			"movq  0(%%edi), %%mm0;\n"
			"movq  8(%%edi), %%mm1;\n"
			"movq 16(%%edi), %%mm2;\n"
			"movq 24(%%edi), %%mm3;\n"
			/* can't use 4/5 since 4 is the accumulator for us */
			"movq 32(%%edi), %%mm6;\n"
			"movq 40(%%edi), %%mm7;\n"
			"psrad $16, %%mm0;\n"
			"psrad $16, %%mm1;\n"
			"psrad $16, %%mm2;\n"
			"psrad $16, %%mm3;\n"
			"psrad $16, %%mm6;\n"
			"psrad $16, %%mm7;\n"
			"packssdw %%mm1, %%mm0;\n"
			"packssdw %%mm3, %%mm2;\n"
			"packssdw %%mm7, %%mm6;\n"
			"movq 0(%%esi), %%mm1;\n"
			"movq 8(%%esi), %%mm3;\n"
			"movq 16(%%esi), %%mm7;\n"
			"pmaddwd %%mm1, %%mm0;\n"
			"pmaddwd %%mm3, %%mm2;\n"
			"pmaddwd %%mm7, %%mm6;\n"
			"paddd %%mm6, %%mm4;\n"
			"paddd %%mm2, %%mm4;\n"
			"paddd %%mm0, %%mm4;\n"
			/* Come back and do for the last few bytes */
			"movq 48(%%edi), %%mm6;\n"
			"movq 56(%%edi), %%mm7;\n"
			"psrad $16, %%mm6;\n"
			"psrad $16, %%mm7;\n"
			"packssdw %%mm7, %%mm6;\n"
			"movq 24(%%esi), %%mm7;\n"
			"pmaddwd %%mm7, %%mm6;\n"
			"paddd %%mm6, %%mm4;\n"
			"add $64, %%edi;\n"
			"add $32, %%esi;\n"
			"dec %%ecx;\n"
		"jnz 1b;\n"
		"movq %%mm4, %%mm0;\n"
		"psrlq $32, %%mm0;\n"
		"paddd %%mm0, %%mm4;\n"
		"movd %%mm4, %0;\n"
		: "=r" (sum)
		: "r" (coeffs), "r" (hist), "r" (len)
		: "%ecx", "%edi", "%esi"
	);
		
	return sum;
}

static inline void UPDATE(volatile int *taps, const short *history, const int nsuppr, const int ntaps)
{
	int i;
	int correction;
	for (i=0;i<ntaps;i++) {
		correction = history[i] * nsuppr;
		taps[i] += correction;
	}
}

static inline void UPDATE2(volatile int *taps, volatile short *taps_short, const short *history, const int nsuppr, const int ntaps)
{
	int i;
	int correction;
#if 0
	ntaps >>= 4;
	/* First, load up taps, */
	__asm__ (
		"pxor %%mm4, %%mm4;\n"
		"mov %0, %%edi;\n"
		"mov %1, %%esi;\n"
		"mov %3, %%ecx;\n"
		"1:"
		"jnz 1b;\n"
		"movq %%mm4, %%mm0;\n"
		"psrlq $32, %%mm0;\n"
		"paddd %%mm0, %%mm4;\n"
		"movd %%mm4, %0;\n"
		: "=r" (taps), "=r" (taps_short)
		: "r" (history), "r" (nsuppr), "r" (ntaps), "0" (taps)
		: "%ecx", "%edi", "%esi"
	);
#endif
#if 1
	for (i=0;i<ntaps;i++) {
		correction = history[i] * nsuppr;
		taps[i] += correction;
		taps_short[i] = taps[i] >> 16;
	}
#endif	
}

static inline int CONVOLVE2(const short *coeffs, const short *hist, int len)
{
	int sum;
	/* Divide length by 16 */
	len >>= 4;
	
	/* Clear our accumulator, mm4 */
	
	/* 
	
	   For every set of eight...
	   Load in eight coefficients and eight historic samples, multliply add and
	   accumulate the result
	*/
	__asm__ (
		"pxor %%mm4, %%mm4;\n"
		"mov %1, %%edi;\n"
		"mov %2, %%esi;\n"
		"mov %3, %%ecx;\n"
		"1:"
			"movq  0(%%edi), %%mm0;\n"
			"movq  8(%%edi), %%mm2;\n"
			"movq 0(%%esi), %%mm1;\n"
			"movq 8(%%esi), %%mm3;\n"
			"pmaddwd %%mm1, %%mm0;\n"
			"pmaddwd %%mm3, %%mm2;\n"
			"paddd %%mm2, %%mm4;\n"
			"paddd %%mm0, %%mm4;\n"
			"movq  16(%%edi), %%mm0;\n"
			"movq  24(%%edi), %%mm2;\n"
			"movq 16(%%esi), %%mm1;\n"
			"movq 24(%%esi), %%mm3;\n"
			"pmaddwd %%mm1, %%mm0;\n"
			"pmaddwd %%mm3, %%mm2;\n"
			"paddd %%mm2, %%mm4;\n"
			"paddd %%mm0, %%mm4;\n"
			"add $32, %%edi;\n"
			"add $32, %%esi;\n"
			"dec %%ecx;\n"
		"jnz 1b;\n"
		"movq %%mm4, %%mm0;\n"
		"psrlq $32, %%mm0;\n"
		"paddd %%mm0, %%mm4;\n"
		"movd %%mm4, %0;\n"
		: "=r" (sum)
		: "r" (coeffs), "r" (hist), "r" (len)
		: "%ecx", "%edi", "%esi"
	);
		
	return sum;
}
static inline short MAX16(const short *y, int len, int *pos)
{
	int k;
	short max = 0;
	int bestpos = 0;
	for (k=0;k<len;k++) {
		if (max < y[k]) {
			bestpos = k;
			max = y[k];
		}
	}
	*pos = (len - 1 - bestpos);
	return max;
}



#else

#ifdef DAHDI_CHUNKSIZE
static inline void ACSS(short *dst, short *src)
{
	int x;

	/* Add src to dst with saturation, storing in dst */

#ifdef BFIN
	for (x = 0; x < DAHDI_CHUNKSIZE; x++)
		dst[x] = __builtin_bfin_add_fr1x16(dst[x], src[x]);
#else
	int sum;

	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		sum = dst[x] + src[x];
		if (sum > 32767)
			sum = 32767;
		else if (sum < -32768)
			sum = -32768;
		dst[x] = sum;
	}
#endif
}

static inline void SCSS(short *dst, short *src)
{
	int x;

	/* Subtract src from dst with saturation, storing in dst */
#ifdef BFIN
	for (x = 0; x < DAHDI_CHUNKSIZE; x++)
		dst[x] = __builtin_bfin_sub_fr1x16(dst[x], src[x]);
#else
	int sum;

	for (x = 0; x < DAHDI_CHUNKSIZE; x++) {
		sum = dst[x] - src[x];
		if (sum > 32767)
			sum = 32767;
		else if (sum < -32768)
			sum = -32768;
		dst[x] = sum;
	}
#endif
}

#endif	/* DAHDI_CHUNKSIZE */

static inline int CONVOLVE(const int *coeffs, const short *hist, int len)
{
	int x;
	int sum = 0;
	for (x=0;x<len;x++)
		sum += (coeffs[x] >> 16) * hist[x];
	return sum;
}

static inline int CONVOLVE2(const short *coeffs, const short *hist, int len)
{
	int x;
	int sum = 0;
	for (x=0;x<len;x++)
		sum += coeffs[x] * hist[x];
	return sum;
}

static inline void UPDATE(int *taps, const short *history, const int nsuppr, const int ntaps)
{
	int i;
	int correction;
	for (i=0;i<ntaps;i++) {
		correction = history[i] * nsuppr;
		taps[i] += correction;
	}
}

static inline void UPDATE2(int *taps, short *taps_short, const short *history, const int nsuppr, const int ntaps)
{
	int i;
	int correction;
	for (i=0;i<ntaps;i++) {
		correction = history[i] * nsuppr;
		taps[i] += correction;
		taps_short[i] = taps[i] >> 16;
	}
}

static inline short MAX16(const short *y, int len, int *pos)
{
	int k;
	short max = 0;
	int bestpos = 0;
	for (k=0;k<len;k++) {
		if (max < y[k]) {
			bestpos = k;
			max = y[k];
		}
	}
	*pos = (len - 1 - bestpos);
	return max;
}

#endif	/* MMX */
#endif	/* _DAHDI_ARITH_H */
