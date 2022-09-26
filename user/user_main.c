//Copyright 2015 <>< Charles Lohr, see LICENSE file.

#include "uart.h"
#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "video_broadcast.h"
#include "commonservices.h"
#include <mdns.h>
#include "3d.h"

#include "pong_geometry.h"

#define PORT 7777

#define procTaskPrio        0
#define procTaskQueueLen    1

static os_timer_t some_timer;
static struct espconn *pUdpServer;

#define POTI1_PIN 4
#define POTI2_PIN 5
int16_t paddlePos[2] = {0,0};

int scorePaddle[2] = {0,0};

#define ADCVAL_TO_Y_POS(ADC) ((ADC)/6.4f)
int adcIdx = 0;

float ballTotalVel = 2.0f;

float ballPosX = 0;
float ballPosY = 0;
float ballVelX = 1;
float ballVelY = 2;

//Tasks that happen all the time.

os_event_t    procTaskQueue[procTaskQueueLen];

void ICACHE_FLASH_ATTR SetupMatrix( )
{
	int16_t lmatrix[16];
	tdIdentity( ProjectionMatrix );
	tdIdentity( ModelviewMatrix );

	Perspective( 600, 250, 50, 8192, ProjectionMatrix );
}

void user_pre_init(void)
{
	//You must load the partition table so the NONOS SDK can find stuff.
	LoadDefaultPartitionMap();
}
 

//0 is the normal flow
//11 is the multi-panel scene.
#define INITIAL_SHOW_STATE 0

extern int gframe;
char lastct[256];
uint8_t showstate = INITIAL_SHOW_STATE;
uint8_t showallowadvance = 1;
int framessostate = 0;
int showtemp = 0;

void ICACHE_FLASH_ATTR Draw3DSegmentTranslate( int16_t * c1, int16_t * c2, int dx, int dy)
{
	int16_t sx0, sy0, sx1, sy1;
	LocalToScreenspace( c1, &sx0, &sy0 );
	LocalToScreenspace( c2, &sx1, &sy1 );
	CNFGTackSegment( sx0+dx, sy0+dy, sx1+dx, sy1+dy );
}

void ICACHE_FLASH_ATTR Draw3DModelTranslate(int16_t *verts, uint16_t *indices, int lenIndices, int dx,int dy){
	int i;
	int nrv = lenIndices/sizeof(uint16_t);
	for( i = 0; i < nrv; i+=2 )
	{
		int16_t * c1 = &verts[indices[i]];
		int16_t * c2 = &verts[indices[i+1]];
		Draw3DSegmentTranslate( c1, c2, dx,dy );
	}
}

void ICACHE_FLASH_ATTR LeftPaddle(int posy){
	int16_t ftmp[16];
	memcpy(ftmp, ModelviewMatrix, sizeof(ModelviewMatrix));
	MakeTranslate(PADDLE_CENTER_X(138),PADDLE_CENTER_Y(0),512, ModelviewMatrix);
	ModelviewMatrix[0] = PADDLE_X;
	ModelviewMatrix[5] = PADDLE_Y;
	ModelviewMatrix[10] = PADDLE_Z;
	Draw3DModelTranslate(cubeVerts, cubeIndices, sizeof(cubeIndices) ,-64,posy+1);
	memcpy(ModelviewMatrix, ftmp, sizeof(ModelviewMatrix));
}


void ICACHE_FLASH_ATTR RightPaddle(int posy){
	int16_t ftmp[16];
	memcpy(ftmp, ModelviewMatrix, sizeof(ModelviewMatrix));
	MakeTranslate(PADDLE_CENTER_X(-138),PADDLE_CENTER_Y(0),512, ModelviewMatrix);
	ModelviewMatrix[0] = PADDLE_X;
	ModelviewMatrix[5] = PADDLE_Y;
	ModelviewMatrix[10] = PADDLE_Z;
	Draw3DModelTranslate(cubeVerts, cubeIndices, sizeof(cubeIndices) ,64,posy+1);
	memcpy(ModelviewMatrix, ftmp, sizeof(ModelviewMatrix));
}

