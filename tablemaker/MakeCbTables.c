#include <stdio.h>
#include <stdint.h>

#ifdef PAL
#define VIDEO_LINES 625
#else
#define VIDEO_LINES 525
#endif

#define FT_STA_d 0
#define FT_STB_d 1
#define FT_B_d 2
#define FT_SRA_d 3
#define FT_SRB_d 4
#define FT_LIN_d 5
#define FT_CLOSE_d 6
#define FT_MAX_d 7

int main()
{

	uint8_t CbLookup[VIDEO_LINES+1]; //Because we're odd, we have to extend this by one byte.
	memset( CbLookup, 0, sizeof(CbLookup) );
	int x;

#ifdef PAL
	#define OVERSCAN_TOP 30
	#define OVERSCAN_BOT 10
	
	//Setup the callback table.
	for( x = 0; x < 2; x++ )
		CbLookup[x] = FT_STB_d;
	CbLookup[x++] = FT_SRB_d;
	for( ; x < 5; x++ )
		CbLookup[x] = FT_STA_d;

	for( ; x < 5+OVERSCAN_TOP; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 310-OVERSCAN_BOT; x++ ) // 250
		CbLookup[x] = FT_LIN_d;
	for( ; x < 310; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 312; x++ )
		CbLookup[x] = FT_STA_d;

	//begin odd field
	CbLookup[x++] = FT_SRA_d;
	for( ; x < 315; x++ )
		CbLookup[x] = FT_STB_d;
	for( ; x < 317; x++ )
		CbLookup[x] = FT_STA_d;

	for( ; x < 317+OVERSCAN_TOP; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 622-OVERSCAN_BOT; x++ )//562
		CbLookup[x] = FT_LIN_d;
	for( ; x < 622; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 624; x++ )
		CbLookup[x] = FT_STA_d;
	CbLookup[x++] = FT_CLOSE_d;
	CbLookup[x++] = FT_CLOSE_d;

#else
	//Setup the callback table.
	for( x = 0; x < 3; x++ )
		CbLookup[x] = FT_STA_d;
	for( ; x < 6; x++ )
		CbLookup[x] = FT_STB_d;
	for( ; x < 9; x++ )
		CbLookup[x] = FT_STA_d;
	for( ; x < 24+6; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 256-15; x++ )
		CbLookup[x] = FT_LIN_d;
	for( ; x < 263; x++ )
		CbLookup[x] = FT_B_d;

	//263rd frame, begin odd sync.
	for( ; x < 266; x++ )
		CbLookup[x] = FT_STA_d;

	CbLookup[x++] = FT_SRA_d;

	for( ; x < 269; x++ )
		CbLookup[x] = FT_STB_d;

	CbLookup[x++] = FT_SRB_d;

	for( ; x < 272; x++ )
		CbLookup[x] = FT_STA_d;
	for( ; x < 288+6; x++ )
		CbLookup[x] = FT_B_d;
	for( ; x < 519-15; x++ )
		CbLookup[x] = FT_LIN_d;
	for( ; x < VIDEO_LINES-1; x++ )
		CbLookup[x] = FT_B_d;
	CbLookup[x] = FT_CLOSE_d;
#endif

	FILE * f = fopen( "CbTable.h", "w" );
	fprintf( f, "#ifndef _CBTABLE_H\n\
#define _CBTABLE_H\n\
\n\
#include <c_types.h>\n\
\n\
#define FT_STA_d 0\n\
#define FT_STB_d 1\n\
#define FT_B_d 2\n\
#define FT_SRA_d 3\n\
#define FT_SRB_d 4\n\
#define FT_LIN_d 5\n\
#define FT_CLOSE 6\n\
#define FT_MAX_d 7\n\
\n\
#define VIDEO_LINES %d\n\
\n\
uint8_t CbLookup[%d];\n\
\n\
#endif\n\n", VIDEO_LINES, (VIDEO_LINES+1)/2 );
	fclose( f );

	f = fopen( "CbTable.c", "w" );
	fprintf( f, "#include \"CbTable.h\"\n\n" );
	fprintf( f, "uint8_t CbLookup[%d] = {", (VIDEO_LINES+1)/2 );
	for( x = 0; x < (VIDEO_LINES+1)/2; x++ )
	{
		if( (x & 0x0f) == 0 )
		{
			fprintf( f, "\n\t" );
		}
		fprintf( f, "0x%02x, ", CbLookup[x*2+0] | ( CbLookup[x*2+1]<<4 ) );
	}
	fprintf( f, "};\n" );

	return 0;
}

