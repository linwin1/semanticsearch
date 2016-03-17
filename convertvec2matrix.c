#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HPUX
#include "getopt.h"
#include <netinet/in.h>
#else
#include <getopt.h>
#include <arpa/inet.h>
#endif
#include <sys/time.h>
#include <sys/resource.h>
#include "svdlib.h"
#include <math.h>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

typedef struct {
    unsigned int row;
    float value;
} value_t;

typedef struct {
    int id;
    char* word;
    float data;  //tf
    float tfidf;
} VectorElement_t;

typedef struct {
    int ArtNum;
    float Length;
    int nElements;
    VectorElement_t *List;
} Vector_t;

typedef struct {
    unsigned int ArticleNumber;
    unsigned long long FileOffset;
}  __attribute__ ((__packed__)) VectorOffset_t;


FILE *fdin,*fdout;
VectorOffset_t Offset[200100];

int cmpvec(const void *a, const void *b)
{
    return (((VectorElement_t *)(a))->id - ((VectorElement_t *)(b))->id);
}

int cmpvecidf(const void *a, const void *b)
{
    if (((VectorElement_t *)(b))->tfidf < ((VectorElement_t *)(a))->tfidf)
	return -1;

    if (((VectorElement_t *)(b))->tfidf > ((VectorElement_t *)(a))->tfidf)
	return 1;

    return 0;
}


Vector_t VectorList;

int readVectorHeader(FILE *fin, int *recordlength, int *ArticleNumber, float *Length)
{
    int WI;
    float WF;

    int nElements = 0;
    *ArticleNumber = 0;
    *Length = 0.;

    //    fflush(stdout);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    *recordlength = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    *ArticleNumber = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    nElements = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    int tmp=ntohl(WI);
    memcpy(Length,&tmp,sizeof(int));
    //    printf("Length = %f\n",*Length);
    //    printf("ArtNr: %d has %d Vector elements\n",*ArticleNumber,nElements);


    return nElements;
}


int readVectorElements(FILE *fin, int recordlength, int nElements,  VectorElement_t *vElement)
{
    static int first=1;
    int loop,wordlength;
    char word[2000];
    static char *buf;
    char *ptr;
    int WI;
    
    if (first){
	buf = calloc(11000000,1);
	first = 0;
    }
    ptr = buf;

    if (recordlength > 10000000){
	fprintf(stderr,"Input buffer too small\n");
	exit(1);
    }


    if (1 != fread(buf,recordlength,1,fin))
	return 0;

    //    printf("reading %u elements\n",nElements);

    for (loop = 0; loop < nElements; loop++){
	//	printf("read element %u    ",loop);
	//	fflush(stdout);
	memcpy(&WI,ptr,sizeof(int));
	vElement[loop].id = ntohl(WI);
	ptr += sizeof(int);
	memcpy(&WI,ptr,sizeof(int));
	wordlength = ntohl(WI);
	//	memcpy(&wordlength,ptr,sizeof(int));
	ptr += sizeof(int);
	memcpy(word,ptr,wordlength);
	word[wordlength]=0;
	vElement[loop].word = NULL;
	//	printf("%s\n",wordlist[vElement[loop].id]);
	ptr += wordlength;
	memcpy(&WI,ptr,sizeof(int));
	int tmp;
	tmp = ntohl(WI);
	memcpy(&vElement[loop].data,&tmp,sizeof(int));
	ptr += sizeof(int);
	memcpy(&WI,ptr,sizeof(int));
	tmp = ntohl(WI);
	memcpy(&vElement[loop].tfidf,&tmp,sizeof(int));	
	ptr += sizeof(int);
	//	printf("%s %f %f\n",vElement[loop].word,vElement[loop].data,vElement[loop].tfidf);
    }

    //    printf("%d vector elements read\n",nElements);

    return nElements;
    
}

WriteMatrixHeader(int dimm, int dimn)
{
    int tmp,loop;

    fseek(fdout,0,SEEK_SET);

    tmp = htonl(dimm);
    fwrite(&tmp,sizeof(int),1,fdout);
    tmp = htonl(dimn);
    fwrite(&tmp,sizeof(int),1,fdout);

    fwrite(Offset,sizeof(VectorOffset_t),dimm,fdout);
}