void ICACHE_FLASH_ATTR Outline(){
	CNFGColor( 5 );
	int16_t ftmp[16];
	memcpy(ftmp, ModelviewMatrix, sizeof(ModelviewMatrix));
	MakeTranslate(-460,-425,512, ModelviewMatrix);
	ModelviewMatrix[0] = 920;
	ModelviewMatrix[5] = 850;
	ModelviewMatrix[10] = 200;
	Draw3DModelTranslate(cubeVerts, cubeIndices, sizeof(cubeIndices) ,0,0);
	memcpy(ModelviewMatrix, ftmp, sizeof(ModelviewMatrix));
	CNFGColor( 17 );
}

void ICACHE_FLASH_ATTR Ball(){
	int16_t ftmp[16];
	memcpy(ftmp, ModelviewMatrix, sizeof(ModelviewMatrix));
	tdIdentity( ModelviewMatrix );
	tdScale(ModelviewMatrix, BALL_X, BALL_Y, BALL_Z);
	tdRotateEA(ModelviewMatrix, framessostate*3,framessostate*5,2*framessostate);
	ModelviewMatrix[11] = 768;
	ModelviewMatrix[3] = BALL_CENTER_X(0);
	ModelviewMatrix[7] = BALL_CENTER_Y(0);
	Draw3DModelTranslate(dodecahedronVerts, dodecahedronIndices, sizeof(dodecahedronIndices) ,((int)ballPosX)-5,((int)ballPosY)-5);
	memcpy(ModelviewMatrix, ftmp, sizeof(ModelviewMatrix));
}

int calcPaddlePos(uint16_t adcVal){
	if(adcVal<50)
		return -70;
	if(adcVal>450)
		return 70;

	return ((adcVal-50)/2.9f)-70;
}

void calcNewBallPos(){
	ballPosX += ballVelX;
	ballPosY += ballVelY;
	if(ballPosY < BALL_Y_POS_MIN || ballPosY > BALL_Y_POS_MAX){
		ballVelY = -ballVelY;
	}

	if(ballPosX < BALL_X_POS_PADDLE_MIN){
		int ballYDelta = (paddlePos[0]-ballPosY);
		if(ballYDelta <= 0 && ballYDelta >= -PADDLE_RADIUS){
			ballVelX = -ballVelX;
			ballVelY = (tdSIN(3*ballYDelta)/256.0f) * ballTotalVel;
			ballPosX = BALL_X_POS_PADDLE_MIN;
		} else if(ballYDelta > 0 && ballYDelta < PADDLE_RADIUS){
			ballVelX = -ballVelX;
			ballVelY = -(tdSIN(3*ballYDelta)/256.0f) * ballTotalVel;
			ballPosX = BALL_X_POS_PADDLE_MIN;
		} 
	} 
	if (ballPosX < BALL_X_POS_MIN){
			ballPosX = 0;
			ballPosY = 0;
			scorePaddle[1]+=1;
	} 

	if (ballPosX > BALL_X_POS_PADDLE_MAX){
		int ballYDelta = (paddlePos[1]-ballPosY);
		if(ballYDelta <= 0 && ballYDelta >= -PADDLE_RADIUS){
			ballVelX = -ballVelX;
			ballVelY = (tdSIN(3*ballYDelta)/256.0f) * ballTotalVel;
			ballPosX = BALL_X_POS_PADDLE_MAX;
		} else if(ballYDelta > 0 && ballYDelta < PADDLE_RADIUS){
			ballVelX = -ballVelX;
			ballVelY = -(tdSIN(3*ballYDelta)/256.0f) * ballTotalVel;
			ballPosX = BALL_X_POS_PADDLE_MAX;
		}
	} 
	if (ballPosX > BALL_X_POS_MAX){
			ballPosX = 0;
			ballPosY = 0;
			scorePaddle[0]+=1;
	} 
}

int16_t Height( int x, int y, int l )
{
	return tdCOS( (x*x + y*y) + l );
}

