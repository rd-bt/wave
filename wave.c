#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <err.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <getopt.h>
#include "expr.h"
struct expr *ep=NULL,*ept=NULL;
struct expr_symset *es=NULL;
unsigned long sample_freq=44100;
double sample_freq_d=44100.0,msg_time_interval=0.05;
volatile double vf=0.0;
const char *outfile=NULL;
int outfd=-1,quiet=0,raw=0;
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
size_t buffer_size=0;
void setexpr(struct expr **p,const char *c){
	int e;
	char ei[EXPR_SYMLEN];
	*p=new_expr(c,"t",es,&e,ei);
	if(!*p)
		errx(EXIT_FAILURE,"%s:%s",expr_error(e),ei);
}
__attribute__((constructor)) void atstart(void){
	es=new_expr_symset();
	if(!es)
		err(EXIT_FAILURE,"new_expr_symset");
	if(!expr_symset_add(es,"y",EXPR_VARIABLE,&vf))
		err(EXIT_FAILURE,"expr_symset_add");
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
double atod2(const char *str){
	double r;
	char *c;
	r=strtod(str,&c);
	if(c==str||*c)
		errx(EXIT_FAILURE,"invaild double %s",str);
	return r;
}
long atol2(const char *str){
	long r;
	char *c;
	r=strtol(str,&c,10);
	if(c==str||*c)
		errx(EXIT_FAILURE,"invaild integer %s",str);
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
const struct option ops[]={
	{"cond",1,NULL,'c'},
	{"sample",1,NULL,'s'},
	{"output",1,NULL,'o'},
	{"quiet",2,NULL,'q'},
	{"buffer",2,NULL,'b'},
	{"raw",0,NULL,'r'},
	{NULL}
};
double det2freq(unsigned long det){
	if(!det)
		return 0.0;
	return sample_freq_d/(det*2);
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
#define show(a,b) if(!quiet)fprintf(stderr,"\033[K\0337%.2lfs cost\t%.2lfs written\tfrequency=%.2lf (inaccurate)\0338",a,b,det2freq(det));
int main(int argc,char **argv){
	double st,lt,ct,x,ovf;
	unsigned long t,det,let;
	ampl_type ampl;
	int status;
	if(argc<2){
		fprintf(stdout,"usage: %s [options] expression\n"
				"\texpresion\tsuch as \"sin(4400*2*pi*t)\" which will generate a sine wave with a frequency of 4400Hz\n"
				"\t-c,--cond expression\tcondition to stop\n"
				"\t-s,--sample sample_rate (default=44100)\n"
				"\t-o,--output filename\n"
				"\t-q,--quiet[=time]\tdo not output message to stderr\n"
				"\t-b,--buffer[=size]\tcreate a buffer to write data,default size is PIPE_BUF\n"
				"\t-r,--raw output raw data to stdout or file\n"
				"format: " ampl_fmt "\n"
				"ffplay/ffmpeg is required in playing/file-output mode.\n"
				,argv[0]);
		return EXIT_SUCCESS;
	}
	opterr=1;
	signal(SIGPIPE,sig);
	signal(SIGINT,sig);
	for(;;){
		switch(getopt_long(argc,argv,"c:s:o:q::b::r",ops,NULL)){
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
				if(!buffer_size)
					break;
				if(sizeof(ampl_type)>1)
					buffer_size=(buffer_size+(sizeof(ampl_type)-1))&~(sizeof(ampl_type)-1);
				buffer=xmalloc(buffer_size);
				buffer_cur=buffer;
				buffer_end=buffer+buffer_size/sizeof(ampl_type);
				break;
			case 'r':
				raw=1;
				break;
			case '?':
				exit(EXIT_FAILURE);
				break;
			case -1:
				goto break2;
		}
	}
break2:
	if(optind==argc-1){
		setexpr(&ep,argv[optind]);
	}else
		errx(EXIT_FAILURE,"no or redefined expression");
	if(raw)
		outfd=(outfile?xopen(outfile):STDOUT_FILENO);
	else
		outfd=getpipe();
	st=dtime();
	lt=st;
	ct=st;
	status=0;
	ovf=0.0;
	let=0;
	det=0;
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
		}else if(vf<-1){
			vf=-1;
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
			fputc('\n',stderr);
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
		fprintf(stderr,"\ndata is written, %.2lfs remaining\n",x-(ct-st));
	waitpid(fpid,NULL,0);
	return EXIT_SUCCESS;
}
