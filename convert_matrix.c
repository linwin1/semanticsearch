#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



#ifdef _WIN32
#include "getopt.h"
#include <winsock2.h>
#pragma comment (lib , "ws2_32.lib")
#pragma comment (lib , "getopt.lib")
#ifdef WITH_SVD
#pragma comment (lib , "svdlibc.lib")
#endif
//float fabsf(float A)
//{
//	return fabs(A);
//}
#elif HPUX
#include "getopt.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#else
#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif


#include "svdlib.h"

#ifndef _WIN32
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

int SkipCalculation = FALSE;
int SVD_Dimension = 200;
int CalcOnly = FALSE;

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

int cmpvec(const void *a, const void *b)
{
    return (((VectorElement_t *)(a))->id - ((VectorElement_t *)(b))->id);
}

typedef struct {
    unsigned int *row;
    unsigned int size;
    float *value;
} column_t;


column_t *column;


//VectorElement_t vectorElement[5000];

Vector_t VectorList[200100];
char *wordlist[600000] = {0};

void writeVectorHeader(FILE *fdvector, int ArtNum,int nElements,int totalStringLength, float Length)
{
    int WI;

    int recordlength = nElements*(4*sizeof(int)) + totalStringLength;
    WI = htonl(recordlength);
    fwrite(&WI,sizeof(int),1,fdvector);
    WI = htonl(ArtNum);
    fwrite(&WI,sizeof(int),1,fdvector);
    WI = htonl(nElements);
    fwrite(&WI,sizeof(int),1,fdvector);
    memcpy(&WI,&Length,sizeof(int));
    WI = htonl(WI);
    fwrite(&WI,sizeof(int),1,fdvector);
}

void writeVectorElement(FILE *fdvector, VectorElement_t *v)
{
    int wordlength = strlen(v->word);
    int WI;

    WI = htonl(v->id);
    //    printf("ID = 0x%x\n",vectorElement[n].id);
    fwrite(&WI,sizeof(int),1,fdvector);
    WI = htonl(wordlength);
    fwrite(&WI,sizeof(int),1,fdvector);
    fwrite(v->word,wordlength,1,fdvector);
    memcpy(&WI,&v->data,sizeof(int));
    WI = htonl(WI);
    fwrite(&WI,sizeof(int),1,fdvector);
    memcpy(&WI,&v->tfidf,sizeof(int));
    WI = htonl(WI);
    fwrite(&WI,sizeof(int),1,fdvector);
}


int readVectorHeader(FILE *fin, int *recordlength, int *ArticleNumber, float *Length)
{
    int WI;
//    float WF;

    int nElements = 0,tmp;
    *ArticleNumber = 0;
    *Length = 0.;

    //    printf("offset = 0x%lx\n",ftell(fin));
    //    fflush(stdout);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    *recordlength = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    *ArticleNumber = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    nElements = ntohl(WI);
    if (0 == fread(&WI,sizeof(int),1,fin)) return 0;
    tmp=ntohl(WI);
    memcpy(Length,&tmp,sizeof(int));
    //    printf("Length = %f\n",*Length);
    //    printf("ArtNr: %d has %d Vector elements\n",*ArticleNumber,nElements);
    return nElements;
}


int readVectorElements(FILE *fin, int recordlength, int nElements,  VectorElement_t *vElement)
{
    static int first=1;
    int loop,wordlength,tmp;
//    char word[2000];
    static char *buf;
    char *ptr;
    int WI;


    if (first){
	buf = calloc(0x40000,1);
	first = 0;
    }
    ptr = buf;

    if (recordlength > 0x30000){
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
	//	memcpy(word,ptr,wordlength);
	//	word[wordlength]=0;
	//	if (wordlist[vElement[loop].id] == NULL)
	//	    wordlist[vElement[loop].id] = strdup(word);
	//	vElement[loop].word = strdup(word);
	//	printf("%s\n",wordlist[vElement[loop].id]);
	ptr += wordlength;
	memcpy(&WI,ptr,sizeof(int));
	
	tmp = ntohl(WI);
	memcpy(&vElement[loop].data,&tmp,sizeof(int));
	ptr += sizeof(int);
	memcpy(&WI,ptr,sizeof(int));
	tmp = ntohl(WI);
	memcpy(&vElement[loop].tfidf,&tmp,sizeof(int));	
	ptr += sizeof(int);
	//	printf("%u %f %f\n",vElement[loop].id,vElement[loop].data,vElement[loop].tfidf);
    }

    //    printf("%d vector elements read\n",nElements);

    return nElements;
    
}

