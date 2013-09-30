/*
Mattia Mattaboni (mattaboni@aero.polimi.it) reworked this file for use within
RTAI-Lab.
*/

/* AutoCode_LicensedWork --------------------------------------------------

This product is protected by the National Instruments License Agreement
which is located at: http://www.ni.com/legal/license.

(c) 1987-2004 National Instruments Corporation. All rights reserved.

-------------------------------------------------------------------------*/
/*
**  File       : sa_utils.c
**  Project    : Autocode/C
**
**  Abstract:
**
**      Definitions for AutoCode files.
**
**  The following routines provide a set of utility procedures
**  that can be used with the code generated by AutoCode/C.
**  These routines allow testing of the generated code in a host
**  environment and provide a link with Xmath via MATRIXx ASCII formatted 
**  output files.
*/

#if (__STDC__ || defined(__cplusplus) || defined(c_plusplus) || defined(RS_VXWORKS))
#include <stdlib.h>
#include <string.h>
#else
extern int strcmp();
extern void exit();
#endif

#include <stdio.h>
#include <math.h>
#include "sa_sys.h"
#include "sa_defn.h"
#include "sa_types.h"
#include "sa_math.h"

#if defined(RS_VXWORKS)
#include <sysLib.h>
#endif

#if (ANSI_PROTOTYPES)
static void fatalerr(RT_INTEGER errorCode);
#else
static void fatalerr();
#endif

/*  Storage allocation parameters.
 
 *  To change storage size limits, modify the following parameters.
 *  ------------------------------------------------------------------
 *  MAXU    = max number of storage elements for input values in U
 *  MAXUTIM = max number of input time points in UTIME
 *  MAXY    = max number of storage elements for output values in Y
 *  MAXYTIM = max number of output time points in YTIME             */

#define   MAXU              100000L
#define   MAXUTIM           10000L

#if defined(OSF1)
#define   MAXY              120000L
#define   MAXYTIM           30000L
#else
#define   MAXY              80000L
#define   MAXYTIM           20000L
#endif

#define   REQVER            700
#define   REQVAR            2

#define   DOUBLE_PRECISION  TRUE

#define   EPSILON_SA           1.49011611938476562E-08
#define   EPS_SA               (4.0 * EPSILON_SA)

#define OUT_OF_INPUT_DATA 17 

/* Global Variables */

extern char *DATA_FILE_LOCATION;

static   RT_INTEGER         UCOUNT;
static   RT_INTEGER         ICNT;
static   RT_INTEGER         YCOUNT;
static   RT_INTEGER         YCOUNTT;
static   RT_INTEGER         TCOUNT;
static   RT_INTEGER         ROWU;
static   RT_INTEGER         COLU;
static   RT_INTEGER         ROWY;
static   RT_INTEGER         COLY;

static   RT_FLOAT           UTIME[MAXUTIM+1];
static   RT_FLOAT           U[MAXU+1];
static   RT_FLOAT           YTIME[MAXYTIM+1];
static   RT_FLOAT           Y[MAXY+1] = {-EPSILON_SA};
static   RT_FLOAT           XDELTAT; 
static   RT_FLOAT           XT; 

static   FILE               *fp;

static   RT_INTEGER         NUMIN;
static   RT_INTEGER         NUMOUT ;

static   RT_FLOAT           *XINPUT;
static   RT_FLOAT           *XOUTPUT;
static   RT_INTEGER         SCHEDULER_STATUS;
static   RT_BOOLEAN         errorFound;

RT_BOOLEAN                  outOfInputData;  /* ANITA */
RT_INTEGER                  numInputData; /* ANITA */
static   void               *clkISR;


/* -----------------------------------------------------------------------------
 *  The following routine simulates input I/O operation by sampling values from
 *  a memory buffer.
 */
#if !defined(FILE_IO)
RT_INTEGER SA_External_Input()
{
    int i;

    /* Dummy initialization. Init with appropriate */
    /* values for your model */

    for (i=0; i<NUMIN; i++)
	 XINPUT[i] = 0;

    /* For super_cruise model, we recommend the */
    /* foll. initial values. If this or similar */
    /* values are not used, then expect to see  */
    /* unexplained behavior. */
/* 
    XINPUT[0] = 0;
    XINPUT[1] = 0;
    XINPUT[2] = 1;
    XINPUT[3] = 0;
    XINPUT[4] = 0.1;
    XINPUT[5] = 0;
    XINPUT[6] = 0.4;
    XINPUT[7] = 0.01;
    XINPUT[8] = 0;
    XINPUT[9] = 0;
*/

    return OK;
}

