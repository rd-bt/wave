#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <err.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <getopt.h>
#include <stdarg.h>
#include "bitmap.h"
#include "expr.h"
struct expr_symset *es=NULL;
void sdtime(double dsec){
	struct timespec ts;
	ts.tv_sec=(time_t)dsec;
	ts.tv_nsec=(time_t)((dsec-(double)(ts.tv_sec))*1000000000);
	clock_nanosleep(CLOCK_REALTIME,0,&ts,NULL);
}
double dtime(void){
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME,&ts);
	return (double)ts.tv_sec+ts.tv_nsec/1000000000.0;
}
int xopen(const char *path){
	int r;
	r=open(path,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
	if(r<0){
		err(EXIT_FAILURE,"cannot open \"%s\"",path);
	}
	return r;
}
void *xmalloc(size_t size){
	void *r;
	r=malloc(size);
	if(!r){
		err(EXIT_FAILURE,"malloc");
	}
	return r;
}
ssize_t readall(int fd,void *bufp){
	char *buf,*p;
	size_t bufsiz,r1;
	ssize_t r,ret=0;
	static const size_t ra_bufsize=4096;
	int i;
	bufsiz=ra_bufsize;
	if((buf=malloc(ra_bufsize))==NULL)return -errno;
	r1=0;
	while((r=read(fd,buf+ret,ra_bufsize-r1))>0){
		r1+=r;
		ret+=r;
		if(ret==bufsiz){
			bufsiz+=ra_bufsize;
			if((p=realloc(buf,bufsiz))==NULL){
				i=errno;
				free(buf);
				return -i;
			}
			buf=p;
			r1=0;
		}
	}
	if(ret==bufsiz){
	if((p=realloc(buf,bufsiz+1))==NULL){
		i=errno;
		free(buf);
		return -i;
	}
	buf=p;
	}
	buf[ret]=0;
	*(void **)bufp=buf;
	return ret;
}
int ff_output=0,quiet=0;
#define out(fmt,...) ((ff_output||quiet)?0:fprintf(stderr,(fmt),__VA_ARGS__))
#define outc(c) ((ff_output||quiet)?0:fputc((c),stderr))
struct bitmap *bm=NULL;
double bm_end=0.0;
double bminterval=0.01;
void bmload(const char *path){
	ssize_t r;
	int fd=open(path,O_RDONLY);
	if(fd<0)
		err(EXIT_FAILURE,"open");
	r=readall(fd,&bm);
	close(fd);
	if(r<0)
		err(EXIT_FAILURE,"readall");
	fd=bm_check(bm,r);
	if(fd<0)
		errx(EXIT_FAILURE,"invaild bitmap (%d)",fd);
	bm_end=(bm->width+1)*bminterval;
}
int32_t bmratio=128;
int bmmirror=0,bmvmirror=0;
double bm_freq_lowest=512,bm_freq_functor=8;
double bmfunc(double t0){
	unsigned long t;
	double sum;
	int32_t dy,w,h,x;
	if(!bm)
		return 0.0;
	t=floor(t0/bminterval);
	w=bm->width;
	if(t>=w)
		return 0.0;
	if(bmmirror)
		t=w-t-1;
	h=bm->height;
	sum=0.0;
	dy=((int64_t)h+(int64_t)bmratio-1)/bmratio;
	for(int32_t y=h-1;y>=0;y-=dy){
		x=bm_getpixel(bm,t,y);
		x=((x&0x0000ff)+((x&0x00ff00)>>8)+((x&0xff0000)>>16));
		sum+=(x/765.0)*sin((2*M_PI)*((bmvmirror?h-y-1:y)*bm_freq_functor+bm_freq_lowest)*t0);
	}
	sum/=bmratio;
	return sum;
}
//#define TEXT_ENABLED
#ifdef TEXT_ENABLED
static void *xrealloc(void *old,size_t size){
	void *r;
	r=old?realloc(old,size):malloc(size);
	if(!r){
		err(EXIT_FAILURE,"realloc");
	}
	return r;
}
#include "texts/text.h"
struct text {
	struct sbmp **sbuf;
	size_t scount;
	size_t text_width;
	double tfinterval;
	double freq_lowest,freq_functor;
	int mirror,vmirror;
	int32_t ratio,tinterval;
} **defts=NULL;
size_t dcount=0;
char dbuf[TEXT_MAXOSIZE];
void sadd(struct text *t,const struct sbmp *sp){
	t->sbuf=xrealloc(t->sbuf,(++t->scount)*sizeof(void *));
	t->sbuf[t->scount-1]=xmalloc(sp->size+sizeof(struct sbmp));
	memcpy(t->sbuf[t->scount-1],sp,sp->size+sizeof(struct sbmp));
}
void sfree(struct text *t){
	for(ptrdiff_t i=t->scount-1;i>=0;--i){
		free(t->sbuf[i]);
	}
	free(t->sbuf);
}
void sfreeall(void){
	for(ptrdiff_t i=dcount-1;i>=0;--i){
		sfree(defts[i]);
		free(defts[i]);
	}
	free(defts);
}
void sbmp2f(struct text *t,int c){
	const struct sbmp *sp;
	sp=text_getsbmp(c);
	if(!sp)
		errx(EXIT_FAILURE,"cannot found char with ascii %u",c);
	if(sbmp_decompress(sp,(struct sbmp *)dbuf))
		errx(EXIT_FAILURE,"cannot decompress bitmap with ascii %u",c);
	sadd(t,(struct sbmp *)dbuf);
	t->text_width+=((struct sbmp *)dbuf)->width;
}
int reverse=0,mirror=0,vmirror=0;
//int text_ok=0;
double text_end=0.0;
double tfinterval=0.00390625;
int32_t ratio=128,tinterval=24;
double freq_lowest=512,freq_functor=4;
void text_scan(struct text *t,const char *p){
	const char *p1;
	double e;
	if(!*p)
		return;
	if(reverse){
		p1=p+strlen(p)-1;
		while(p1>=p){
			sbmp2f(t,*p1);
			--p1;
		}
	}else {
		do{
			sbmp2f(t,*p);
			++p;
		}while(*p);
	}
	e=(t->text_width+1)*tfinterval;
	if(e>text_end)
		text_end=e;
}
double ftext2(const struct text *restrict tp,double t0){
	unsigned long n,t;
	struct sbmp *sp;
	double sum;
	int64_t u;
	int32_t dy,w,h;
	if(!tp->sbuf)
		return 0.0;
	t=floor(t0/tp->tfinterval);
	n=0;
next:
	sp=tp->sbuf[n];
	w=sp->width;
	u=w+tp->tinterval;
	if(t>=u){
		++n;
		if(n<tp->scount){
			t-=u;
			goto next;
		}
		return 0.0;
	}
	if(t>=w)
		return 0.0;
	if(tp->mirror)
		t=w-t-1;
	h=sp->height;
	sum=0.0;
	dy=((int64_t)h+(int64_t)tp->ratio-1)/tp->ratio;
	for(int32_t y=h-1;y>=0;y-=dy){
		if(!sbmp_tstpixel(sp,t,y))
			continue;
		sum+=sin((2*M_PI)*((tp->vmirror?h-y-1:y)*tp->freq_functor+tp->freq_lowest)*t0);
	}
	sum/=tp->ratio;
	return sum;
}
double ftext(double t0){
	return dcount?ftext2(defts[0],t0):0.0;
}
static double ftext2_md(size_t n,double *args){
	n=(size_t)args[1];
	return n<dcount?ftext2(defts[n],*args):0.0;
}
#define setfield(fld) t->fld=fld
void tattr_init(struct text *t){
	t->sbuf=NULL;
	t->scount=0;
	t->text_width=0;
	setfield(tfinterval);
	setfield(freq_lowest);
	setfield(freq_functor);
	setfield(mirror);
	setfield(vmirror);
	setfield(ratio);
	setfield(tinterval);
}
struct text *newdeft(void){
	struct text *r=xmalloc(sizeof(struct text));
	defts=xrealloc(defts,(++dcount)*sizeof(void *));
	tattr_init(r);
	defts[dcount-1]=r;
	return r;
}
extern unsigned long sample_freq;
static double supt(size_t n,const struct expr *args,double input){
	double r=-INFINITY,x;
	for(unsigned long t=0;;++t){
		x=(double)t/sample_freq;
		if(x>=text_end)
			break;
		if((x=expr_eval(args,x))>r)
			r=x;
	}
	return r;
}
static double inft(size_t n,const struct expr *args,double input){
	double r=+INFINITY,x;
	for(unsigned long t=0;;++t){
		x=(double)t/sample_freq;
		if(x>=text_end)
			break;
		if((x=expr_eval(args,x))<r)
			r=x;
	}
	return r;
}
static double correct(size_t n,double *args){
	double l=args[1],u=args[2];
	return 2.0*(*args-l)/(u-l)-1;
}
void text_init(){
	if(!expr_symset_add(es,"text",EXPR_FUNCTION,ftext))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"text2",EXPR_MDFUNCTION,ftext2_md,(size_t)2))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"text_end",EXPR_VARIABLE,&text_end))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"supt",EXPR_MDEPFUNCTION,supt,(size_t)1))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"inft",EXPR_MDEPFUNCTION,inft,(size_t)1))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"correct",EXPR_MDFUNCTION,correct,(size_t)3))
		err(EXIT_FAILURE,"expr_symset_add");
}
#endif
struct expr *ep=NULL,*ept=NULL;
unsigned long sample_freq=44100;
double sample_freq_d=44100.0,msg_time_interval=0.05;
volatile double vf=0.0;
const char *outfile=NULL;
char hotsym[EXPR_SYMLEN]={"hot"};
int outfd=-1,raw=0;
#if !defined(U8)&&!defined(U16BE)&&!defined(U32BE)&&!defined(S32LE)
#define S32LE
#endif