unsigned int read_vectors(FILE *fin,unsigned long *TotalNonZeroValues, unsigned long *m, unsigned long *n)
{
    unsigned int count = -1;
    unsigned int nElements;
    unsigned int MaxElements = 0, MinElements = 10000;
    unsigned int total_length = 0;
    int len = 0;
    int ArticleNumber,tmp,loop;
    float Length;
    unsigned long TotalElements = 0;
    unsigned int min_id;
	unsigned int colidx,size;

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

	VectorList[count].ArtNum = ArticleNumber;
	VectorList[count].nElements = nElements;
	VectorList[count].Length = Length;
	VectorList[count].List = (VectorElement_t *)calloc(nElements,sizeof(VectorElement_t));
	if (VectorList[count].List == NULL){
	    fprintf(stderr,"not enough core to allocate %d bytes nElements = %d\n",
		    nElements * sizeof(VectorElement_t),nElements);
	    exit(-1);
	}
	if (readVectorElements(fin,len,nElements,VectorList[count].List) == 0)
	    return 0;

	//	printf("all elements read\n");
	//	fflush(stdout);

	
	for(loop=0;loop < nElements; loop++){
	    //	    printf("size =%u",column[VectorList[count].List[loop].id].size );
	    //	    fflush(stdout);

	    colidx = VectorList[count].List[loop].id;
	    //	    printf("colidx = %u  new colidx.size = %u\n",colidx,column[colidx].size);
	    //	    fflush(stdout);
	    size = column[colidx].size;
	    column[colidx].row = (unsigned int*)realloc(column[colidx].row,  (size+1)* sizeof(int));
	    //	    if (column[colidx].row == NULL)
	    //		printf("alloc failed 1\n");

	    //	    fflush(stdout);
	    column[colidx].row[column[colidx].size] = count;
	    column[colidx].value = (float *)realloc(column[colidx].value,(size+1) * sizeof(float));

	    column[colidx].value[column[colidx].size] = (float)
		VectorList[count].List[loop].data * VectorList[count].List[loop].tfidf;
	    column[colidx].size = size+1;
	    //	    printf("size after insert = %u\n",size+1);
	    //	    fflush(stdout);
	    
	}

	min_id = min(VectorList[count].List[0].id,min_id);
	*n = max(VectorList[count].List[nElements-1].id,*n);
	//	printf("n = %lu - %d\n",*n,VectorList[count].List[nElements-1].id);
#if 0
	int loop;
	VectorElement_t *vectorElement = VectorList[count].List;
	for (loop = 0; loop < nElements; loop++){
	    printf("count = %d  ",count);	    
	    printf("ID    = %d  ",vectorElement[loop].id);
	    printf("word  = %s  ",vectorElement[loop].word);
	    printf("data  = %d  ",vectorElement[loop].data);
	    printf("tdidf = %.5lf\n",vectorElement[loop].tfidf);
	}
#endif
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

/***********************************************************************
 *                                                                     *
 *                        timer()                                      *
 *            Returns elapsed cpu time (float, in seconds)             *
 *                                                                     *
 ***********************************************************************/
#ifdef _WIN32
float timer(void) 
{
	FILETIME CreationTime, ExitTime, KernelTime, UserTime;
	ULARGE_INTEGER Kernel,User;
	LARGE_INTEGER Sum;
	float total=0.;

	if (GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime))
		{
		Kernel.LowPart = KernelTime.dwLowDateTime;
		Kernel.HighPart = KernelTime.dwHighDateTime;
		User.LowPart = UserTime.dwLowDateTime;
		User.HighPart = UserTime.dwLowDateTime;
		Sum.QuadPart = Kernel.QuadPart+User.QuadPart;
		return (float)((double)Sum.QuadPart / 10000000.);
		}

	return -1.;
}

#else 
float timer(void) {
  long elapsed_time;
  struct rusage mytime;
  getrusage(RUSAGE_SELF,&mytime);
 
  /* convert elapsed time to milliseconds */
  elapsed_time = (mytime.ru_utime.tv_sec * 1000 +
                  mytime.ru_utime.tv_usec / 1000);
  
  /* return elapsed time in seconds */
  return((float)elapsed_time/1000.);
}

unsigned long mikro_diff(void) {
    static unsigned long lasttime=0;
    unsigned long elapsed_time,tmp;
  struct rusage mytime;
  getrusage(RUSAGE_SELF,&mytime);
 
  /* convert elapsed time to milliseconds */
  elapsed_time = (mytime.ru_utime.tv_sec * 1000000 +
                  mytime.ru_utime.tv_usec );
  
  /* return elapsed time in seconds */
  tmp = lasttime;
  lasttime = elapsed_time;
  return(elapsed_time-tmp);
}
#endif