#else
RT_INTEGER SA_External_Input()
{

    RT_INTEGER  i, j;
    RT_FLOAT    ALPHA;
    
    if ( (numInputData <= 0 && NUMIN > 0) || TCOUNT > (0.5*MAXY-2)  ) /* exhausted all input */
    {
	 outOfInputData = TRUE;
	 return ERROR;
    }
    else numInputData--;
    /* Get next input values by interpolation */

    XT = TCOUNT*XDELTAT;
    TCOUNT++;   

    if( NUMIN > 0 ){ 
	for( i=ICNT; i<UCOUNT; i++ ){ 
	   if( XT >= UTIME[i]  &&  XT <= UTIME[i+1] ){ 
	      ALPHA = (XT - UTIME[i]) / (UTIME[i+1]-UTIME[i]);
	      ICNT = i;
	      for( j=1; j<=NUMIN; j++ ){ 
		  XINPUT[j-1] = ALPHA * (U[j+i*COLU]-U[j+(i-1)*COLU]) 
				  + U[j+(i-1)*COLU];
	      } 
	      break;
	   } 
	} 
    } 
    return SCHEDULER_STATUS;
}
#endif /* !defined(FILE_IO) */

/* -----------------------------------------------------------------------------
 *  The following routine simulates output I/O operations by providing values 
 *  to an output buffer in memory.
 */
#if !defined(FILE_IO)
RT_INTEGER SA_External_Output()
{
    return OK;
}
#else
RT_INTEGER SA_External_Output()
{
    RT_INTEGER  i;
    static int cnt = 0;

    /* Copy last outputs */

    for(  i=1; i<=NUMOUT; i++ ){ 
	YTIME[YCOUNT] = XT;
	if( YCOUNT>1 ){ 
	   Y[i+(YCOUNT-1)*COLY] = Y[i+(YCOUNT-2)*COLY];
	}
    } 
    YCOUNT++;

    /* Deposit new outputs */

    for(  i=1; i<=NUMOUT; i++ ){ 
	/* Outputs from scheduler are zero-based */
	YTIME[YCOUNT] = XT;    
	Y[i+(YCOUNT-1)*COLY] = XOUTPUT[i-1];
    }
    YCOUNT++;
    return SCHEDULER_STATUS;
}
#endif /* !defined(FILE_IO) */


/* -----------------------------------------------------------------------------
 *  The following routine initializes I/O by loading input data 
 *  from a Xmath 'MATRIXx ASCII' formatted file.  The file can contain at
 *  most two arrays.  The first array in the file must be a
 *  column vector of time points.  The second array (dimensioned 
 *  the number of time points by the number of input signals) is 
 *  a set of input values that corresponds to the time points.  
 *  This second array is not required if the number of external inputs
 *  is zero.  This file can be created in Xmath by saving the 'pdm' of input
 *  and time vectors with {matrixx, ascii} options.
 *  The C version of RTUTILS can only read FSAVE files written with
 *  the Fortran format (1P3D25.17).
 */ 
