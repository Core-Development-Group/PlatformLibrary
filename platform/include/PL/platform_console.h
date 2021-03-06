/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/
#pragma once

#include <PL/platform.h>
#include <PL/platform_math.h>

/* todo: make this structure private */
typedef struct PLConsoleVariable {
	char var[ 32 ];
	char description[ 256 ];

	PLVariableType type;

	void ( *CallbackFunction )( const struct PLConsoleVariable *variable );

	/////////////////////////////

#define PL_VAR_VALUE_LENGTH 512

	union {
		float f_value;
		int i_value;
		const char *s_value;
		bool b_value;
	};
	char value[ PL_VAR_VALUE_LENGTH ];
	char default_value[ PL_VAR_VALUE_LENGTH ];

	/////////////////////////////

	bool archive;
} PLConsoleVariable;

PL_EXTERN_C

void plGetConsoleVariables( PLConsoleVariable ***vars, size_t *num_vars );

PLConsoleVariable *plGetConsoleVariable( const char *name );
const char *plGetConsoleVariableValue( const char *name );
const char *plGetConsoleVariableDefaultValue( const char *name );
void plSetConsoleVariable( PLConsoleVariable *var, const char *value );
void plSetConsoleVariableByName( const char *name, const char *value );

PLConsoleVariable *plRegisterConsoleVariable( const char *name, const char *def, PLVariableType type,
                                              void ( *CallbackFunction )( const PLConsoleVariable *variable ),
                                              const char *desc );

PL_EXTERN_C_END

/////////////////////////////////////////////////////////////////////////////////////

typedef struct PLConsoleCommand {
	char cmd[ 24 ];

	void ( *Callback )( unsigned int argc, char *argv[] );

	char description[ 512 ];
} PLConsoleCommand;

PL_EXTERN_C

PL_EXTERN void plGetConsoleCommands( PLConsoleCommand ***cmds, size_t *num_cmds );
PL_EXTERN void plRegisterConsoleCommand( const char *name, void ( *CallbackFunction )( unsigned int argc, char *argv[] ), const char *description );
PL_EXTERN PLConsoleCommand *plGetConsoleCommand( const char *name );

PL_EXTERN void plSetupConsole( unsigned int num_instances );

PL_EXTERN void plSetConsoleColour( unsigned int id, PLColour colour );
PL_EXTERN void plSetConsoleOutputCallback( void ( *Callback )( int level, const char *msg ) );

PL_EXTERN const char **plAutocompleteConsoleString( const char *string, unsigned int *numElements );
PL_EXTERN void plParseConsoleString( const char *string );

PL_EXTERN void plShowConsole( bool show );
PL_EXTERN void plDrawConsole( void );

/////////////////////////////////////////////////////////////////////////////////////

extern void plSetupLogOutput( const char *path );
extern int plAddLogLevel( const char *prefix, PLColour colour, bool status );
extern void plSetLogLevelStatus( int id, bool status );
extern void plLogMessage( int id, const char *msg, ... );

PL_EXTERN_C_END