#if defined(U32BE)
#define ampl_type uint32_t
#define ampl_fmt "u32be"
#define ampl_make(_dampl) (ampl_type)(UINT32_MAX*0.5*(1+(_dampl)))
#define ampl_hton(_a) htonl(_a)
#define ffmpeg_extra ,
#elif defined(S32LE)
#define ampl_type int32_t
#define ampl_fmt "s32le"
#define ampl_make(_dampl) (ampl_type)(INT32_MAX*(_dampl))
#define ampl_hton(_a) (_a)
#define ffmpeg_extra ,"-c:a","pcm_s32le",
#elif defined(U16BE)
#define ampl_type uint16_t
#define ampl_fmt "u16be"
#define ampl_make(_dampl) (ampl_type)(UINT16_MAX*0.5*(1+(_dampl)))
#define ampl_hton(_a) htons(_a)
#define ffmpeg_extra ,
#elif defined(U8)
#define ampl_type uint8_t
#define ampl_fmt "u8"
#define ampl_make(_dampl) (ampl_type)(UINT8_MAX*0.5*(1+(_dampl)))
#define ampl_hton(_a) (_a)
#define ffmpeg_extra ,"-c:a","pcm_u8",
#endif
ampl_type *buffer=NULL,*buffer_cur=NULL,*buffer_end=NULL;
size_t buffer_size=PIPE_BUF;
int unsafe=0;
void setexpr(struct expr **p,const char *c){
	int e;
	char ei[EXPR_SYMLEN];
	*p=new_expr6(c,"t",es,unsafe?0:EXPR_IF_PROTECT,&e,ei);
	if(!*p)
		errx(EXIT_FAILURE,"%s:%s",expr_error(e),ei);
}
extern double sample_freq_d;
__attribute__((constructor)) void atstart(void){
	es=new_expr_symset();
	if(!es)
		err(EXIT_FAILURE,"new_expr_symset");
	if(!expr_symset_add(es,"y",EXPR_VARIABLE,&vf))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"sample",EXPR_VARIABLE,&sample_freq_d))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"time",EXPR_ZAFUNCTION,dtime))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"bm",EXPR_FUNCTION,bmfunc))
		err(EXIT_FAILURE,"expr_symset_add");
	if(!expr_symset_add(es,"bm_end",EXPR_VARIABLE,&bm_end))
		err(EXIT_FAILURE,"expr_symset_add");