WriteVectorToMatrix(Vector_t *VectorList)
{
    int loop;
    int tmp;
    unsigned short stmp;

    Offset[VectorList->ArtNum].ArticleNumber = VectorList->ArtNum;
    Offset[VectorList->ArtNum].FileOffset    = ftell(fdout);    

    tmp = htonl(VectorList->ArtNum);
    fwrite(&tmp,sizeof(int),1,fdout);
    tmp = htonl(VectorList->Length);
    fwrite(&tmp,sizeof(int),1,fdout);
    tmp = htonl(VectorList->nElements);
    fwrite(&tmp,sizeof(int),1,fdout);

    for (loop = 0; loop < VectorList->nElements; loop++){
	tmp = htonl(VectorList->List[0].id);
	fwrite(&tmp,sizeof(int),1,fdout);
	tmp = htonl(VectorList->List[0].tfidf);
	fwrite(&tmp,sizeof(int),1,fdout);
    }
	
}


unsigned int read_vectors(FILE *fin,unsigned long *TotalNonZeroValues, unsigned long *m, unsigned long *n)
{
    unsigned int count = -1;
    int nElements;
    int MaxElements = 0, MinElements = 10000;
    unsigned int total_length = 0;
    int len = 0;
    int ArticleNumber,tmp,loop;
    float Length;
    unsigned long TotalElements = 0;
    unsigned int min_id;

    min_id = 100000000;
    *n = 0;
    while(0 != (nElements = readVectorHeader(fin,&len,&ArticleNumber,&Length))){
	count++;
	TotalElements += nElements;
	total_length += len;
	if (nElements > MaxElements) 
	    MaxElements = nElements;

	if (nElements < MinElements)
	    MinElements = nElements;
	
	if ((count % 1000) == 0){
	    fprintf(stderr,"\rArticle Number %d",count);
	    fflush(stderr);
	}

	//	printf("\n\nArticle Number = %d\n",ArticleNumber+1);

	VectorList.ArtNum = ArticleNumber;
	VectorList.nElements = nElements;
	VectorList.Length = Length;
	VectorList.List = (VectorElement_t *)calloc(nElements,sizeof(VectorElement_t));
	if (VectorList.List == NULL){
	    fprintf(stderr,"not enough core to allocate %d bytes nElements = %d\n",
		    (int)(nElements * sizeof(VectorElement_t)),nElements);
	    exit(-1);
	}
	if (readVectorElements(fin,len,nElements,VectorList.List) == 0)
	    return 0;

	*n = max(*n,VectorList.List[VectorList.nElements-1].id);
	WriteVectorToMatrix(&VectorList);
	if (VectorList.List != NULL)
	    free(VectorList.List);


    }

    *m = count+1;

    printf("\nTotal length = %u\n",total_length);
    printf("Total Elements = %lu\n",TotalElements);
    *TotalNonZeroValues = TotalElements;
    printf("Minimal Elements = %d\n",MinElements);
    printf("Maximal Elements = %d\n",MaxElements);
    printf("Average Elements = %.2lf\n",((float)TotalElements)/count);
    printf("ID min = %u ID max = %lu\n",min_id,*n);
    
    ++*n;

    printf("N = %lu\n",*n);

    return TotalElements;
}


main(int argc, char *argv[])
{
    int val;
    unsigned long TotalElements,dimM,dimN,row,col,colcount,tmp,newdim=0;
    char *word;
    unsigned int WI;
    VectorElement_t key,*res;
    char *InputName,*OutputName;

    /*
    printf("sizeof(int) = %d\n",sizeof(int));
    printf("sizeof(long long) = %d\n",sizeof(long long));
    printf("sizeof(VectorOffset_t) = %d\n",sizeof(VectorOffset_t));
    exit(0);
    */

    while ((val = getopt (argc, argv, "i:o:")) != EOF){
	switch (val) {
	case 'i':
	    InputName = strdup(optarg);
	    break;
	case 'o':
	    OutputName = strdup(optarg);
	    break;
	default:
	    printf("-i infile\n");
	    printf("-o outfile\n");
	    exit(1);
	}
    }

    if (InputName != NULL){
	if(NULL == (fdin=fopen(InputName,"rb"))){
	    fprintf(stderr,"can't open matrix %s\n",InputName);
	    exit(1);
	}
    }
    
    if (fdin == NULL){
	printf("can't open input file %s\n",InputName);
	exit(1);
    }

    if (OutputName != NULL){
	if(NULL == (fdout=fopen(OutputName,"wb"))){
	    fprintf(stderr,"can't open output matrix %s\n",OutputName);
	    exit(1);
	}
    }
    
    if (fdout == NULL){
	printf("can't open output file %s\n",OutputName);
	exit(1);
    }


    WriteMatrixHeader(200100,0);
    read_vectors(fdin,&TotalElements,&dimM,&dimN);
    WriteMatrixHeader(dimM,dimN);
    printf("Elements != 0    : %lu\n",TotalElements);
    printf("Matrix dimensions: %lu x %lu\n",dimM,dimN);
    fclose(fdin);
}
