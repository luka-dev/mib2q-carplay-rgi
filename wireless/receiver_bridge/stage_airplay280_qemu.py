#!/usr/bin/env python3
"""Stage the local 280 source with the confirmed MU1316 ScreenCreate ABI fix."""
from pathlib import Path
import hashlib
import shutil
import sys

AUDI = Path(__file__).resolve().parents[4]
SOURCE = AUDI / 'Patches/dio_asm_to_c/_legacy_attempt/libairplay-mhi2q'
DEFAULT_STAGE = Path(__file__).resolve().parent / 'out/airplay280_qemu/source'


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f'expected one ABI anchor in {path}: {old!r}')
    path.write_text(text.replace(old, new))


def replace_span_once(path: Path, start: str, end: str, replacement: str) -> None:
    text = path.read_text()
    if text.count(start) != 1:
        raise SystemExit(f'expected one start anchor in {path}: {start!r}')
    before, rest = text.split(start, 1)
    if end not in rest:
        raise SystemExit(f'missing end anchor in {path}: {end!r}')
    _, after = rest.split(end, 1)
    path.write_text(before + replacement + after)


def main() -> None:
    stage = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else DEFAULT_STAGE
    out_root = (Path(__file__).resolve().parent / 'out').resolve()
    if not stage.is_relative_to(out_root) or stage.name != 'source':
        raise SystemExit('staging output must be an out/*/source directory')
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    for name in ('include', 'src', 'vendor', 'libs', 'scripts'):
        shutil.copytree(SOURCE / name, stage / name)
    qsa_upstream = SOURCE / 'src/audio.c'
    if hashlib.sha256(qsa_upstream.read_bytes()).hexdigest() != '5054145a1e3d328369632754a0fee3f3e7a64f3acd2cdcf5aab397838c2db078':
        raise SystemExit('QSA audio backend upstream changed; recheck local stop fix')
    shutil.copyfile(Path(__file__).with_name('audio_qsa.c'), stage / 'src/audio.c')
    shutil.copyfile(Path(__file__).with_name('airplay_random_qnx.c'),
                    stage / 'src/qnx_random.c')
    # The local QNX GCC 8 stdint.h pulls stddef.h in a restricted mode; size_t
    # remains unavailable if stddef.h comes second in this shim.
    replace_once(stage / 'vendor/shims/glib.h',
                 '#include <stdint.h>\n#include <stddef.h>',
                 '#include <stddef.h>\n#include <stdint.h>')
    keychain = stage / 'vendor/carplay-sdk/AccessorySDK/Support/KeychainLiteFile.c'
    replace_once(keychain,
                 '#include <stdio.h>\n#include <sys/stat.h>',
                 '#include <fcntl.h>\n#include <stdio.h>\n#include <sys/stat.h>\n#include <unistd.h>')
    replace_once(keychain,
                 '\tfile = fopen( path, "wb" );\n'
                 '\terr = map_global_value_errno( file, file );\n'
                 '\trequire_noerr( err, exit );',
                 '''\t// The keychain contains long-term pairing secrets. The service umask
\t// must not decide whether another process can read them.
\t{
\t\tint fd = open( path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
\t\terr = map_global_value_errno( fd >= 0, fd );
\t\trequire_noerr( err, exit );
\t\tif( fchmod( fd, S_IRUSR | S_IWUSR ) != 0 )
\t\t{
\t\t\terr = map_global_value_errno( 0, fd );
\t\t\tclose( fd );
\t\t\tgoto exit;
\t\t}
\t\tfile = fdopen( fd, "wb" );
\t\tif( !file )
\t\t{
\t\t\terr = map_global_value_errno( 0, fd );
\t\t\tclose( fd );
\t\t\tgoto exit;
\t\t}
\t}''')

    support = stage / 'vendor/carplay-sdk/AccessorySDK/Support'
    # The guarded owner signals dio after the receiver has quiesced. On QNX,
    # that signal can interrupt DispatchLite's select; rebuild its fd sets
    # and timer timeout on the next iteration instead of asserting/sleeping.
    replace_once(support / 'DispatchLite.c',
                 '''		n = select( maxFD + 1, &gDispatchSelect_ReadSet, writeSetPtr, NULL, timeoutPtr );
		err = select_errno( n );
		if( !err )''',
                 '''		n = select( maxFD + 1, &gDispatchSelect_ReadSet, writeSetPtr, NULL, timeoutPtr );
		err = select_errno( n );
		if( err == EINTR ) continue;
		if( !err )''')
    replace_once(support / 'CFLite.c',
                 'void\tCFLRelease( CFLObjectRef inObject )\n{\n\tCFLObject *\t\tobj;\n\t\n\trequire( CFLValidObject( inObject ), exit );',
                 '''void\tCFLRelease( CFLObjectRef inObject )
{
\tCFLObject *\t\tobj;

#ifdef MHI2Q_QEMU_CFRELEASE_CALLER
\tif( !CFLValidObject( inObject ) )
\t{
\t\tfprintf( stderr, "[cfrelease-qemu] object=%p caller=%p release=%p\\n",
\t\t\tinObject, __builtin_return_address( 0 ), (void *) CFLRelease );
\t\tgoto exit;
\t}
#elif defined( MHI2Q_QEMU_CFRELEASE_ASSERT )
\t{
\t\tint valid = CFLValidObject( inObject );
\t\tif( !valid )
\t\t{
\t\t\tunsigned index = mhi2q_cf_release_index;
\t\t\tunsigned n;
\t\t\tchar line[160];
\t\t\tint used;
\t\t\tfor( n = 0; n < 128 && n < index; ++n )
\t\t\t{
\t\t\t\tunsigned slot = ( index - 1 - n ) & 127;
\t\t\t\tif( mhi2q_cf_release_ring[ slot ].object != inObject ) continue;
\t\t\t\tused = snprintf( line, sizeof( line ),
\t\t\t\t\t"[cfassertcaller-qemu] object=%p caller=%p age=%u\\n",
\t\t\t\t\tinObject, mhi2q_cf_release_ring[ slot ].caller, n );
\t\t\t\tif( used > 0 && used < (int) sizeof( line ) ) write( 2, line, (size_t) used );
\t\t\t\tif( n > 0 ) break;
\t\t\t}
\t\t}
\t\trequire( valid, exit );
\t}
#elif defined( MHI2Q_QEMU_ASSERT_STACK )
\t{
\t\tint valid = CFLValidObject( inObject );
\t\tif( !valid )
\t\t{
\t\t\tchar line[ 160 ];
\t\t\tconst uint8_t *raw = (const uint8_t *) inObject;
\t\t\tint used = snprintf( line, sizeof( line ),
\t\t\t\t"[cfreturn-qemu] object=%p caller0=%p top=%p topobj=%p bytes=%02x%02x%02x%02x\\n",
\t\t\t\tinObject, __builtin_return_address( 0 ), mhi2q_cf_release_top_caller,
\t\t\t\tmhi2q_cf_release_top_object, raw ? raw[0] : 0, raw ? raw[1] : 0,
\t\t\t\traw ? raw[2] : 0, raw ? raw[3] : 0 );
\t\t\tif( used > 0 && used < (int) sizeof( line ) ) write( 2, line, (size_t) used );
\t\t}
\t\trequire( valid, exit );
\t}
#else
\trequire( CFLValidObject( inObject ), exit );
#endif''')
    replace_once(support / 'CFLite.c',
                 '''#include "DebugServices.h"
''',
                 '''#include "DebugServices.h"
#ifdef MHI2Q_QEMU_CFRELEASE_ASSERT
struct mhi2q_cf_release_entry { const void *object; void *caller; };
extern struct mhi2q_cf_release_entry mhi2q_cf_release_ring[ 128 ];
extern volatile unsigned mhi2q_cf_release_index;
#endif
#ifdef MHI2Q_QEMU_ASSERT_STACK
extern void *volatile mhi2q_cf_release_top_caller;
extern const void *volatile mhi2q_cf_release_top_object;
#endif
''')
    replace_once(support / 'CFCompat.c',
                 '''void\tCFRelease( CFTypeRef inObject )
{
\tCFLRelease( inObject );
}''',
                 '''#ifdef MHI2Q_QEMU_CFRELEASE_ASSERT
struct mhi2q_cf_release_entry { const void *object; void *caller; };
struct mhi2q_cf_release_entry mhi2q_cf_release_ring[ 128 ];
volatile unsigned mhi2q_cf_release_index;
#endif
#ifdef MHI2Q_QEMU_ASSERT_STACK
void *volatile mhi2q_cf_release_top_caller;
const void *volatile mhi2q_cf_release_top_object;
#endif
void\tCFRelease( CFTypeRef inObject )
{
#ifdef MHI2Q_QEMU_CFRELEASE_ASSERT
\tunsigned index = __sync_fetch_and_add( &mhi2q_cf_release_index, 1 );
\tmhi2q_cf_release_ring[ index & 127 ].object = inObject;
\tmhi2q_cf_release_ring[ index & 127 ].caller = __builtin_return_address( 0 );
#endif
#ifdef MHI2Q_QEMU_ASSERT_STACK
\tmhi2q_cf_release_top_object = inObject;
\tmhi2q_cf_release_top_caller = __builtin_return_address( 0 );
#endif
\tCFLRelease( inObject );
}''')
    # A lost peer UDP timing port makes select() report the socket as readable
    # forever. Without backoff, this thread floods logs and can starve the
    # HTTP queue handling the control-socket disconnect.
    session = stage / 'vendor/carplay-sdk/Sources/AirPlayReceiverSession.c'
    replace_once(session,
                 'if( FD_ISSET( sock,    &readSet ) ) _TimingReceiveResponse( session, sock );',
                 '''if( FD_ISSET( sock, &readSet ) )
		{
			err = _TimingReceiveResponse( session, sock );
			if( err == ECONNREFUSED ) usleep( 100000 );
		}''')
    replace_once(session,
                 '''AirTunesTime		recvTime;
\t
	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, &ticks, NULL, NULL );
	if( err == EWOULDBLOCK ) goto exit;
	require_noerr( err, exit );''',
                 '''AirTunesTime		recvTime;
\t
	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, &ticks, NULL, NULL );
	if( ( err == EWOULDBLOCK ) || ( err == ECONNREFUSED ) ) goto exit;
	require_noerr( err, exit );''')
    receiver = stage / 'vendor/carplay-sdk/Sources/AirPlayReceiverServer.c'
    replace_once(receiver,
                 '''dispatch_queue_t\tAirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer )
{
\treturn( inServer->queue );
}
''',
                 '''dispatch_queue_t\tAirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer )
{
\treturn( inServer->queue );
}

/* The bridge calls this on its own thread. The test and stop are serialized
 * with the phone's SETUP handler on httpQueue; closing admission before
 * scheduling HTTPServerStop also covers SETUP events already queued behind us. */
static int gMhi2qMediaAdmissionClosed;
typedef struct { AirPlayReceiverServerRef server; int result; } Mhi2qIdleStop;
static void _Mhi2qStopIfIdleOnHTTPQueue( void *inContext )
{
\tMhi2qIdleStop *request = (Mhi2qIdleStop *) inContext;
\tAirPlayReceiverServerRef server = request->server;
\tif( server->playing ) { request->result = 1; return; }
\tgMhi2qMediaAdmissionClosed = 1;
\tif( server->started ) { server->started = false; _StopServers( server ); }
\trequest->result = 0;
}
EXPORT_GLOBAL int MHI2QAirPlayReceiverServerStopIfIdle( AirPlayReceiverServerRef inServer )
{
\tMhi2qIdleStop request;
\tif( !inServer || !inServer->httpQueue ) return -1;
\trequest.server = inServer;
\trequest.result = -1;
\tdispatch_sync_f( inServer->httpQueue, &request, _Mhi2qStopIfIdleOnHTTPQueue );
\treturn request.result;
}
static void _Mhi2qResumeOnHTTPQueue( void *inContext )
{
\t(void) inContext;
\tgMhi2qMediaAdmissionClosed = 0;
}
EXPORT_GLOBAL int MHI2QAirPlayReceiverServerResume( AirPlayReceiverServerRef inServer )
{
\tif( !inServer || !inServer->httpQueue ) return -1;
\tdispatch_sync_f( inServer->httpQueue, NULL, _Mhi2qResumeOnHTTPQueue );
\treturn 0;
}
''')
    replace_once(receiver,
                 '''static HTTPStatus _requestProcessSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
\tHTTPStatus\t\t\t\tstatus;
\tconst char *\t\t\tptr;
\tsize_t\t\t\t\t\tlen;
''',
                 '''static HTTPStatus _requestProcessSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
\tHTTPStatus\t\t\t\tstatus;
\tconst char *\t\t\tptr;
\tsize_t\t\t\t\t\tlen;

\tif( gMhi2qMediaAdmissionClosed ) return kHTTPStatus_ServiceUnavailable;
''')
    replace_once(support / 'DebugServices.c',
                 '''\tva_start( args, inMessageFormat );
\tif( inErrorCode != 0 )
\t{
\t\tprintf( "### [ASSERT] %s:%ld \\"%s\\", %d %s\\n", inFilename, inLineNumber, inFunction, (int) inErrorCode, ''',
                 '''\tva_start( args, inMessageFormat );
#ifdef MHI2Q_QEMU_ASSERT_STACK
\tif( inFilename && strstr( inFilename, "CFLite.c" ) &&
\t\t( inLineNumber == 465 || inLineNumber == 473 ) )
\t{
\t\tfprintf( stderr, "[cfassert-qemu] call0=%p call1=%p\\n",
\t\t\t__builtin_return_address( 0 ), __builtin_return_address( 1 ) );
\t}
#endif
\tif( inErrorCode != 0 )
\t{
\t\tprintf( "### [ASSERT] %s:%ld \\"%s\\", %d %s\\n", inFilename, inLineNumber, inFunction, (int) inErrorCode, ''')
    replace_once(support / 'DebugServices.c',
                 '''\tva_start( args, inMessageFormat );
\tif( inErrorCode != 0 )
\t{
\t\tdbs_ulog( kLogLevelAssert, "### [ASSERT] %s:%ld \\"%###s\\", %#m %V\\n%s", ''',
                 '''\tva_start( args, inMessageFormat );
#ifdef MHI2Q_QEMU_ASSERT_STACK
\tif( inFilename && strstr( inFilename, "CFLite.c" ) &&
\t\t( inLineNumber == 465 || inLineNumber == 473 ) )
\t{
\t\tfprintf( stderr, "[cfassert-qemu] call0=%p call1=%p\\n",
\t\t\t__builtin_return_address( 0 ), __builtin_return_address( 1 ) );
\t}
#endif
\tif( inErrorCode != 0 )
\t{
\t\tdbs_ulog( kLogLevelAssert, "### [ASSERT] %s:%ld \\"%###s\\", %#m %V\\n%s", ''')
    replace_once(stage / 'src/hid_2q.c',
                 '''/* ---- ScreenRegisterDelegates: copy 5 dwords to screen+60 ------------------ */
void *ScreenRegisterDelegates(void *screen, const void *delegates)
{
    if (screen) {
        uint32_t *dst = (uint32_t *)((char *)screen + 60);
        const uint32_t *src = (const uint32_t *)delegates;
        dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; dst[4] = src[4];
    }
    return screen;
}''',
                 '''/* The 280 SDK ScreenPrivate is 48 bytes on ARM (CFLObject 8 + 10
 * 32-bit fields). The stock MHI2Q implementation copied delegates to +60,
 * which would overwrite the adjacent allocation in this replacement SDK.
 * The 280 receiver uses its own ScreenStream callbacks; dio's legacy
 * ScreenRegisterDelegates payload has no consumer in this SDK. */
void *ScreenRegisterDelegates(void *screen, const void *delegates)
{
    (void)delegates;
    return screen;
}''')
    for header in (support / 'ScreenUtils.h', support / 'CoreUtils/ScreenUtils.h'):
        replace_once(header,
                     'OSStatus\tScreenCreate( ScreenRef *outScreen, CFDictionaryRef inProperties );',
                     'OSStatus\tScreenCreate( ScreenRef *outScreen );\n'
                     'OSStatus\tScreenCreateWithProperties( ScreenRef *outScreen, CFDictionaryRef inProperties );')
    impl = support / 'ScreenUtils.c'
    replace_once(impl,
                 '};\n\nstatic void\t_ScreenGetTypeID( void *inContext );',
                 '};\ncheck_compile_time( sizeof( struct ScreenPrivate ) == 48 );\n\n'
                 'static void\t_ScreenGetTypeID( void *inContext );')
    replace_once(impl,
                 'OSStatus\tScreenCreate( ScreenRef *outScreen, CFDictionaryRef inProperties )',
                 'OSStatus\tScreenCreate( ScreenRef *outScreen )\n'
                 '{\n\treturn ScreenCreateWithProperties( outScreen, NULL );\n}\n\n'
                 'OSStatus\tScreenCreateWithProperties( ScreenRef *outScreen, CFDictionaryRef inProperties )')
    replace_once(impl, 'ScreenCreate( &screen, NULL )',
                 'ScreenCreateWithProperties( &screen, NULL )')
    replace_once(stage / 'vendor/carplay-sdk/Sources/AirPlayReceiverServer.c',
                 'ScreenCreate( &screen, dict )',
                 'ScreenCreateWithProperties( &screen, dict )')
    server = stage / 'vendor/carplay-sdk/Sources/AirPlayReceiverServer.c'
    replace_once(server,
                 'features = (AirPlayFeatures) UINT64_C( 0x0000000144440B80 );',
                 'features = (AirPlayFeatures) UINT64_C( 0x0000006144440B80 );')
    replace_span_once(server,
                      '\t// 210-CLAMP: no HomeKit identity in stock 210 /info.',
                      '\n\t// RightHandDrive',
                      '\terr = AirPlayCopyHomeKitPairingIdentity( &cptr, NULL );\n'
                      '\tif( !err )\n\t{\n'
                      '\t\tCFDictionarySetCString( info, CFSTR( kAirPlayKey_PublicHKID ), cptr, kSizeCString );\n'
                      '\t\tfree( cptr );\n\t}\n\n\t// RightHandDrive')
    replace_span_once(server,
                      '/* 210-CLAMP: no HomeKit pairing identity in stock-210 Bonjour TXT.',
                      '\n\t// Source Version',
                      '''{
    char *cptr;
    err = AirPlayCopyHomeKitPairingIdentity( &cptr, NULL );
    if( !err )
    {
        err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity, cptr );
        free( cptr );
        require_noerr( err, exit );
    }
    else
    {
        aprs_ulog( kLogLevelNotice, "### Add pairing HomeKit peer identifier to advertiser info failed: %#m\\n", err );
    }
}

\t// Source Version''')
    replace_once(server,
                 '//else if( strnicmp_suffix( pathPtr, pathLen, "/pair-setup" )',
                 'else if( strnicmp_suffix( pathPtr, pathLen, "/pair-setup" )')
    replace_once(server,
                 '//else if( strnicmp_suffix( pathPtr, pathLen, "/pair-verify" )',
                 'else if( strnicmp_suffix( pathPtr, pathLen, "/pair-verify" )')
    replace_once(server,
                 'HTTPMessageSetBody( response, kMIMEType_AppleBinaryPlist, outputPtr, outputLen )',
                 'HTTPMessageSetBody( response, kPairingMIMEType, outputPtr, outputLen )')
    replace_once(server, '#include CF_HEADER', '#include <unistd.h>\n#include CF_HEADER')
    # QEMU-only: identify a malformed field if /info plist serialization fails.
    # The diagnostic is compiled out of the on-unit receiver candidate.
    replace_once(server,
                 'static HTTPStatus _requestSendPlistResponse( HTTPConnectionRef inCnx, HTTPMessageRef inRequest, CFPropertyListRef inPlist, OSStatus *outErr )',
                 '''#ifdef MHI2Q_QEMU_INFO_CFDIAG
static void _MHI2QInfoCFDiag( CFTypeRef object, const char *path, unsigned depth )
{
    CFTypeID type;
    if( !object || depth > 5 ) return;
    type = CFGetTypeID( object );
    {
        char line[ 320 ];
        int n = snprintf( line, sizeof( line ), "[info-cfdiag] %s ptr=%p type=%lu\\n",
            path, object, (unsigned long) type );
        if( n > 0 && n < (int) sizeof( line ) ) (void) write( 2, line, (size_t) n );
    }
    if( !type ) return;
    if( type == CFDictionaryGetTypeID() )
    {
        const void *keys[ 128 ], *values[ 128 ];
        CFIndex i, count = CFDictionaryGetCount( (CFDictionaryRef) object );
        if( count > 128 ) return;
        CFDictionaryGetKeysAndValues( (CFDictionaryRef) object, keys, values );
        for( i = 0; i < count; ++i )
        {
            char key[ 96 ] = "?", child[ 256 ];
            if( CFGetTypeID( keys[ i ] ) == CFStringGetTypeID() )
                CFStringGetCString( (CFStringRef) keys[ i ], key, sizeof( key ), kCFStringEncodingUTF8 );
            snprintf( child, sizeof( child ), "%s.%s", path, key );
            _MHI2QInfoCFDiag( values[ i ], child, depth + 1 );
        }
    }
    else if( type == CFArrayGetTypeID() )
    {
        CFIndex i, count = CFArrayGetCount( (CFArrayRef) object );
        if( count > 128 ) return;
        for( i = 0; i < count; ++i )
        {
            char child[ 256 ];
            snprintf( child, sizeof( child ), "%s[%ld]", path, (long) i );
            _MHI2QInfoCFDiag( CFArrayGetValueAtIndex( (CFArrayRef) object, i ), child, depth + 1 );
        }
    }
}
#endif

static HTTPStatus _requestSendPlistResponse( HTTPConnectionRef inCnx, HTTPMessageRef inRequest, CFPropertyListRef inPlist, OSStatus *outErr )''')
    replace_once(server,
                 '\t\tdata = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );',
                 '''#ifdef MHI2Q_QEMU_INFO_CFDIAG
        if( inRequest->header.urlLen >= 5 &&
            memcmp( inRequest->header.urlPtr, "/info", 5 ) == 0 )
        {
            static volatile unsigned info_calls;
            if( __sync_add_and_fetch( &info_calls, 1 ) >= 2 )
                _MHI2QInfoCFDiag( inPlist, "info", 0 );
        }
#endif
\t\tdata = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );''')
    # The failure-only probe leaves successful /info calls untouched. Stage it
    # only in the QEMU source tree so the pinned hardware image is byte stable.
    if stage.parent.name == 'airplay280_qemu':
        replace_once(server,
                     '\tHTTPConnectionSetTransportDelegate( cnx->httpCnx, &delegate );\n\tcnx->pairingVerified = true;',
                     '''\tHTTPConnectionSetTransportDelegate( cnx->httpCnx, &delegate );
#ifdef MHI2Q_QEMU_NTCP_TRACE
    fprintf( stderr, "[ntcp-qemu] pair-verify transport configured http=%p\\n", cnx->httpCnx );
#endif
\tcnx->pairingVerified = true;''')
        transport = stage / 'vendor/carplay-sdk/AccessorySDK/Support/NetTransportChaCha20Poly1305.c'
        replace_once(transport,
                     '#include "NetTransportChaCha20Poly1305.h"',
                     '#include "NetTransportChaCha20Poly1305.h"\n'
                     '#ifdef MHI2Q_QEMU_NTCP_TRACE\n#include <stdio.h>\n#endif')
        replace_once(transport,
                     '\tif( outLen ) *outLen = len;\n\treturn( err );',
                     '''#ifdef MHI2Q_QEMU_NTCP_TRACE
    fprintf( stderr, "[ntcp-qemu] read sock=%d state=%d err=%d len=%zu size=%zu offset=%zu\\n",
        (int) ctx->sock, (int) ctx->readState, (int) err, len, ctx->readSize, ctx->readOffset );
#endif
\tif( outLen ) *outLen = len;
\treturn( err );''')
        replace_once(server,
                     '#ifdef MHI2Q_QEMU_INFO_CFDIAG\nstatic void _MHI2QInfoCFDiag',
                     '#if defined(MHI2Q_QEMU_INFO_CFDIAG) || defined(MHI2Q_QEMU_INFO_FAILDIAG)\n'
                     'static void _MHI2QInfoCFDiag')
        replace_once(server,
                     '\t\tdata = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );\n'
                     '\t\trequire_action( data, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError );',
                     '''\t\tdata = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
#ifdef MHI2Q_QEMU_INFO_FAILDIAG
        if( !data && inRequest->header.urlLen >= 5 &&
            memcmp( inRequest->header.urlPtr, "/info", 5 ) == 0 )
        {
            static const char marker[] = "[info-faildiag] serialization failed; walking plist\\n";
            (void) write( 2, marker, sizeof( marker ) - 1 );
            _MHI2QInfoCFDiag( inPlist, "info", 0 );
        }
#endif
\t\trequire_action( data, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError );''')
    text = server.read_text()
    start = text.index('static HTTPStatus _requestProcessPairVerifyHomeKit(')
    end = text.index('static HTTPStatus _requestProcessPairVerify(', start)
    old = 'HTTPMessageSetBody( response, kMIMEType_Binary, outputPtr, outputLen )'
    if text[start:end].count(old) != 1:
        raise SystemExit('expected one pair-verify MIME anchor')
    server.write_text(text[:start] + text[start:end].replace(old,
                      'HTTPMessageSetBody( response, kPairingMIMEType, outputPtr, outputLen )') + text[end:])
    replace_once(stage / 'vendor/carplay-sdk/Sources/AirPlayVersion.h',
                 '#define kAirPlaySourceVersion\t\t\t210.81',
                 '#define kAirPlaySourceVersion\t\t\t280.33.8')
    print(f'Staged AirPlay 280 with MU1316 ABI and wireless pairing: {stage}')


if __name__ == '__main__':
    main()
