/*******************************************************************************
 *License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>*
 *This is free software: you are free to change and redistribute it.           *
 *******************************************************************************/
#ifndef _EXPR_H_
#define _EXPR_H_
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef __unix__
#include <sys/types.h>
#else
#ifndef EXPR_HIDE_SSIZE_WARNING
#warning "__unix__ is not defined. define ssize_t as ptrdiff_t."
#endif
typedef ptrdiff_t ssize_t;
#define SSIZE_MAX PTRDIFF_MAX
#endif
enum expr_op {
EXPR_COPY=0,
EXPR_INPUT,
EXPR_CONST,
EXPR_BL,
EXPR_ADD,
EXPR_SUB,
EXPR_MUL,
EXPR_DIV,
EXPR_MOD,
EXPR_POW,
EXPR_AND,
EXPR_XOR,
EXPR_OR,
EXPR_SHL,
EXPR_SHR,
EXPR_GT,
EXPR_GE,
EXPR_LT,
EXPR_LE,
EXPR_SLE,
EXPR_SGE,
EXPR_SEQ,
EXPR_SNE,
EXPR_EQ,
EXPR_NE,
EXPR_ANDL,
EXPR_ORL,
EXPR_XORL,
EXPR_NEXT,
EXPR_DIFF,
EXPR_NEG,
EXPR_NOT,
EXPR_NOTL,
EXPR_TSTL,
EXPR_IF,
EXPR_WHILE,
EXPR_DO,
EXPR_DOW,
EXPR_WIF,
EXPR_DON,
EXPR_SUM,
EXPR_INT,
EXPR_PROD,
EXPR_SUP,
EXPR_INF,
EXPR_ANDN,
EXPR_ORN,
EXPR_XORN,
EXPR_GCDN,
EXPR_LCMN,
EXPR_LOOP,
EXPR_FOR,
EXPR_ZA,
EXPR_MD,
EXPR_ME,
EXPR_MEP,
EXPR_VMD,
EXPR_DO1,
EXPR_EP,
EXPR_EVAL,
EXPR_HOT,
EXPR_PBL,
EXPR_PZA,
EXPR_PMD,
EXPR_PME,
EXPR_PMEP,
EXPR_READ,
EXPR_WRITE,
EXPR_OFF,
EXPR_ALO,
EXPR_SJ,
EXPR_LJ,
EXPR_END
};
#define EXPR_VOID ((void *)-1UL)
#define EXPR_VOID_NR ((void *)-2UL)
#define EXPR_SYMSET_INITIALIZER {NULL,0UL,0UL,0UL,0}
#define EXPR_SYMLEN 64
#ifndef EXPR_SYMNEXT
#define EXPR_SYMNEXT 14
#endif
#define EXPR_ESYMBOL 1
#define EXPR_EPT 2
#define EXPR_EFP 3
#define EXPR_ENVP 4
#define EXPR_ENEA 5
#define EXPR_ENUMBER 6
#define EXPR_ETNV 7
#define EXPR_EEV 8
#define EXPR_EUO 9
#define EXPR_EZAFP 10
#define EXPR_EDS 11
#define EXPR_EVMD 12
#define EXPR_EMEM 13
#define EXPR_EUSN 14
#define EXPR_ENC 15
#define EXPR_ECTA 16
#define EXPR_ESAF 17
#define EXPR_EVD 18
#define EXPR_EPM 19

#define EXPR_CONSTANT 0
#define EXPR_VARIABLE 1
#define EXPR_FUNCTION 2
#define EXPR_MDFUNCTION 3
#define EXPR_MDEPFUNCTION 4
#define EXPR_HOTFUNCTION 5
#define EXPR_ZAFUNCTION 6
//expr symbol flag
#define EXPR_SF_INJECTION 1
#define EXPR_SF_WRITEIP 2
#define EXPR_SF_PMD 4
#define EXPR_SF_PME 8
#define EXPR_SF_UNSAFE 16

#define EXPR_SF_PEP 12
#define EXPR_SF_PMASK (~12)
//expr initial flag
#define EXPR_IF_NOOPTIMIZE 1
#define EXPR_IF_INSTANT_FREE 2

#define EXPR_IF_EXTEND_MASK (\
		EXPR_IF_INSTANT_FREE\
		)