#ifdef WITH_SVD
calc_svd(FILE* fdout, char *InputName, char *BaseName)
{
    SVDRec R = NULL;
    SVDRec Rt = NULL;
    SMat A = NULL;
    float start;
	float *line;
	unsigned int row,col,ucol,nElements,loop,totalStringLength;
    float tmp;
    float *RtUtRow;
    float *RtVtRow;
	unsigned int cnt = 0;




    char UtName[1024],VtName[1024],SName[1024];

    A =  svdLoadSparseMatrix(InputName, SVD_F_SB);
    //    Ad = svdConvertStoD(A);
    //    svdWriteDenseMatrix(Ad, "initial-A.txt", SVD_F_DT);

    if (BaseName){
	sprintf(UtName,"%s-Ut.mtx",BaseName);
	sprintf(VtName,"%s-Vt.mtx",BaseName);
	sprintf(SName,"%s-S.txt",BaseName);
    }
    else{
	sprintf(UtName,"res-Ut.mtx");
	sprintf(VtName,"res-Vt.mtx");
	sprintf(SName,"res-S.txt");
    }


    printf("Output Filenames\n");
    printf("Ut =  %s\n",UtName);
    printf("Vt =  %s\n",VtName);
    printf("S  =  %s\n",SName);

    fflush(stdout);

    if (SkipCalculation == FALSE){
	printf("Start calculation\n");
	fflush(stdout);

	start = timer();

	if (!(R = svdLAS2A(A, SVD_Dimension)))
	    printf("error\n");

	printf("\nCPU time used: %.2f\n",timer()-start);
	fflush(stdout);
    
	
	svdWriteDenseMatrix(R->Ut,UtName, SVD_F_DB);
	svdWriteDenseMatrix(R->Vt, VtName, SVD_F_DB);
	svdWriteDenseArray(R->S, R->d, SName, FALSE);    
	if (CalcOnly)
	    exit(0);
    }
    else{
	R->Ut = svdLoadDenseMatrix(UtName, SVD_F_DB);
	R->Vt = svdLoadDenseMatrix(VtName, SVD_F_DB);
	R->S  = svdLoadDenseArray(SName, &R->d, FALSE);    
    }

    //    ANew = svdNewDMat(Ad->rows, Ad->cols);

    printf("Start Tranposing\n");
//    mikro_diff();

    Rt = svdNewSVDRec();

    Rt->Ut = svdTransposeD(R->Ut);
    svdFreeDMat(R->Ut);
    Rt->Vt = svdTransposeD(R->Vt);
    svdFreeDMat(R->Vt);

//  printf("End. %lu us needed\n",mikro_diff());

    line = (float *)calloc(A->cols,sizeof(float));


    fprintf(stderr,"\n");
    printf("A->rows = %u\n",A->rows);
    printf("A->cols = %u\n",A->cols);
    printf("R->Ut->rows = %u\n",Rt->Ut->rows);
    printf("R->Ut->cols = %u\n",Rt->Ut->cols);
    printf("R->Vt->rows = %u\n",Rt->Vt->rows);    
    printf("R->Vt->cols = %u\n",Rt->Vt->cols);    
    

    fflush(stdout);

    printf("Start Mul 1\n");
//    mikro_diff();

    for (row = 0; row < A->rows; row++){
	RtUtRow = Rt->Ut->value[row];
	for (ucol = 0; ucol < Rt->Ut->cols; ucol++){
	    RtUtRow[ucol] = RtUtRow[ucol] * R->S[ucol];
	}	
    }

    printf("End Mul 1\n");

    fflush(stdout);

    for (row = 0; row < A->rows; row++){
	nElements = 0;
	RtUtRow = Rt->Ut->value[row];
	for (col = 0; col < A->cols; col++){
	    tmp = 0;
	    line[col] = 0;

	    RtVtRow = Rt->Vt->value[col];
	    for (ucol = 0; ucol < Rt->Ut->cols; ucol++){
		tmp += RtUtRow[ucol] * RtVtRow[ucol];
		//tmp += Rt->Ut->value[row][ucol]*R->S[ucol]*Rt->Vt->value[col][ucol];
	    }
	    
	    if (fabsf(tmp) > 1.e-4){
		line[col] = tmp;
		nElements++;
	    }
	}
	VectorList[row].List = (VectorElement_t *)realloc(VectorList[row].List,nElements*sizeof(VectorElement_t));
	VectorList[row].ArtNum = row;
	VectorList[row].nElements = nElements;
	VectorList[row].Length = 0.0;

	totalStringLength = 0;
	
	for (loop = 0; loop < A->cols; loop++){
	    if (fabsf(line[loop]) < 1.e-4)
		continue;

	    VectorList[row].List[cnt].data = 1.0;
	    VectorList[row].List[cnt].tfidf = line[loop];
	    VectorList[row].List[cnt].id = loop;
	    VectorList[row].List[cnt].word = wordlist[loop];
	    totalStringLength += strlen(wordlist[loop]);
	    VectorList[row].Length += line[loop]*line[loop];
	    cnt++;
	    //	    printf("%.2f  ",line[loop]); 
	}

	fprintf(stderr, "\rWriting %u elements to row %u           ",nElements,row+1);

	writeVectorHeader(fdout, row,nElements,totalStringLength,VectorList[row].Length);
	for (loop = 0; loop < nElements; loop++){
	    writeVectorElement(fdout,&VectorList[row].List[loop]);
	}

	if(VectorList[row].List != NULL){
	    free(VectorList[row].List);
	    VectorList[row].List = NULL;
	}
    }
    
    fprintf(stderr,"\ndone!\n");
    //    svdWriteDenseMatrix(ANew,"res-Anew.txt",SVD_F_DT);
	
    
}
#endif


