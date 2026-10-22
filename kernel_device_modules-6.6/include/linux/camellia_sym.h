/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Optional cross-module hooks for camellia (panel <-> touch, leds <-> backlight IC).
 *
 * __weak references to symbols in other modules compile to GOT relocations
 * (R_AARCH64_ADR_GOT_PAGE), which the arm64 module loader rejects. Look the
 * symbol up at call time instead; it must be EXPORT_SYMBOL_GPL (6.6 symbol_get
 * only resolves GPL exports). Absent providers read as 0 / are skipped.
 */
#ifndef _CAMELLIA_SYM_H
#define _CAMELLIA_SYM_H

#include <linux/module.h>

/* Call sym(args) if loaded; evaluates to true if it was called. */
#define CAMELLIA_SYM_CALL(sym, ...) ({				\
	typeof(&sym) __f = symbol_get(sym);			\
	if (__f) {						\
		__f(__VA_ARGS__);				\
		symbol_put(sym);				\
	}							\
	__f != NULL;						\
})

/* Return sym(args) if loaded, else def. */
#define CAMELLIA_SYM_CALL_RET(sym, def, ...) ({			\
	typeof(&sym) __f = symbol_get(sym);			\
	typeof(sym(__VA_ARGS__)) __r = (def);			\
	if (__f) {						\
		__r = __f(__VA_ARGS__);				\
		symbol_put(sym);				\
	}							\
	__r;							\
})

/* Value of variable sym if loaded, else 0. */
#define CAMELLIA_SYM_READ(sym) ({				\
	typeof(&sym) __p = symbol_get(sym);			\
	typeof(sym) __v = 0;					\
	if (__p) {						\
		__v = *__p;					\
		symbol_put(sym);				\
	}							\
	__v;							\
})

#endif