void Implementation_Initialize(RT_FLOAT BUS_IN[],  RT_INTEGER NI, 
				   RT_FLOAT BUS_OUT[], RT_INTEGER NO,
				   RT_FLOAT SCHEDULER_FREQ, void *clkISRptr)
{

    char               line[81], FILENAME[81], str1[20], str2[20], LFORM[20];
    RT_INTEGER         VERSION, NUMVAR;
    RT_INTEGER         i, j, x, IIMG, arg_index, row, col, E, reqvar=2;
    RT_INTEGER         IROW, ICOL;
    RT_FLOAT           arg[3];
    char               *readformat;
    char               outFile[512], inFile[512];

    NUMIN  = NI;
    NUMOUT = NO;
    SCHEDULER_STATUS = 0;

    XINPUT  = BUS_IN;
    XOUTPUT = BUS_OUT;
    clkISR  = clkISRptr;

#if defined(FILE_IO)

    for(  i=1; i<=NUMOUT; i++ ){
	   Y[i] = XOUTPUT[i-1];
    }
    /* Request and open Xmath {MATRIXx, ASCII} format input file */

    printf( " Enter Xmath {matrixx, ascii} formatted input filename: " );
    scanf ( "%s", FILENAME );
    sprintf(inFile, "%s/%s", DATA_FILE_LOCATION, FILENAME);
    /*if( ( fp = fopen(FILENAME, "r") ) == NULL ){ */
    if( ( fp = fopen(inFile, "r") ) == NULL ){
	E = 315;
	fatalerr( E );
    } 

    /* Process FSAVE file header line and then skip directory */

    fgets( line, 81, fp );
    /* check for bad file format */
    if(sscanf( line, "%8s %8s %d %d", str1, str2, &VERSION, &NUMVAR)  != 4 ||
	strcmp(str1, "MATRIXx")  ||  strcmp(str2, "VERSION" ) ){ 
	fclose ( fp );
	E=301;
	fatalerr( E );
    } 
    
    /* check version number */
    
    if( VERSION < REQVER ){ 
	fclose ( fp );
	E=303;
	fatalerr( E );
    } 
    
    /* check for no more than two input arrays */
    
    if( NUMVAR > reqvar ){ 
	fclose ( fp );
	E=305;
	fatalerr( E );
    } 
    
    /* skip directory (a single line for two arrays or less)  */
    
    fgets ( line, 81, fp );
    
    /* Load input time vector */
    
    fgets ( line, 81, fp );
    sscanf( line, "%11s %d %d %d %12s", str1, &IROW, &ICOL, &IIMG, LFORM );
    
    /* check for single column and not imaginary */
    
    if( ICOL!=1  ||  IIMG ){ 
	fclose ( fp );
	E=307;
	fatalerr( E );
    } 
    
    /* check size limit on input time vector */
    
    if( IROW > MAXUTIM ){ 
	fclose ( fp );
	E=309;
	fatalerr( E );
    } 
    
    /* read in the input time vector */
    
    if (sizeof(RT_FLOAT) == sizeof(float))
	 readformat = " %f %f %f ";
    else if (sizeof(RT_FLOAT) == sizeof(double))
	 readformat = " %lf %lf %lf ";
    else {
	 fclose(fp);
	 E = 501;
	 fatalerr(E);
    }

    for( i=1; i<=IROW; i+=3 ){ 
	fgets ( line, 81, fp );
	for( j=0; line[j]; j++ ){ 
	   if( line[j]=='D' ){ 
	      line[j]='E'; 
	   } 
	} 
	x = sscanf( line, readformat, &UTIME[i], &UTIME[i+1], &UTIME[i+2] );
	if ((i > 3  && (UTIME[i]   - UTIME[i-1] <= EPSILON_SA)) || 
	    (x == 2 && (UTIME[i+1] - UTIME[i]   <= EPSILON_SA)) || 
	    (x == 3 && (UTIME[i+2] - UTIME[i+1] <= EPSILON_SA))) {
	     fclose(fp);
	     E = 320;
	     fatalerr(E);
	     break;
	}
    } 
    UCOUNT = IROW;
    
    if(UTIME[1] != 0.0) {
	fclose ( fp );
	E=319;
	fatalerr( E );
    }
    
    /* Load input array if number of external inputs .GT. zero */

    if( NUMIN>0  &&  NUMVAR==reqvar ){ 
	fgets ( line, 81, fp );
	sscanf( line, "%11s %d %d %d %12s", str1, &ROWU, &COLU, &IIMG, LFORM );
    
	/* check for correct number of inputs and not imaginary */

	if( COLU != NUMIN  ||  IIMG ){ 
	   fclose ( fp );
	   E=311;
	   fatalerr( E );
	} 
    
	/* check size limit on input value array */
    
	if( ROWU*COLU > MAXU ){ 
	   fclose ( fp );
	   E=313;
	   fatalerr( E );
	} 
    
	/* read in the input value array (row-wise) 
	 * MATRIXx stores vectors column-wise    */
	
	for( col=1; col<=COLU; col++ ){ 
	   for( row=1; row<=ROWU; row++ ){ 
	      arg_index = ((col-1) * ROWU + row - 1) % 3;
	      if( arg_index  ==  0 ){ 
		  fgets ( line, 81, fp );
		  for(  j=0; line[j]; j++ ){ 
		     if( line[j]=='D' ){ 
			 line[j]='E'; 
		     } 
		  } 
		  sscanf( line, readformat, arg, arg+1, arg+2 );
	      } 
	      U[ (row-1)*COLU + col ] = arg[ arg_index ];
	   } 
	} 
    } else if (NUMIN != 0) {
	   fclose ( fp );
	   E=321;
	   fatalerr( E );
      } 
 

    /* All done with input processing; close it up */
    
    fclose (fp);
    ICNT   = 1;
    YCOUNT = 1;
    TCOUNT = 0;
    COLY   = NUMOUT;

    /* Compute number of output time points (YCOUNTT) */

    XDELTAT = 1.0/SCHEDULER_FREQ;
    XT      = -XDELTAT;
    YCOUNTT = (RT_INTEGER)(EPS_SA + (UTIME[UCOUNT]-UTIME[1]) * SCHEDULER_FREQ);

    /* Check limits on output storage */

    if( 2*(YCOUNTT+1) > MAXYTIM  ||  2*(YCOUNTT+1)*COLY > MAXY ){ 
	E=401;
	fatalerr( E );
    } 

    numInputData = YCOUNTT; /* ANITA */

    /* Request and open output file */
    
    printf( "\n Enter output filename: " );
    scanf ( "%s", FILENAME );
    sprintf(outFile, "%s/%s", DATA_FILE_LOCATION, FILENAME);

    /*if( (  fp=fopen(FILENAME,"w")  ) == NULL ){ */
    if( (  fp=fopen(outFile,"w")  ) == NULL ){
	E=317;
	fatalerr( E );
    }
    if (NUMIN > 0)
    printf( "\n Scheduler running for %6d cycles at %.7E seconds per cycle.\n",
	     YCOUNTT, XDELTAT );

#endif /* defined(FILE_IO) */
}


