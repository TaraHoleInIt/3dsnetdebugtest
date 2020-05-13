#include <3ds.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

bool Netdebug_Open( int Port );
void Netdebug_Close( void );
size_t Netdebug_send( const char* Data );
size_t Netdebug_printf( const char* Text, ... );

int DisplayError( const char* ErrorText );
void Cleanup( void );

static uint32_t* SOCBuffer = NULL;

static struct sockaddr_in DebugAddr;
static int DebugSocket = -1;

bool Netdebug_Open( int Port ) {
    struct in_addr OurBroadcast;
    struct in_addr OurNetmask;
    struct in_addr OurIP;

    memset( &DebugAddr, 0, sizeof( DebugAddr ) );

    DebugAddr.sin_port = htons( Port );
    DebugAddr.sin_family = AF_INET;

    // Get our broadcast address
    if ( SOCU_GetIPInfo( &OurIP, &OurNetmask, &OurBroadcast ) == 0 ) {
        DebugAddr.sin_addr = OurBroadcast;
    } else {
        // Maybe we're running under Citra and SOCU_GetIPInfo isn't supported?
        // Try another method...

        // Convert our IP to broadcast IP?
        // This assumes a netmask of 255.255.255.0
        DebugAddr.sin_addr.s_addr = gethostid( );
        DebugAddr.sin_addr.s_addr |= ~inet_addr( "255.255.255.0" );
    }

    // protocol must be 0?
    return ( DebugSocket = socket( AF_INET, SOCK_DGRAM, 0 ) ) != -1;
}

void Netdebug_Close( void ) {
    if ( DebugSocket != -1 ) {
        closesocket( DebugSocket );

        memset( &DebugAddr, 0, sizeof( DebugAddr ) );
        DebugSocket = -1;
    }
}

size_t Netdebug_send( const char* Data ) {
    size_t BytesSent = -1;

    if ( Data && DebugSocket > -1 ) {
        BytesSent = sendto(
            DebugSocket,
            Data,
            strlen( Data ) + 1, // +1 for null terminator
            0,
            ( const struct sockaddr* ) &DebugAddr,
            sizeof( DebugAddr )
        );
    }

    return BytesSent;
}

size_t Netdebug_printf( const char* Text, ... ) {
    static char TempBuffer[ 512 ];
    va_list Argp;

    va_start( Argp, Text );
        vsnprintf( TempBuffer, sizeof( TempBuffer ), Text, Argp );
    va_end( Argp );

    return Netdebug_send( TempBuffer );
}

int DisplayError( const char* ErrorText ) {
    errorConf e;

    errorInit( &e, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN );
    errorText( &e, ErrorText );
    errorDisp( &e );

    return 1;
}

void Cleanup( void ) {
    if ( SOCBuffer ) {
        socExit( );
        free( SOCBuffer );
    }

    gfxExit( );
}

int main( void ) {
    uint32_t NextHeartbeat = 0;
    uint32_t Now = 0;

    gfxInitDefault( );
    consoleInit( GFX_TOP, GFX_LEFT );

    atexit( Cleanup );

    // Allocate memory for SoC
    if ( ( SOCBuffer = memalign( SOC_ALIGN, SOC_BUFFERSIZE ) ) == NULL )
        return DisplayError( "Failed to allocate SoC memory" );

    // Initialize SoC
    if ( socInit( SOCBuffer, SOC_BUFFERSIZE ) != 0 )
        return DisplayError( "Failed to initialize SoC" );

    // Initialize Netdebug
    if ( Netdebug_Open( 6000 ) == false )
        return DisplayError( "Failed to open network debug socket" );

    while ( aptMainLoop( ) ) {
        hidScanInput( );

        // Exit with start button
        if ( hidKeysDown( ) & KEY_START )
            break;

        Now = osGetTime( );

        if ( Now >= NextHeartbeat ) {
            NextHeartbeat = Now + 1000;

            Netdebug_printf( "THUMPTHUMP!\n" );
        }
    }

    return 0;
}
