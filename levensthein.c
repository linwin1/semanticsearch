#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "hash.h"

int levenshtein_distance(char *s,char*t, int damerau);
inline int minimum(int a,int b,int c);


#define MAX_WORD_LENGTH  1024
#define MAX_NWORDS     300000
#define min(a,b) ((a) < (b)) ? (a) : (b)

char *word[MAX_NWORDS];

/****************************************/
/*Implementation of Levenshtein distance*/
/****************************************/


int levenshtein_distance(char *s,char*t, int damerau)
/*Compute levenshtein distance between s and t*/
{
    int i,j,n,m,cost,*d,distance;
    n=strlen(s); 
    m=strlen(t);

    if(n==0 || m==0)
	return -1; //a negative return value means that one or both strings are empty.
  
    d = (int *)malloc((sizeof(int))*(m+1)*(n+1));
    m++;
    n++;

    for(i = 0; i < n; i++)
	d[i] = i;

    for(j = 0; j < m; j++)
	d[j*n] = j;

    for(i=1;i<n;i++)
	for(j=1;j<m;j++){
	    if(s[i-1]==t[j-1])
		cost=0;
	    else
		cost=1;

	    d[j*n+i]=minimum(d[(j-1)*n+i]+1,d[j*n+i-1]+1,d[(j-1)*n+i-1]+cost);

	    if (damerau){
		// Damerau Levenshtein (Transposition)
		if ( (i > 1) && (j > 1) && (s[j-1] == t[i-2]) && (s[j-2] == t[i-1]) )
		    d[j*n+i] = min(d[j*n+i],d[(j-2)*n+i-2] +cost);
	    }
	}
    distance=d[m*n-1];
    free(d);
    return distance;
}

inline int minimum(int a,int b,int c)
/*Gets the minimum of three values*/
{
  int mini=a;
  if(b<mini)
    mini=b;
  if(c<mini)
    mini=c;
  return mini;
}

#ifdef _TEST
int main(int argc, char *argv[])
{
    if (argc == 3)
	printf("\n%s %s : %d\n",argv[1],argv[2],levenshtein_distance(argv[1],argv[2]));
}
#endif



int alphasort(const void *a, const void *b)
{
    return strcasecmp((char *)(*(int *)a),(char *)(*(int *)b));
}

void recurse_levenshtein(int idx,int nwords)
{
    int loop,loop2,dist;
    char tmpword[MAX_WORD_LENGTH];
    static int depth,found;
    int locfound = 0;

    if (depth > 3){
	depth--;
	return;
    }

    if (depth == 0)
	found = 0;

    depth++;
    //    printf("depth = %d\n",depth);

    for (loop = 0; loop < nwords; loop++){
	if (word[loop]==NULL)
	    continue;
	dist = levenshtein_distance(word[idx],word[loop],1);
	if (dist == 1){
	    if (depth == 1)
		printf("%s\n",word[idx]);

	    printf("%s\n",word[loop]);
	    free(word[idx]);
	    word[idx] = NULL;
	    found++;
	    locfound = loop;
	    recurse_levenshtein(loop,nwords);
	}
    }
    depth--;
    
    if (locfound){
	    free(word[locfound]);
	    word[locfound] = NULL;
    }

    if (depth == 0){
	if (found == 0){
	    printf("%s\n",word[idx]);
	    free(word[idx]);
	    word[idx] = NULL;
	}
	printf("------------------\n");
    }
}


void subst_umlaut(unsigned char *w)
{
    unsigned char *ptr;
    unsigned char *end;

    end = w + strlen(w)+1;

    for (ptr = w; *ptr; ptr++){
	switch (*ptr){
	case 0xe4: // a umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='a';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xfc: //u umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='u';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xf6: //o umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='o';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xc4: //A umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='A';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xdc: //U umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='U';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xd6: //O umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='O';
	    *(ptr+1) = 'e';
	    end = w + strlen(w)+1;
	    break;
	case 0xdf: //ss umlaut
	    memmove(ptr+2,ptr+1,end-ptr);
	    *ptr='s';
	    *(ptr+1) = 's';
	    end = w + strlen(w)+1;
	    break;

	}
    }
}


int main(int argc, char *argv[])
{
    FILE *fin;
    int loop,nword,loop2;
    char line[MAX_WORD_LENGTH];
    hash_t word_tab;
    
    fin = fopen(argv[1],"r");
    
    if (fin == NULL){
	printf("File %s not readable\n");
	exit(1);
    }
    
    hash_init(&word_tab,MAX_NWORDS);
    
    for (loop = 0; !feof(fin);loop++){
	if (NULL == fgets(line,sizeof(line),fin))
	    break;
	line[strlen(line)-1] = 0;
	for (loop2=0;loop2<strlen(line);loop2++)
	    line[loop2] = tolower(line[loop2]);

	subst_umlaut(line);
	if (HASH_FAIL == hash_lookup(&word_tab,line)){
	    word[loop] = strdup(line);
	    if (HASH_OK != hash_insert(&word_tab,word[loop],(int)word[loop])){
		printf("fatal HASH Error\n");
		exit(2);
	    }
	}
    }
    fclose(fin);

    nword = loop;

    qsort(word,nword,sizeof(char *),alphasort);

    for (loop = 0; loop < nword; loop ++){
	if ((loop % 100) == 0)
	    fprintf(stderr,"\r%d     ",loop);

	if (word[loop]==NULL)
	    continue;


	recurse_levenshtein(loop,nword);
    }
}