/* -----------------------------------------------------------------------------
 *  Generate Xmath compatible output {MATRIXx, ASCII} formatted file 
 *  from data stored in memory. The output file has already been opened by 
 *  routine Implementation_Initialize using the file-pointer fp.
 *  To be compatible with simulation output provided by Xmath,
 *  output points already computed in memory must be augmented
 *  by any additional time points occurring in the input vector.
 *  The following code segments compute the number of augmented 
 *  points (IUCNT) and write the augmented points into the output 
 *  file along with all points occuring at discrete system transitions.
 *  The resulting output file can be compared with results obtained
 *  in Xmath by setting SIM('CDELAY') and then using the sim command
 *  in the form [T1,Y1]=SIM("modelname",T,U).
 */

#define  C_FORM_D  "% .17E"          /* Format in C for Double Precision */
#define  C_FORM_S  "% .7E"           /* Format in C for Single Precision */
#define  F_FORM_D  "(1P3E25.17)"     /* Fortran Double Precision Format  */
#define  F_FORM_S  "(1P5E15.7)"      /* Fortran Single Precision Format  */

static void PrintReal_C_FORM(FILE *pFile, RT_FLOAT val)
{
     static char string[64], *p;
     int i=0;

     if( DOUBLE_PRECISION )
	  sprintf( string, C_FORM_D, val );
     else 
	  sprintf( string, C_FORM_S, val );
     
     p = string; while(*p && *p++ != 'E') i++;
     if(*p && *(p+3)) while((string[i++] = *p++)) ;
     fprintf( pFile, "%s", string );
}