#ifdef TEXT_ENABLED
	text_init();
#endif
}
__attribute__((destructor)) void atend(void){
	if(ep)
		expr_free(ep);
	if(ept)
		expr_free(ept);
	expr_symset_free(es);
	if(outfd>=0)
		close(outfd);
	if(buffer)
		free(buffer);
	if(bm)
		free(bm);
#ifdef TEXT_ENABLED
	sfreeall();
#endif
}
#define ffplay_arg "-f",ampl_fmt,"-ar",ar,"-i",pipename,"-autoexit","-nodisp","-hide_banner"
#define ffmpeg_arg "-y","-f",ampl_fmt,"-ar",ar,"-i",pipename,"-hide_banner"
pid_t fpid;
int getpipe(void){
	char pipename[32];
	char ar[32];
	int pipefd[2];
	int fd;
	pid_t pid;
	if(pipe(pipefd)<0)
		err(EXIT_FAILURE,"pipe");
	pid=fork();
	if(pid<0)
		err(EXIT_FAILURE,"fork");
	if(!pid){
		if(!ff_output){
			fd=open("/dev/null",O_RDWR);
			if(fd>=0){
				close(STDERR_FILENO);
				close(STDOUT_FILENO);
				close(STDIN_FILENO);
				dup2(fd,STDERR_FILENO);
				dup2(fd,STDOUT_FILENO);
				dup2(fd,STDIN_FILENO);
				close(fd);
			}
		}
		snprintf(ar,32,"%lu",sample_freq);
		snprintf(pipename,32,"pipe:%d",pipefd[0]);
		close(pipefd[1]);
		if(outfile){
			execlp("ffmpeg","ffmpeg",ffmpeg_arg ffmpeg_extra outfile,NULL);
			execlp("./ffmpeg","./ffmpeg",ffmpeg_arg ffmpeg_extra outfile,NULL);
		}else {
			execlp("ffplay","ffplay",ffplay_arg,NULL);
			execlp("./ffplay","./ffplay",ffplay_arg,NULL);
		}
		exit(EXIT_FAILURE);
	}
	fpid=pid;
	close(pipefd[0]);
	return pipefd[1];
}
double atod2(const char *str){
	double r;
	/*char *c;
	r=strtod(str,&c);
	if(c==str||*c)
		errx(EXIT_FAILURE,"invaild double %s",str);*/
	int error=0;
	char err[EXPR_SYMLEN];
	r=expr_calc5(str,&error,err,NULL,EXPR_IF_PROTECT|EXPR_IF_NOKEYWORD);
	if(error)
		errx(EXIT_FAILURE,"invaild expression: %s (%s:%s)",str,expr_error(error),err);
	return r;
}
long atol2(const char *str){
	long r;
	char *c;
	r=strtol(str,&c,10);
	if(c==str||*c)
		errx(EXIT_FAILURE,"invaild integer: %s",str);
	return r;
}
sig_atomic_t sat=0;
void sig(int s){
	switch(s){
		case SIGPIPE:
			sat=1;
			break;
		case SIGINT:
			sat=2;
			break;
		default:
			break;
	}
}
double det2freq(unsigned long det){
	if(!det)
		return 0.0;
	return sample_freq_d/(det*2);
}
int str2i(const char *in,...){
	va_list ap;
	const char *p;
	int r;
	va_start(ap,in);
	while((p=va_arg(ap,const char *))){
		r=va_arg(ap,int);
		if(!strcmp(in,p))
			goto end;
	}
	r=-1;
end:
	va_end(ap);
	return r;
}
void showsym(int type,const char *extra){
	char buf[32];
	for(const struct expr_builtin_symbol *p=expr_symbols;;++p){
		if(!p->str){
			break;
		}
		if(p->type!=type)
			continue;
		snprintf(buf,32,"%s%s",p->str,extra);
		buf[31]=0;
		fprintf(stdout,"%-16s",buf);
		if(p->flag&EXPR_SF_INJECTION)
			fputs(" injection",stdout);
		if(p->flag&EXPR_SF_UNSAFE)
			fputs(" unsafe",stdout);
		fputc('\n',stdout);
	}
}
void printdouble(double x){
	char *buf,*p;
	if(asprintf(&buf,"%.1024lf",x)<0)
		err(EXIT_FAILURE,"asprintf");
	p=strchr(buf,'.');
	if(p){
		p+=strlen(p);
		while(*(--p)=='0')*p=0;
		if(*p=='.')*p=0;
	}
	fprintf(stdout,"%s\n",buf);
	free(buf);
}
int calc=0,noint=0;
double calc_input=0.0;
const struct option ops[]={
	{"cond",1,NULL,'c'},
	{"sample",1,NULL,'s'},
	{"output",1,NULL,'o'},
	{"quiet",2,NULL,'q'},
	{"buffer",2,NULL,'b'},
	{"raw",0,NULL,'r'},
	{"hot",1,NULL,'h'},
	{"hotsym",1,NULL,'H'},
	{"ff-output",0,NULL,'f'},
	{"calc",2,NULL,'C'},
	{"unsafe",2,NULL,'u'},
	{"no-catch-SIGINT",0,NULL,'i'},
	{"list",2,NULL,'l'},
	{"help",0,NULL,0x706c6568},
	{"in",1,NULL,0x6e69},
	{"mirror",0,NULL,'m'},
	{"vmirror",0,NULL,'v'},
	{"freq-interval",1,NULL,'n'},
	{"freq-lowest",1,NULL,'w'},
	{"freq-functor",1,NULL,'t'},
	{"ratio",1,NULL,'a'},
#ifdef TEXT_ENABLED
	{"text",1,NULL,'T'},
	{"text-reverse",0,NULL,'R'},
	{"text-mirror",0,NULL,'M'},
	{"text-vmirror",0,NULL,'V'},
	{"text-freq-interval",1,NULL,'I'},
	{"text-freq-lowest",1,NULL,'L'},
	{"text-freq-functor",1,NULL,'F'},
	{"text-ratio",1,NULL,'A'},
	{"text-interval",1,NULL,'D'},
#endif
	{NULL}
};
#define show(a,b) {if(sndbkn<0.0)out("\033[K\0337%.2lfs cost|%.2lfs written|freq=%.2lf (inaccurate)\0338",a,b,det2freq(det));else out("\033[K\0337%.2lfs cost|%.2lfs written|freq=%.2lf (inaccurate)|sound broken(%.2lfs)\0338",a,b,det2freq(det),sndbkn);}
int main(int argc,char **argv){
	double st,lt,ct,x,ovf,sndbkn;
	unsigned long t,det,let;
	ampl_type ampl;
	int status;
	if(argc<2){
show_help:
		fprintf(stdout,"usage: %s [options] expression\n"
				"\texpresion\tsuch as \"sin(4400*2*pi*t)\" which will generate a sine wave with a frequency of 4400Hz\n"
				"\t-c,--cond expression\tcondition to stop\n"
				"\t-s,--sample sample_rate (default=%lu)\n"
				"\t-o,--output filename\n"
				"\t-q,--quiet[=time]\tdo not output message to screen\n"
				"\t-b,--buffer[=size]\tcreate a buffer to write data,default size is PIPE_BUF(%zu)\n"
				"\t-r,--raw\toutput raw data to stdout or file\n"
				"\t-h,--hot expression\thot function\n"
				"\t-f,--ff-output\toutput message of ffplay/ffmpeg to screen\n"
				"\t-C,--calc[=input]\tevaluate the expression only\n"
				"\t-u,--unsafe\tallow unsafe operations(e.g. explode() and direct memory access [] which may cause SIGSEGV)\n"
				"\t-i,--no-catch-SIGINT\tdo not catch SIGINT\n"
				"\t-l,--list[=category]\tlist function,variable,etc\n"
				"\t--help\tshow this help\n"
				"options for bitmap:\n"
				"\t--in\tinput a bitmap to function bm()\n"
				"\t-m,--freq-mirror\tmirror the text\n"
				"\t-v,--freq-vmirror\tmirror the text vertically\n"
				"\t-n,--freq-interval time (default=%lg)\n"
				"\t-w,--freq-lowest freq (default=%lg)\n"
				"\t-t,--freq-functor (default=%lg)\n"
				"\t-a,--ratio ratio (default=%d)\n"
#ifdef TEXT_ENABLED
				"options for text:\n"
				"\t-T,--text text\tgiven the in function text()\n"
				"\t-R,--text-reverse\treverse the text\n"
				"\t-M,--text-mirror\tmirror the text\n"
				"\t-V,--text-vmirror\tmirror the text vertically\n"
				"\t-I,--text-freq-interval time (default=%lg)\n"
				"\t-L,--text-freq-lowest freq (default=%lg)\n"
				"\t-F,--text-freq-functor functor (default=%lg)\n"
				"\t-A,--text-ratio ratio (default=%d)\n"
				"\t-D,--text-interval pixel (default=%d)\n"
#endif
#ifdef TEXT_ENABLED
				"text height: %d\n"
#endif
				"format: " ampl_fmt "\n"
				"ffplay/ffmpeg is required in playing/file-output mode.\n"
				"compiled on " __DATE__ " "  __TIME__ "\n"
				,argv[0],sample_freq,(size_t)PIPE_BUF,bminterval,bm_freq_lowest,bm_freq_functor,bmratio
#ifdef TEXT_ENABLED
				,tfinterval,freq_lowest,freq_functor,ratio,tinterval,(int32_t)TEXT_HEIGHT
#endif
				);
		return EXIT_SUCCESS;
	}
	opterr=1;
	for(;;){
		switch(getopt_long(argc,argv,"c:s:o:q::b::rh:H:fC::ul::imvn:w:t:a:"
#ifdef TEXT_ENABLED
					"T:RMVI:L:F:A:D:"
#endif
					,ops,NULL)){
			case 'c':
				if(ept)
					errx(EXIT_FAILURE,"cond redefined");
				setexpr(&ept,optarg);
				break;
			case 's':
				sample_freq=atol2(optarg);
				sample_freq_d=(double)sample_freq;
				break;
			case 'o':
				outfile=optarg;
				break;
			case 'q':
				if(optarg){
					msg_time_interval=atod2(optarg);
				}else
					quiet=1;
				break;
			case 'b':
				buffer_size=(optarg?atol2(optarg):PIPE_BUF);
				break;
			case 'r':
				raw=1;
				break;
			case 'h':
				if(!expr_symset_add(es,hotsym,EXPR_HOTFUNCTION,optarg,"t"))
					errx(EXIT_FAILURE,"cannot add hot function");
				break;
			case 'H':
				if(strlen(optarg)>=EXPR_SYMLEN)
					errx(EXIT_FAILURE,"symbol of hot function is too long");
				strcpy(hotsym,optarg);
				break;
			case 'f':
				ff_output=1;
				break;
			case 'C':
				if(optarg)
					calc_input=atod2(optarg);
				calc=1;
				break;
			case 'u':
				unsafe=1;
				break;
			case 'i':
				noint=1;
				break;
			case 0x706c6568:
				goto show_help;
			case 0x6e69:
				if(!bm)
					bmload(optarg);
				break;
			case 'm':
				bmmirror^=1;
				break;
			case 'v':
				bmvmirror^=1;
				break;
			case 'n':
				bminterval=atod2(optarg);
				if(bminterval<=0.0)
					errx(EXIT_FAILURE,"\"%s\" is not a positive number.",optarg);
				break;
			case 'w':
				bm_freq_lowest=atod2(optarg);
				break;
			case 't':
				bm_freq_functor=atod2(optarg);
				break;
			case 'a':
				bmratio=(int32_t)atol2(optarg);
				if(bmratio<=0)
					errx(EXIT_FAILURE,"\"%s\" is not a positive integer.",optarg);
				break;
			case 'l':
				if(!optarg){
					fprintf(stdout,"non-builtin function constant\n"
						"function,constant only show the builtins.\n"
						);
				}else {
					switch(status=str2i(optarg,"function",0,"constant",1,"non-builtin",2,NULL)){
						case 0:
							showsym(EXPR_FUNCTION,"(t)");
							showsym(EXPR_ZAFUNCTION,"()");
							showsym(EXPR_MDFUNCTION,"(...)");
							showsym(EXPR_MDEPFUNCTION,"(***)");
							break;
						case 1:
							showsym(EXPR_CONSTANT,"");
							break;
						case 2:
							fprintf(stdout,
			"function:\n"
			"time()\treturn current unix stamp\n"
			"bm(t)\tuse the function to generate sound of the given bitmap in spectrum\n"
#ifdef TEXT_ENABLED
			"text(t)\tuse the function to generate sound of the given text in spectrum\n"
			"text2(t,index)\tfor more than 1 texts,equivalent to text(t) when index=0\n"
			"supt(f(t))\tfind max value of f(t) for 0<t<text_end\n"
			"inft(f(t))\tfind min value of f(t) for 0<t<text_end\n"
			"correct(t,a,b)\tmap [a,b] to [-1,1] linearly(may exceed for the deviation of float)\n"
#endif
			"variable:\n"
			"sample\tsample rate\n"
			"y\tcurrent value of expression\n"
			"bm_end\tbitmap will vanish when t>=text_end\n"
#ifdef TEXT_ENABLED
			"text_end\ttext will vanish when t>=text_end\n"
#endif
									);
							break;
						default:
							errx(EXIT_FAILURE,"invaild category: %s",optarg);
					}
				}
				return EXIT_SUCCESS;
			case '?':
				exit(EXIT_FAILURE);
				break;
#ifdef TEXT_ENABLED
//#define text_ok_check(_c) if(text_ok)errx(EXIT_FAILURE,"option -" _c " must be used before --text/-T")

			case 'T':
				text_scan(newdeft(),optarg);
				break;
			case 'R':
				reverse^=1;
				break;
			case 'M':
				mirror^=1;
				break;
			case 'V':
				vmirror^=1;
				break;
			case 'I':
				tfinterval=atod2(optarg);
				if(tfinterval<=0.0)
					errx(EXIT_FAILURE,"\"%s\" is not a positive number.",optarg);
				break;
			case 'L':
				freq_lowest=atod2(optarg);
				break;
			case 'F':
				freq_functor=atod2(optarg);
				break;
			case 'A':
				ratio=(int32_t)atol2(optarg);
				if(ratio<=0)
					errx(EXIT_FAILURE,"\"%s\" is not a positive integer.",optarg);
				break;
			case 'D':
				tinterval=(int32_t)atol2(optarg);
				if(tinterval<0)
					errx(EXIT_FAILURE,"\"%s\" is not a non-negative integer.",optarg);
				break;
#endif
			case -1:
				goto break2;
		}
	}
break2:
	if(optind==argc-1){
		setexpr(&ep,argv[optind]);
	}else
		errx(EXIT_FAILURE,"no or redefined expression");
	if(calc){
		printdouble(expr_eval(ep,calc_input));
		return EXIT_SUCCESS;
	}
	if(raw)
		outfd=(outfile?xopen(outfile):STDOUT_FILENO);
	else
		outfd=getpipe();
	if(buffer_size){
		if(sizeof(ampl_type)>1)
			buffer_size=(buffer_size+(sizeof(ampl_type)-1))&~(sizeof(ampl_type)-1);
		buffer=xmalloc(buffer_size);
		buffer_cur=buffer;
		buffer_end=buffer+buffer_size/sizeof(ampl_type);
	}
	signal(SIGPIPE,sig);
	if(!noint)
		signal(SIGINT,sig);
	st=dtime();
	lt=st;
	ct=st;
	status=0;
	ovf=0.0;
	let=0;
	det=0;
	sndbkn=-1.0;
	show(0.0,0.0);
	for(t=0;;++t){
		x=(double)t/sample_freq;
		vf=expr_eval(ep,x);
		if((ept&&expr_eval(ept,x))||sat==2){
			show(ct-st,x);
			break;
		}
		if(vf>1){
			vf=1;
			sndbkn=x;
		}else if(vf<-1){
			vf=-1;
			sndbkn=x;
		}
		if(vf>ovf&&status){
			det=t-let;
			let=t;
			status=0;
		}else if(vf<ovf&&!status){
			det=t-let;
			let=t;
			status=1;
		}
		ovf=vf;
        	ampl=ampl_make(vf);
		ampl=ampl_hton(ampl);
		if(!buffer){
	        	write(outfd,&ampl,sizeof(ampl));
		}else {
			if(buffer_cur==buffer_end){
				write(outfd,buffer,buffer_size);
				buffer_cur=buffer;
			}
			*(buffer_cur++)=ampl;
		}
		if(sat==1){
			outc('\n');
			if(waitpid(fpid,&status,0)>=0&&WIFEXITED(status))
				errx(EXIT_FAILURE,"broken pipe (status:%d)",WEXITSTATUS(status));
			errx(EXIT_FAILURE,"broken pipe");
		}
		ct=dtime();
		if(ct-lt>=msg_time_interval){
			show(ct-st,x);
			lt=ct;
		}
	}
	if(buffer&&buffer_cur>buffer){
		write(outfd,buffer,(buffer_cur-buffer)*sizeof(ampl));
	}
	close(outfd);
	outfd=-1;
	if(!quiet)
		out("\ndata is written, %.2lfs remaining\n",x-(ct-st));
	if(waitpid(fpid,&status,0)>=0&&WIFEXITED(status)&&WEXITSTATUS(status)!=EXIT_SUCCESS)
		errx(EXIT_FAILURE,"failed (status:%d)",WEXITSTATUS(status));
	return EXIT_SUCCESS;
}
