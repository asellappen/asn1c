#ifndef	_ASN1FIX_INTERNAL_H_
#define	_ASN1FIX_INTERNAL_H_

/*
 * System headers required in various modules.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>		/* isupper() */
#include <errno.h>
#include <assert.h>

#include <asn1parser.h>		/* Our lovely ASN.1 parser module */

/*
 * A definition of a function that will log error messages.
 */
typedef void (*error_logger_f)(int _is_fatal, const char *fmt, ...);

/*
 * Universal argument.
 */
typedef struct arg_s {
	asn1p_t         *asn;
	asn1p_module_t  *mod;
	asn1p_expr_t    *expr;
	error_logger_f   eh;
	error_logger_f   debug;
	void            *key;           /* The next level key */
} arg_t;

/*
 * Functions performing normalization of various types.
 */
#include "asn1fix_misc.h"		/* Support functions */
#include "asn1fix_value.h"		/* Value processing */
#include "asn1fix_cstring.h"		/* Fix cstring values */
#include "asn1fix_compat.h"		/* Data compatibility */
#include "asn1fix_constr.h"		/* Constructed types */
#include "asn1fix_class.h"		/* CLASS support */
#include "asn1fix_param.h"		/* Parametrization */
#include "asn1fix_retrieve.h"		/* Data retrieval */
#include "asn1fix_enum.h"		/* Process ENUMERATED */
#include "asn1fix_integer.h"		/* Process INTEGER */
#include "asn1fix_bitstring.h"		/* Process BIT STRING */
#include "asn1fix_dereft.h"		/* Dereference types */
#include "asn1fix_derefv.h"		/* Dereference values */
#include "asn1fix_tags.h"		/* Tags-related stuff */


/*
 * Merge the return value of the called function with the already
 * partially computed return value of the current function.
 */
#define	RET2RVAL(ret,rv) do {					\
		int __ret = ret;				\
		switch(__ret) {					\
		case  0: break;					\
		case  1: if(rv) break;				\
		case -1: rv = __ret; break;			\
		default:					\
			assert(__ret >= -1 && __ret <= 1);	\
			rv = -1;				\
		}						\
	} while(0)

/*
 * Temporary substitute module for the purposes of evaluating expression.
 */
#define	WITH_MODULE(tmp_mod, expr)	do {			\
		void *_saved_mod = arg->mod;			\
		arg->mod = tmp_mod;				\
		do { expr; } while(0);				\
		arg->mod = _saved_mod;				\
	} while(0)

#define	LOG(code, fmt, args...) do {				\
		int _save_errno = errno;			\
		if(code < 0) {					\
			if(arg->debug)				\
				arg->debug(code, fmt, ##args);	\
		} else {					\
			arg->eh(code, fmt " in %s", ##args,	\
				arg->mod->source_file_name);	\
		}						\
		errno = _save_errno;				\
	} while(0)

#define	DEBUG(fmt, args...)	LOG(-1, fmt, ##args)
#define	FATAL(fmt, args...)	LOG( 1, fmt, ##args)
#define	WARNING(fmt, args...)	LOG( 0, fmt, ##args)


/*
 * Define the symbol corresponding to the name of the current function.
 */
#if __STDC_VERSION__ < 199901
#if !(__GNUC__ == 2 && __GNUC_MINOR__ >= 7 || __GNUC__ >= 3)
#define __func__	(char *)0	/* Name of the current function */
#endif	/* GNUC */
/* __func__ is supposed to be defined */
#endif


#endif	/* _ASN1FIX_INTERNAL_H_ */