void SA_Output_To_File(void)
{
#ifdef FILE_IO
     RT_INTEGER         i, IU, IY, IUCNT, IIMG;
     RT_INTEGER         ICOL;
     RT_INTEGER         ITEMS_PER_LINE;
     RT_INTEGER         CURRENT;
     RT_FLOAT           IEPS;

     IEPS  = 5.0E-6;
     IIMG  = 0;
     ICOL  = 1;
     IUCNT = 0;
     IU    = 1;
     if( DOUBLE_PRECISION ){ 
	 ITEMS_PER_LINE = 3;
     }else{ 
	 ITEMS_PER_LINE = 5;
     } 

     for( IY=1; IY<YCOUNT; IY++ ){ 
	 if( fabs( UTIME[IU]-YTIME[IY] ) <= IEPS  &&  IU < UCOUNT ){ 
	    IU++;
	 } 
	 while( UTIME[IU] < YTIME[IY]
	    &&  fabs( UTIME[IU]-YTIME[IY] ) > IEPS 
	    && IU < UCOUNT ){ 
	    IUCNT++;
	    IU++;
	 } 
     } 
     
     for( ; IU<=UCOUNT; IU++ ){ 
	 if( UTIME[IU]-YTIME[YCOUNT-1] > IEPS ){ 
	    IUCNT++;
	 } 
     } 

     /* write Xmath {MATRIXx, ASCII} formatted file header and directory */
     
     fprintf( fp, "%-44s\n","MATRIXx VERSION 700   2" );
     fprintf( fp, "YTIME     %5ld%5ldYRT       %5ld%5d\n", 
		     YCOUNT-1+IUCNT, ICOL, YCOUNT-1+IUCNT, NUMOUT );
     
     /* write the time points vector */
     
     if( DOUBLE_PRECISION ){ 
	 fprintf( fp, "YTIME     %5ld%5ld%5d%-20s\n", 
		   YCOUNT-1+IUCNT, ICOL, IIMG, F_FORM_D );
     }else{ 
	 fprintf( fp, "YTIME     %5ld%5ld%5d%-20s\n", 
		   YCOUNT-1+IUCNT, ICOL, IIMG, F_FORM_S );
     } 
     
     IU      = 1;
     CURRENT = 0;
     for( IY=1; IY<YCOUNT; IY++ ){ 
	 if( fabs( UTIME[IU]-YTIME[IY] ) <= IEPS  &&  IU < UCOUNT ){ 
	   IU++;
	 } 

	 while( UTIME[IU] < YTIME[IY]  
	    &&  fabs( UTIME[IU]-YTIME[IY] ) > IEPS 
	    && IU < UCOUNT ){ 

	    fprintf( fp, " ");
	    PrintReal_C_FORM(fp, UTIME[IU]);

	    CURRENT++;
	    if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
		fprintf( fp, "\n" );
	    } 

	    IU++;
	 } 

	 fprintf( fp, " ");
	 PrintReal_C_FORM(fp, YTIME[IY]);

	 CURRENT++;
	 if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
	    fprintf( fp, "\n" );
	 } 

     } 

     for( ; IU<=UCOUNT; IU++ ){ 
	 if( UTIME[IU]-YTIME[YCOUNT-1] > IEPS ){ 
	    fprintf( fp, " ");
	    PrintReal_C_FORM(fp, UTIME[IU]);

	    CURRENT++;
	    if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
		fprintf( fp, "\n" );
	    } 
	 } 
     }  

     /* write the output array */

     if( (CURRENT % ITEMS_PER_LINE) != 0 ){ 
	 fprintf( fp, "\n" );
     } 

     if( DOUBLE_PRECISION ){ 
	 fprintf( fp, "YRT       %5ld%5d%5d%-20s\n", 
		   YCOUNT-1+IUCNT, NUMOUT, IIMG, F_FORM_D );
     }else{ 
	 fprintf( fp, "YRT       %5ld%5d%5d%-20s\n", 
		   YCOUNT-1+IUCNT, NUMOUT, IIMG, F_FORM_S );
     } 

     CURRENT = 0;
     for( i=1; i<=(int)NUMOUT; i++ ){ 
	 IU      = 1;
	 for( IY=1; IY<(int)YCOUNT; IY++ ){ 
	    if( fabs( UTIME[IU]-YTIME[IY] ) <= IEPS
		&&  IU < (int)UCOUNT ){ 
		IU++;
	    } 

	    while( UTIME[IU] < YTIME[IY]
		&&  fabs(UTIME[IU]-YTIME[IY]) > IEPS 
		&& IU < UCOUNT ) { 
		fprintf( fp, " ");
		PrintReal_C_FORM(fp, Y[i+(IY-2)*(int)NUMOUT]);

		CURRENT++;
		if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
		   fprintf( fp, "\n" );
		} 

		IU++;
	    } 

	    fprintf( fp, " ");
	    PrintReal_C_FORM(fp, Y[i+(IY-1)*(int)NUMOUT]);

	    CURRENT++;
	    if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
		fprintf( fp, "\n" );
	    } 
	 } 

	 for( ; IU<=UCOUNT; IU++ ){ 
	    if( UTIME[IU]-YTIME[YCOUNT-1] >  IEPS ){ 
		fprintf( fp, " ");
		PrintReal_C_FORM(fp, Y[i+(int)NUMOUT*(YCOUNT-2)]);

		CURRENT++;
		if( (CURRENT % ITEMS_PER_LINE) == 0 ){ 
		   fprintf( fp, "\n" );
		} 
	    } 
	 } 

     } 
     if( (CURRENT % ITEMS_PER_LINE) != 0 ) 
	 fprintf( fp, "\n" );
     
     fclose( fp );