#define EXPR_IF_NOBUILTIN 4
#define EXPR_IF_NOKEYWORD 8
#define EXPR_IF_PROTECT 16
#define EXPR_IF_INJECTION 32
#define EXPR_IF_INJECTION_S 64
//expr keyword flag
#define EXPR_KF_SUBEXPR 1
#define EXPR_KF_SEPCOMMA 2
#define EXPR_KF_NOPROTECT 4

#define EXPR_EDBASE(d) (((union expr_double *)(d))->rd.base)
#define EXPR_EDEXP(d) (((union expr_double *)(d))->rd.exp)
#define EXPR_EDSIGN(d) (((union expr_double *)(d))->rd.sign)
#define EXPR_EDIVAL(d) (((union expr_double *)(d))->ival)
#define expr_cast(x,type) \
	({\
		union {\
			__typeof(x) _x;\
			__typeof(type) _o;\
		} _un;\
		_un._x=(x);\
		_un._o;\
	})
struct expr;
struct expr_symset;
struct expr_suminfo {
	struct expr *fromep,*toep,*stepep,*ep;
	volatile double index;
};
struct expr_branchinfo {
	struct expr *cond,*body,*value;
};
struct expr_mdinfo {
	struct expr *eps;
	double *args;
	const char *e;
	union {
		double (*func)(size_t,double *);
		double (*funcep)(size_t,
			const struct expr *,double);
	} un;
	size_t dim;
};
struct expr_vmdinfo {
	struct expr *fromep,*toep,*stepep,*ep;
	ssize_t max;
	double (*func)(size_t,double *);
	double *args;
	volatile double index;
};
struct expr_rawdouble {
	uint64_t base:52;
	uint64_t exp:11;
	uint64_t sign:1;
} __attribute__((packed));
struct expr_inst {
	union {
		double *dst;
		int64_t *idst;
		void *uaddr;
		struct expr_rawdouble *rdst;
		double (**md2)(size_t,double *);
		double (**me2)(size_t,
			const struct expr *,double);
	} dst;
	union {
		double *src;
		int64_t *isrc;
		double **src2;
		void *uaddr;
		double value;
		ssize_t zd;
		struct expr *hotfunc;
		struct expr **hotfunc2;
		double (*func)(double);
		double (**func2)(double);
		double (*zafunc)(void);
		double (**zafunc2)(void);
		struct expr_suminfo *es;
		struct expr_branchinfo *eb;
		struct expr_mdinfo *em;
		struct expr_vmdinfo *ev;
	} un;
	enum expr_op op;
	int flag;
};
union expr_symvalue {
	double value;
	long ivalue;
	unsigned long uvalue;
	double *addr;
	void *uaddr;
	double (*func)(double);
	double (*zafunc)(void);
	char *hotexpr;
	double (*mdfunc)(size_t,double *);
	double (*mdepfunc)(size_t,
		const struct expr *,double);
};
struct expr_symbol {
	union expr_symvalue un;
	struct expr_symbol *next[EXPR_SYMNEXT];
	unsigned int length;
	unsigned short strlen;
	unsigned char type,flag;
	char str[];
};
struct expr_builtin_symbol {
	union expr_symvalue un;
	const char *str;
	unsigned short strlen;
	short type,flag,dim;
};
struct expr_builtin_keyword {
	const char *str;
	enum expr_op op;
	unsigned short flag;
	unsigned short strlen;
	const char *desc;
};
struct expr_symset {
	struct expr_symbol *syms;
	size_t size,depth,length;
	int freeable;
};
struct expr_resource {
	struct expr_resource *next;
	union {
		void *uaddr;
		double *addr;
		char *str;
		struct expr *ep;
	} un;
	int type;
};
struct expr {
	struct expr_inst *data;
	struct expr_inst *ip;
	struct expr *parent;
	double **vars;
	struct expr_symset *sset;
	struct expr_resource *res,*tail;
	size_t size,length,vsize,vlength;
	int error;
	short iflag;
	unsigned char freeable,sset_shouldfree;
	char errinfo[EXPR_SYMLEN];
};