int main(int argc, char *argv[])
{
    int val;
    FILE *fdin,*fout;
    unsigned long TotalElements,dimM,dimN,row,col,colcount,tmp,newdim=0;
    char *word;
    unsigned int WI;
    VectorElement_t key,*res;
    char *InputName,*OutputName,*OutputVector = NULL,*BaseName = NULL;
	float ftmp;
	FILE *fdvecnew;

    while ((val = getopt (argc, argv, "d:sb:i:o:v:x")) != EOF){
	switch (val) {
	case 'b':
	    BaseName = strdup(optarg);
	    break;
	case 'i':
	    InputName = strdup(optarg);
	    break;
	case 'o':
	    OutputName = strdup(optarg);
	    break;
	case 'v':
	    OutputVector = strdup(optarg);
	    break;
	case 's':
	    SkipCalculation = TRUE;
	    break;
	case 'd':
	    SVD_Dimension = atoi(optarg);
	    printf("SVD Dimension = %d\n",SVD_Dimension);
	    break;
	case 'x':
	    CalcOnly = TRUE;
	    break;

	default:
	    printf("-i infile -o outfile\n");
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


    column = (column_t *)calloc(sizeof(column_t),1200000);
			      
    read_vectors(fdin,&TotalElements,&dimM,&dimN);
    printf("Elements != 0    : %lu\n",TotalElements);
    printf("Matrix dimensions: %lu x %lu\n",dimM,dimN);
    fclose(fdin);

    if (OutputName != NULL)
	if(NULL == (fout=fopen(OutputName,"wb"))){
	    fprintf(stderr,"can't open matrix output file %s\n",OutputName);
	    exit(1);
	}
	    
    WI = htonl(dimM);
    fwrite(&WI,sizeof(int),1,fout);
    WI = htonl(dimN);
    fwrite(&WI,sizeof(int),1,fout);
    WI = htonl(TotalElements);
    fwrite(&WI,sizeof(int),1,fout);

    for (col = 0; col < dimN; col++){	
	colcount = 0;
	if ((col % 100) == 0)
	    fprintf(stderr,"\rProcessing Col %lu / %.2f%% done                ",col,100.*((float)col)/dimN);

	if (column[col].size == 0){
	    printf("col %u is empty?\n",col);
	    continue;
	}

	newdim++;
	WI = htonl(column[col].size);
	fwrite(&WI,sizeof(int),1,fout);
	for( tmp = 0; tmp < column[col].size; tmp ++){
	    WI = htonl(column[col].row[tmp]);
	    fwrite(&WI,sizeof(int),1,fout);
	    //		printf("row = %d\n",collist[tmp].row);
	    ftmp = column[col].value[tmp];
	    memcpy(&WI,&ftmp,sizeof(int));
	    WI = htonl(WI);
	    //		printf("value = %.3f\n",ftmp);
	    fwrite(&WI,sizeof(int),1,fout);
	}	
    }
	
	
    fclose(fout);

    for (col = 0; col < dimN; col++){	
	if (column[col].row != NULL)
	    free(column[col].row);
	if (column[col].value != NULL)
	    free(column[col].value);
    }

    free(column);
#ifdef WITH_SVD
    printf("\nNew Dimension = %lu x %lu\n",dimM,newdim);

    fflush(stdout);

    if (OutputVector){
	fdvecnew = fopen(OutputVector,"wb");
	calc_svd(fdvecnew,OutputName,BaseName);
	fclose(fdvecnew);
    }
#endif
}