void ICACHE_FLASH_ATTR DrawFrame(  )
{
	char * ctx = &lastct[0];
	int x = 0;
	int y = 0;
	int i;
	int sqsiz = gframe&0x7f;
	int newstate = showstate;
	CNFGPenX = 14;
	CNFGPenY = 20;
	ets_memset( frontframe, 0x00, ((FBW/4)*FBH) );
	int16_t rt[16];
	tdIdentity( ModelviewMatrix );
	tdIdentity( ProjectionMatrix );
	CNFGColor( 17 );

	adcIdx = (!adcIdx) & 1;
	if(adcIdx){
		gpio_output_set(0, 0, 0, (1 << POTI2_PIN));
		gpio_output_set(0, (1 << POTI1_PIN), (1 << POTI1_PIN), 0);
	} else {
		gpio_output_set(0, 0, 0, (1 << POTI1_PIN));
		gpio_output_set(0, (1 << POTI2_PIN), (1 << POTI2_PIN), 0);
	}
	CNFGPenY += 24;
	CNFGPenX += 80;

	char buf[256];
	ets_sprintf(buf, "%d:%d", scorePaddle[0], scorePaddle[1]);
	CNFGDrawText(buf, 4);

	SetupMatrix();
	paddlePos[adcIdx] = calcPaddlePos(system_adc_read());
	Outline();
	LeftPaddle(paddlePos[0]);
	RightPaddle(paddlePos[1]);
	calcNewBallPos();
	Ball();

	if( showstate != newstate && showallowadvance )
	{
		showstate = newstate;
		framessostate = 0;
		showtemp = 0;
	}
	else
	{
		framessostate++;
	}

}

static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	static uint8_t lastframe = 0;
	uint8_t tbuffer = !(gframe&1);

	CSTick( 0 );

	if( lastframe != tbuffer )
	{
		//printf( "FT: %d - ", last_internal_frametime );
		uint32_t tft = system_get_time();
		frontframe = (uint8_t*)&framebuffer[((FBW2/4)*FBH)*tbuffer];
		DrawFrame( frontframe );
		//ets_memset( frontframe, 0xaa, ((FBW/4)*FBH) );
		lastframe = tbuffer;
		//printf( "%d\n", system_get_time() - tft );
	}

	system_os_post(procTaskPrio, 0, 0 );
}

//Timer event.
static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	CSTick( 1 );
}


//Called when new packet comes in.
static void ICACHE_FLASH_ATTR
udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;

	uart0_sendStr("X");
/*
	ws2812_push( pusrdata+3, len-3 );

	len -= 3;
	if( len > sizeof(last_leds) + 3 )
	{
		len = sizeof(last_leds) + 3;
	}
	ets_memcpy( last_leds, pusrdata+3, len );
	last_led_count = len / 3;*/
}

void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("\r\nesp8266 ws2812 driver\r\n");
//	int opm = wifi_get_opmode();
//	if( opm == 1 ) need_to_switch_opmode = 120;
//	wifi_set_opmode_current(2);
//Uncomment this to force a system restore.
//	system_restore();

#ifdef FORCE_SSID
#define SSID ""
#define PSWD ""
#endif

	//Override wifi.
#if FORCE_SSID
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		os_strcpy((char*)&stationConf.ssid, SSID );
		os_strcpy((char*)&stationConf.password, PSWD );
		stationConf.bssid_set = 0;
		wifi_station_set_config(&stationConf);
		wifi_set_opmode(1);
	}
#endif

	CSSettingsLoad( 0 );
	CSPreInit();

	//Override wifi.
#if FORCE_SSID
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		os_strcpy((char*)&stationConf.ssid, SSID );
		os_strcpy((char*)&stationConf.password, PSWD );
		stationConf.bssid_set = 0;
		wifi_station_set_config(&stationConf);
		wifi_set_opmode(1);
	}
#else
		wifi_set_opmode(2);
#endif


    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = 7777;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	CSInit(1);

	SetServiceName( "ws2812" );
	AddMDNSName( "cn8266" );
	AddMDNSName( "ws2812" );
	AddMDNSService( "_http._tcp", "An ESP8266 Webserver", 80 );
	AddMDNSService( "_ws2812._udp", "WS2812 Driver", 7777 );
	AddMDNSService( "_cn8266._udp", "ESP8266 Backend", 7878 );

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 100, 1);


	testi2s_init();

	system_update_cpu_freq( SYS_CPU_160MHZ );

	system_os_post(procTaskPrio, 0, 0 );
}


//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical()
{
}

void ExitCritical()
{
}