union expr_double {
	double val;
	int64_t ival;
	uint64_t uval;
	struct expr_rawdouble rd;
};
extern const struct expr_builtin_symbol expr_symbols[];
extern const struct expr_builtin_keyword expr_keywords[];
const char *expr_error(int error);
uint64_t expr_gcd64(uint64_t x,uint64_t y);
double expr_gcd2(double x,double y);
double expr_lcm2(double x,double y);
void expr_mirror(double *buf,size_t size);
void expr_fry(double *v,size_t n);
void *expr_sort3(double *restrict v,size_t n,void *(*allocator)(size_t));
void expr_sort_old(double *restrict v,size_t n);
void expr_sort(double *v,size_t n);
double expr_and2(double x,double y);
double expr_or2(double x,double y);
double expr_xor2(double x,double y);
double expr_not(double x);
double expr_exp_old(double x);
void expr_contract(void *buf,size_t size);
__attribute__((noreturn)) void expr_explode(void);
double expr_isfinite(double x);
double expr_isinf(double x);
double expr_isnan(double x);
double expr_multilevel_derivate(const struct expr *ep,double input,long level,double epsilon);
const struct expr_builtin_symbol *expr_builtin_symbol_search(const char *sym,size_t sz);
const struct expr_builtin_symbol *expr_builtin_symbol_rsearch(void *addr);
size_t expr_strscan(const char *s,size_t sz,char *restrict buf);
char *expr_astrscan(const char *s,size_t sz,size_t *restrict outsz);
void expr_free(struct expr *restrict ep);
struct expr_inst *expr_addop(struct expr *restrict ep,double *dst,void *src,enum expr_op op,int flag);
void init_expr_symset(struct expr_symset *restrict esp);
struct expr_symset *new_expr_symset(void);
void expr_symset_free(struct expr_symset *restrict esp);
void expr_symset_wipe(struct expr_symset *restrict esp);
struct expr_symbol *expr_symset_add(struct expr_symset *restrict esp,const char *sym,int type,...);
struct expr_symbol *expr_symset_addl(struct expr_symset *restrict esp,const char *sym,size_t symlen,int type,...);
struct expr_symbol *expr_symset_vadd(struct expr_symset *restrict esp,const char *sym,int type,va_list ap);
struct expr_symbol *expr_symset_vaddl(struct expr_symset *restrict esp,const char *sym,size_t symlen,int type,va_list ap);
struct expr_symbol *expr_symset_addcopy(struct expr_symset *restrict esp,const struct expr_symbol *restrict es);
struct expr_symbol *expr_symset_search(const struct expr_symset *restrict esp,const char *sym,size_t sz);
struct expr_symbol *expr_symset_rsearch(const struct expr_symset *restrict esp,void *addr);
void expr_symset_copy(struct expr_symset *restrict dst,const struct expr_symset *restrict src);
struct expr_symset *expr_symset_clone(const struct expr_symset *restrict ep);
int expr_isconst(const struct expr *restrict ep);
int init_expr_const(struct expr *restrict ep,double val);
struct expr *new_expr_const(double val);
int init_expr7(struct expr *restrict ep,const char *e,size_t len,const char *asym,size_t asymlen,struct expr_symset *esp,int flag);
int init_expr5(struct expr *restrict ep,const char *e,const char *asym,struct expr_symset *esp,int flag);
int init_expr(struct expr *restrict ep,const char *e,const char *asym,struct expr_symset *esp);
struct expr *new_expr9(const char *e,size_t len,const char *asym,size_t asymlen,struct expr_symset *esp,int flag,int n,int *error,char errinfo[EXPR_SYMLEN]);
struct expr *new_expr7(const char *e,const char *asym,struct expr_symset *esp,int flag,int n,int *error,char errinfo[EXPR_SYMLEN]);
struct expr *new_expr8(const char *e,size_t len,const char *asym,size_t asymlen,struct expr_symset *esp,int flag,int *error,char errinfo[EXPR_SYMLEN]);
struct expr *new_expr6(const char *e,const char *asym,struct expr_symset *esp,int flag,int *error,char errinfo[EXPR_SYMLEN]);
struct expr *new_expr(const char *e,const char *asym,struct expr_symset *esp,int *error,char errinfo[EXPR_SYMLEN]);
double expr_calc5(const char *e,int *error,char errinfo[EXPR_SYMLEN],struct expr_symset *esp,int flag);
double expr_calc4(const char *e,int *error,char errinfo[EXPR_SYMLEN],struct expr_symset *esp);
double expr_calc3(const char *e,int *error,char errinfo[EXPR_SYMLEN]);
double expr_calc2(const char *e,int flag);
double expr_calc(const char *e);
double expr_eval(const struct expr *restrict ep,double input);
#endif
