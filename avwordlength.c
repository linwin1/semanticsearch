#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>


#define MAX_LINE_LENGTH 4096
#define MAX_WORD_LENGTH 100

#define SEPARATOR " \\\t\r\n.,;/()[]{}=+-*:<>&#%^?$'`\""


static int  get_token(char *buf, int start, char* pattern, char *res, int maxlen);

FILE *fin = stdin;


int main(int argc, char *argv[])
{
    int c,start,slen,len,min_word_len=2;
    char *inputname=NULL;
    char line[MAX_LINE_LENGTH];
    static char word[MAX_WORD_LENGTH];
    long totlen=0,totword=0;

    while ((c = getopt (argc, argv, "i:n:")) != EOF){
	switch (c) {
	case 'i':
	    inputname = strdup(optarg);
	    break;
	case 'n':
	    min_word_len = atoi(optarg);
	    printf("Minimale Wordlaenge auf %d Zeichen gesetzt\n",min_word_len);
	    break;
	default:
	    usage(argv[0]);
	}
    }

    if (inputname)
	if (NULL == (fin = fopen(inputname,"r"))){
	    printf("can't open %s\n",inputname);
	    exit(-1);
	}


    while(fgets(line,MAX_LINE_LENGTH,fin)){
	start=0;
	len=strlen(line);
	while( (start=get_token(line,start,SEPARATOR,word,MAX_WORD_LENGTH)) <= len){
	    if ((slen=strlen(word)) >= min_word_len){
		totlen+=slen;
		totword++;
	    }
	}

    }

    fclose(fin);

    printf("%d Worte gefunden\n",totword);
    printf("Mittlere Wordlaenge: %.2f Zeichen\n",(totlen*1.0)/totword);
    
    return 0;
}


static int get_token(char *buf, int start, char* pattern, char *res, int maxlen)
{
    int len,buflen=strlen(buf);
    char *tmp,*ptr;

    *res=0;

    if (start > buflen) return -1;

    ptr=buf+buflen;
    pattern--;
    while(*++pattern){
        tmp=strchr(buf+start,*pattern); 
        if (NULL == tmp) continue;
        ptr = (tmp < ptr) ? tmp : ptr;
    }

    if (ptr != (buf+buflen)){
        len= ((ptr-buf-start) < maxlen) ? (ptr-buf-start) : maxlen-1;
        memcpy(res,buf+start,len);
	*(res+len)=0;
        return (int)(ptr-buf+1);
    }

    return buflen+1;
}


usage(char *name)
{
    printf("usage: %s\n",name);
    exit(1);
}