#endif
}


void SA_Error(RT_INTEGER NTSK, RT_INTEGER eventType,
		RT_INTEGER errorCode, char cIn)
{
   switch (errorCode)
   { 
      case STOP_BLOCK    :     
	  printf("*** Stop Block encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case MATH_ERROR    :
	  printf("*** Math error encountered in task 0x%lx, eventType:%ld\n",
		  NTSK, eventType);
	  break;
      case UCB_ERROR     :
	  printf("*** Usercode error encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case RANGE_ERROR   :
	  printf("*** Value out of range encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case UNKNOWN_ERROR :
	  printf("*** Unknown error encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case TIME_OVERFLOW : 
	  printf("*** Time overflow encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case VXWORKS_ERROR : 
	  printf("*** VxWorks error encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case TASK_OVERRUN  :
	  printf("*** Task overrun error encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      case OUT_OF_INPUT_DATA  :
	  printf("*** Out of input data error encountered in task 0x%lx, eventType:%ld\n", 
		  NTSK, eventType);
	  break;
      default            :
	  printf("*** Unexpected error in task 0x%lx, errorCode:0x%lx\n", 
		  NTSK, errorCode);
   }
   errorFound = TRUE;
}

static void fatalerr(RT_INTEGER errorCode)
{

    char  *string;

    /* Exceptions */
    switch( errorCode ){ 
	case 301:string="INPUT FILE IS NOT IN Xmath{matrixx, ascii} SAVE FORMAT";
		   break;
	case 303: string="INPUT FILE VERSION IS NOT V7.0 OR LATER";        break;
	case 305: string="INPUT FILE CONTAINS MORE THAN TWO ARRAYS";       break;
	case 307: string="INPUT TIME VECTOR NOT ONE COLUMN";               break;
	case 309: string="INPUT TIME VECTOR TOO LARGE";                    break;
	case 311: string="INPUT U DIMENSION NOT (TIME x NUMBER OF INPUTS)";break;
	case 313: string="INPUT U ARRAY TOO LARGE FOR AVAILABLE STORAGE";  break;
	case 315: string="ERROR OPENING THE INPUT FILE";                   break;
	case 317: string="ERROR OPENING THE OUTPUT FILE";                  break;
	case 319: string="NON-ZERO FIRST TIME POINT IN INPUT TIME VECTOR"; break;
	case 320: string="ERROR IN TIME POINT IN INPUT TIME VECTOR";       break;
	case 321: string="INSUFFICIENT DATA: MISSING TIME/U VECTOR";       break;
	case 401: string="OUTPUT STORAGE EXCEEDS THE AVAILABLE STORAGE";   break;
	case 501: string="INCONSISTENT SIZE OF RT_FLOAT WHEN READING";   break;
	default : string="UNKNOWN ERROR";
    } 

    /* Write the error message */
    printf( "\n FATAL ERROR: %d", errorCode );
    printf( "\n %s\n", string );

    exit(1);
}



void SA_Initialize(void)
{

   outOfInputData = FALSE;
}
