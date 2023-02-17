/*
sys_psvita.c - psvita backend
Copyright (C) 2021-2023 fgsfds

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "platform/platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <vitasdk.h>
#include <vitaGL.h>
#include <vrtld.h>

#define DATA_PATH "data/xash3d"

// 200MB libc heap, 512K main thread stack, 32MB for loading game DLLs, 8MB vertex pool
// the rest goes to vitaGL
SceUInt32 sceUserMainThreadStackSize = 512 * 1024;
unsigned int _pthread_stack_default_user = 512 * 1024;
unsigned int _newlib_heap_size_user = 200 * 1024 * 1024;
#define VGL_MEM_THRESHOLD ( 32 * 1024 * 1024 )
#define VGL_VERTEX_POOL_SIZE ( 8 * 1024 * 1024 )

/* HACK: stubs for GL functions that are missing from vitaGL */

static void glDrawBuffer( GLenum which )
{
	/* nada */
}

/* end of GL stubs*/

/* HACKHACK: force-export stuff required by the dynamic libs */

extern void *__aeabi_idiv;
extern void *__aeabi_uidiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_uidivmod;
extern void *__aeabi_d2ulz;
extern void *__aeabi_ul2d;

static const vrtld_export_t aux_exports[] =
{
	VRTLD_EXPORT_SYMBOL( __aeabi_d2ulz ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_uidivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_uidiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_ul2d ),
	VRTLD_EXPORT_SYMBOL( ctime ),
	VRTLD_EXPORT_SYMBOL( vasprintf ),
	VRTLD_EXPORT_SYMBOL( vsprintf ),
	VRTLD_EXPORT_SYMBOL( vprintf ),
	VRTLD_EXPORT_SYMBOL( printf ),
	VRTLD_EXPORT_SYMBOL( putchar ),
	VRTLD_EXPORT_SYMBOL( puts ),
	VRTLD_EXPORT_SYMBOL( tolower ),
	VRTLD_EXPORT_SYMBOL( toupper ),
	VRTLD_EXPORT_SYMBOL( isalnum ),
	VRTLD_EXPORT_SYMBOL( isalpha ),
	VRTLD_EXPORT_SYMBOL( strchrnul ),
	VRTLD_EXPORT_SYMBOL( rand ),
	VRTLD_EXPORT_SYMBOL( srand ),
	VRTLD_EXPORT_SYMBOL( glDrawBuffer ),
	VRTLD_EXPORT( "dlopen", vrtld_dlopen ),
	VRTLD_EXPORT( "dlclose", vrtld_dlclose ),
	VRTLD_EXPORT( "dlsym", vrtld_dlsym ),
};

const vrtld_export_t *__vrtld_exports = aux_exports;
const size_t __vrtld_num_exports = sizeof( aux_exports ) / sizeof( *aux_exports );

/* end of export crap */

void Platform_ShellExecute( const char *path, const char *parms )
{
	Con_Reportf( S_WARN "Tried to shell execute ;%s; -- not supported\n", path );
}

void PSVita_Init( void )
{
	char xashdir[1024] = { 0 };

	// cd to the base dir immediately for library loading to work
	if ( PSVita_GetBasePath( xashdir, sizeof( xashdir ) ) )
	{
		chdir( xashdir );
	}

	sceCtrlSetSamplingModeExt( SCE_CTRL_MODE_ANALOG_WIDE );
	sceTouchSetSamplingState( SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START );
	scePowerSetArmClockFrequency( 444 );
	scePowerSetBusClockFrequency( 222 );
	scePowerSetGpuClockFrequency( 222 );
	scePowerSetGpuXbarClockFrequency( 166 );
	sceSysmoduleLoadModule( SCE_SYSMODULE_NET );

	if ( vrtld_init( 0 ) < 0 )
	{
		Sys_Error( "Could not init vrtld: %s\n", vrtld_dlerror( ) );
	}

	// init vitaGL with some memory budget for immediate mode vertices
	// TODO: we don't need to do this for ref_soft
	vglUseVram( GL_TRUE );
	vglUseExtraMem( GL_TRUE );
	vglInitExtended( VGL_VERTEX_POOL_SIZE, 960, 544, VGL_MEM_THRESHOLD, 0 );
}

void PSVita_Shutdown( void )
{
	vrtld_quit( );
}

qboolean PSVita_GetBasePath( char *buf, const size_t buflen )
{
	// check if a xash3d folder exists on one of these drives
	// default to the last one (ux0)
	static const char *drives[] = { "uma0", "imc0", "ux0" };
	SceUID dir;
	size_t i;

	for ( i = 0; i < sizeof( drives ) / sizeof( *drives ); ++i )
	{
		Q_snprintf( buf, buflen, "%s:" DATA_PATH, drives[i] );
		dir = sceIoDopen( buf );
		if ( dir >= 0 )
		{
			sceIoDclose( dir );
			return true;
		}
	}

	return false;
}