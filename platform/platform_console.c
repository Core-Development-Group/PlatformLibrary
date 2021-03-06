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

#include <PL/platform_console.h>
#include <PL/platform_filesystem.h>

#include "platform_private.h"
#include "graphics/graphics_private.h"

#include <errno.h>
#if defined(_WIN32)
#   include <Windows.h>
#   include <io.h>
#endif

#define CONSOLE_MAX_ARGUMENTS 8

/* Multi Console Manager */
// todo, should the console be case-sensitive?
// todo, mouse input callback
// todo, keyboard input callback

static PLConsoleCommand **_pl_commands = NULL;
static size_t _pl_num_commands = 0;
static size_t _pl_commands_size = 512;

void plRegisterConsoleCommand(const char *name, void(*CallbackFunction)(unsigned int argc, char *argv[]),
                              const char *description) {
    FunctionStart();

    if(name == NULL || name[0] == '\0') {
        ReportError(PL_RESULT_COMMAND_NAME, plGetResultString(PL_RESULT_COMMAND_NAME));
        return;
    }

    if(CallbackFunction == NULL) {
        ReportError(PL_RESULT_COMMAND_FUNCTION, plGetResultString(PL_RESULT_COMMAND_FUNCTION));
        return;
    }

    // Deal with resizing the array dynamically...
    if((1 + _pl_num_commands) > _pl_commands_size) {
        PLConsoleCommand **old_mem = _pl_commands;
        _pl_commands = (PLConsoleCommand**)realloc(_pl_commands, (_pl_commands_size += 128) * sizeof(PLConsoleCommand));
        if(!_pl_commands) {
            ReportError(PL_RESULT_MEMORY_ALLOCATION, "failed to allocate %d bytes",
                           _pl_commands_size * sizeof(PLConsoleCommand));
            _pl_commands = old_mem;
            _pl_commands_size -= 128;
            return;
        }
    }

    if(_pl_num_commands < _pl_commands_size) {
        _pl_commands[_pl_num_commands] = (PLConsoleCommand*)pl_malloc(sizeof(PLConsoleCommand));
        if(!_pl_commands[_pl_num_commands]) {
            return;
        }

        PLConsoleCommand *cmd = _pl_commands[_pl_num_commands];
        memset(cmd, 0, sizeof(PLConsoleCommand));
        cmd->Callback = CallbackFunction;
        strncpy(cmd->cmd, name, sizeof(cmd->cmd));
        if(description != NULL && description[0] != '\0') {
            strncpy(cmd->description, description, sizeof(cmd->description));
        }

        _pl_num_commands++;
    }
}

void plGetConsoleCommands( PLConsoleCommand ***cmds, size_t *num_cmds ) {
	*cmds = _pl_commands;
	*num_cmds = _pl_num_commands;
}

PLConsoleCommand *plGetConsoleCommand(const char *name) {
    for(PLConsoleCommand **cmd = _pl_commands; cmd < _pl_commands + _pl_num_commands; ++cmd) {
        if(pl_strcasecmp(name, (*cmd)->cmd) == 0) {
            return (*cmd);
        }
    }
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////

static PLConsoleVariable **_pl_variables = NULL;
static size_t _pl_num_variables = 0;
static size_t _pl_variables_size = 512;

PLConsoleVariable *plRegisterConsoleVariable(const char *name, const char *def, PLVariableType type,
                                             void(*CallbackFunction)(const PLConsoleVariable *variable),
                                             const char *desc) {
    FunctionStart();

    plAssert(_pl_variables);

    if(name == NULL || name[0] == '\0') {
        return NULL;
    }

    // Deal with resizing the array dynamically...
    if((1 + _pl_num_variables) > _pl_variables_size) {
        PLConsoleVariable **old_mem = _pl_variables;
        _pl_variables = (PLConsoleVariable**)realloc(_pl_variables, (_pl_variables_size += 128) * sizeof(PLConsoleVariable));
        if(_pl_variables == NULL) {
            ReportError(PL_RESULT_MEMORY_ALLOCATION, "failed to allocate %d bytes",
                           _pl_variables_size * sizeof(PLConsoleVariable));
            _pl_variables = old_mem;
            _pl_variables_size -= 128;
            return NULL;
        }
    }

    PLConsoleVariable *out = NULL;
    if(_pl_num_variables < _pl_variables_size) {
        _pl_variables[_pl_num_variables] = (PLConsoleVariable*)pl_malloc(sizeof(PLConsoleVariable));
        if(_pl_variables[_pl_num_variables] == NULL) {
            ReportError(PL_RESULT_MEMORY_ALLOCATION, "failed to allocate memory for ConsoleCommand, %d",
                        sizeof(PLConsoleVariable));
            return NULL;
        }

        out = _pl_variables[_pl_num_variables];
        memset(out, 0, sizeof(PLConsoleVariable));
        out->type = type;
        snprintf(out->var, sizeof(out->var), "%s", name);
        snprintf(out->default_value, sizeof(out->default_value), "%s", def);
        snprintf(out->description, sizeof(out->description), "%s", desc);

        plSetConsoleVariable(out, out->default_value);

        // Ensure the callback is only called afterwards
        if(CallbackFunction != NULL) {
            out->CallbackFunction = CallbackFunction;
        }

        _pl_num_variables++;
    }

    return out;
}

void plGetConsoleVariables( PLConsoleVariable*** vars, size_t* num_vars ) {
	*vars = _pl_variables;
	*num_vars = _pl_num_variables;
}

PLConsoleVariable *plGetConsoleVariable(const char *name) {
    for(PLConsoleVariable **var = _pl_variables; var < _pl_variables + _pl_num_variables; ++var) {
        if(pl_strcasecmp(name, (*var)->var) == 0) {
            return (*var);
        }
    }
    return NULL;
}

const char *plGetConsoleVariableValue( const char *name ) {
	PLConsoleVariable *var = plGetConsoleVariable( name );
	if ( var == NULL ) {
		return NULL;
	}

	return var->value;
}

const char *plGetConsoleVariableDefaultValue( const char *name ) {
	PLConsoleVariable *var = plGetConsoleVariable( name );
	if ( var == NULL ) {
		return NULL;
	}

	return var->default_value;
}

// Set console variable, with sanity checks...
void plSetConsoleVariable(PLConsoleVariable *var, const char *value) {
    plAssert(var);
    switch(var->type) {
        default: {
            Print("Unknown variable type %d, failed to set!\n", var->type);
        } return;

        case pl_int_var: {
            if(pl_strisdigit(value) != -1) {
                Print("Unknown argument type %s, failed to set!\n", value);
                return;
            }

            var->i_value = (int)strtol(value, NULL, 10);
        } break;

        case pl_string_var: {
            var->s_value = &var->value[0];
        } break;

        case pl_float_var: {
            var->f_value = strtof(value, NULL);
        } break;

        case pl_bool_var: {
            if(pl_strisalnum(value) == -1) {
                Print("Unknown argument type %s, failed to set!\n", value);
                return;
            }

            if(strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
                var->b_value = true;
            } else {
                var->b_value = false;
            }
        } break;
    }

    strncpy(var->value, value, sizeof(var->value));
    if(var->CallbackFunction != NULL) {
        var->CallbackFunction(var);
    }
}

void plSetConsoleVariableByName( const char *name, const char *value ) {
	PLConsoleVariable *var = plGetConsoleVariable( name );
	if ( var == NULL ) {
		Print( "Failed to find console variable \"%s\"!\n", name );
		return;
	}

	plSetConsoleVariable( var, value );
}

/////////////////////////////////////////////////////////////////////////////////////

#define CONSOLE_MAX_INSTANCES 4

#define CONSOLE_DEFAULT_COLOUR 128, 0, 0, 128

typedef struct PLConsolePane {
    PLRectangle2D display;
    PLColour colour;

    char buffer[4096];
    unsigned int buffer_pos;
} PLConsolePane;

static PLConsolePane console_panes[CONSOLE_MAX_INSTANCES];
static unsigned int num_console_panes;
static unsigned int active_console_pane = 0;

static bool console_visible = false;

/////////////////////////////////////////////////////////////////////////////////////
// PRIVATE

IMPLEMENT_COMMAND( pwd, "Print current working directory." ) {
	plUnused( argv );
	plUnused( argc );
	Print( "%s\n", plGetWorkingDirectory() );
}

IMPLEMENT_COMMAND( echo, "Prints out string to console." ) {
	for ( unsigned int i = 0; i < ( argc - 1 ); ++i ) {
		Print( "%s ", argv[ i ] );
    }
    Print("\n");
}

#if 0
IMPLEMENT_COMMAND(clear, "Clears the console buffer.") {
    memset(console_panes[active_console_pane].buffer, 0, 4096);
}
#endif

IMPLEMENT_COMMAND(colour, "Changes the colour of the current console.") {
	plUnused(argv);
	plUnused(argc);
    //console_panes[active_console_pane].
}

IMPLEMENT_COMMAND(time, "Prints out the current time.") {
	plUnused(argv);
	plUnused(argc);
    Print("%s\n", plGetFormattedTime());
}

IMPLEMENT_COMMAND(mem, "Prints out current memory usage.") {
	plUnused(argv);
	plUnused(argc);
}

IMPLEMENT_COMMAND(cmds, "Produces list of existing commands.") {
	plUnused(argv);
	plUnused(argc);
    for(PLConsoleCommand **cmd = _pl_commands; cmd < _pl_commands + _pl_num_commands; ++cmd) {
        Print(" %-20s : %-20s\n", (*cmd)->cmd, (*cmd)->description);
    }
    Print("%zu commands in total\n", _pl_num_commands);
}

IMPLEMENT_COMMAND(vars, "Produces list of existing variables.") {
	plUnused(argv);
	plUnused(argc);
    for(PLConsoleVariable **var = _pl_variables; var < _pl_variables + _pl_num_variables; ++var) {
        Print(" %-20s : %-5s / %-15s : %-20s\n",
               (*var)->var, (*var)->value, (*var)->default_value, (*var)->description);
    }
    Print("%zu variables in total\n", _pl_num_variables);
}

IMPLEMENT_COMMAND(help, "Returns information regarding specified command or variable.\nUsage: help <cmd/cvar>") {
    if(argc == 1) {
        // provide help on help, gross...
        Print("%s\n", help_var.description);
        return;
    }

    PLConsoleVariable *var = plGetConsoleVariable(argv[1]);
    if(var != NULL) {
        Print(" %-20s : %-5s / %-15s : %-20s\n",
                 var->var, var->value, var->default_value, var->description);
        return;
    }

    PLConsoleCommand *cmd = plGetConsoleCommand(argv[1]);
    if(cmd != NULL) {
        Print(" %-20s : %-20s\n", cmd->cmd, cmd->description);
        return;
    }

    Print("Unknown variable/command, %s!\n", argv[1]);
}

//////////////////////////////////////////////

//static PLMesh *mesh_line = NULL;
static PLBitmapFont *console_font = NULL;

void (*ConsoleOutputCallback)(int level, const char *msg);

static void InitializeDefaultLogLevels( void );
PLresult plInitConsole(void) {
    console_visible = false;

#if 0
    if((console_font = plCreateBitmapFont("fonts/console.font")) == NULL) {
        // todo, print warning...
    }
#endif

    ConsoleOutputCallback = NULL;

    memset(&console_panes, 0, sizeof(PLConsolePane) * CONSOLE_MAX_INSTANCES);
    active_console_pane = num_console_panes = 0;
    for(unsigned int i = 0; i < CONSOLE_MAX_INSTANCES; ++i) {
        console_panes[i].colour = plCreateColour4b(CONSOLE_DEFAULT_COLOUR);
    }

#if 0 /* todo, move this into graphics init code */
#if defined(PL_USE_GRAPHICS)
    if((mesh_line = plCreateMesh(PL_MESH_LINES, PL_DRAW_DYNAMIC, 0, 4)) == NULL) {
        return PL_RESULT_MEMORY_ALLOCATION;
    }
#endif
#endif

    if((_pl_commands = (PLConsoleCommand**)pl_malloc(sizeof(PLConsoleCommand*) * _pl_commands_size)) == NULL) {
        return PL_RESULT_MEMORY_ALLOCATION;
    }

    if((_pl_variables = (PLConsoleVariable**)pl_malloc(sizeof(PLConsoleVariable*) * _pl_variables_size)) == NULL) {
        return PL_RESULT_MEMORY_ALLOCATION;
    }

    PLConsoleCommand base_commands[]={
            //clear_var,
            help_var,
            time_var,
            mem_var,
            colour_var,
            cmds_var,
            vars_var,
            pwd_var,
            echo_var,
    };
    for(unsigned int i = 0; i < plArrayElements(base_commands); ++i) {
        plRegisterConsoleCommand(base_commands[i].cmd, base_commands[i].Callback, base_commands[i].description);
    }

	/* initialize our internal log levels here
	 * as we depend on the console variables being setup first */
	InitializeDefaultLogLevels();

    return PL_RESULT_SUCCESS;
}

void plShutdownConsole(void) {
    console_visible = false;

#if defined(PL_USE_GRAPHICS)
    plDestroyBitmapFont(console_font);
#endif

    memset(&console_panes, 0, sizeof(PLConsolePane) * CONSOLE_MAX_INSTANCES);
    active_console_pane = num_console_panes = 0;

    if(_pl_commands) {
        for(PLConsoleCommand **cmd = _pl_commands; cmd < _pl_commands + _pl_num_commands; ++cmd) {
            // todo, should we return here; assume it's the end?
            if ((*cmd) == NULL) {
                continue;
            }

            pl_free((*cmd));
        }
        pl_free(_pl_commands);
    }

    if(_pl_variables) {
        for(PLConsoleVariable **var = _pl_variables; var < _pl_variables + _pl_num_variables; ++var) {
            // todo, should we return here; assume it's the end?
            if ((*var) == NULL) {
                continue;
            }

            pl_free((*var));
        }
        pl_free(_pl_variables);
    }

    ConsoleOutputCallback = NULL;
}

void plSetConsoleOutputCallback(void(*Callback)(int level, const char *msg)) {
    ConsoleOutputCallback = Callback;
}

/////////////////////////////////////////////////////

/**
 * Takes a string and returns a list of possible options.
 */
const char **plAutocompleteConsoleString( const char *string, unsigned int *numElements ) {
#define MAX_AUTO_OPTIONS 16
	static const char *options[ MAX_AUTO_OPTIONS ];
	unsigned int c = 0;
	/* gather all the console variables */
	for ( unsigned int i = 0; i < _pl_num_variables; ++i ) {
		if ( c >= MAX_AUTO_OPTIONS ) {
			break;
		} else if ( pl_strncasecmp( string, _pl_variables[ i ]->var, strlen( string ) ) != 0 ) {
			continue;
		}
        options[ c++ ] = _pl_variables[ i ]->var;
	}
	/* gather up all the console commands */
	for ( unsigned int i = 0; i < _pl_num_commands; ++i ) {
		if ( c >= MAX_AUTO_OPTIONS ) {
			break;
		} else if ( pl_strncasecmp( string, _pl_commands[ i ]->cmd, strlen( string ) ) != 0 ) {
			continue;
		}
		options[ c++ ] = _pl_commands[ i ]->cmd;
	}

	*numElements = c;
	return options;
}

void plParseConsoleString(const char *string) {
    if(string == NULL || string[0] == '\0') {
        DebugPrint("Invalid string passed to ParseConsoleString!\n");
        return;
    }

    plLogMessage(LOG_LEVEL_LOW, string);

    static char **argv = NULL;
    if(argv == NULL) {
        if((argv = (char**)pl_malloc(sizeof(char*) * CONSOLE_MAX_ARGUMENTS)) == NULL) {
            return;
        }
        for(char **arg = argv; arg < argv + CONSOLE_MAX_ARGUMENTS; ++arg) {
            (*arg) = (char*)pl_malloc(sizeof(char) * 1024);
            if((*arg) == NULL) {
                break; // continue to our doom... ?
            }
        }
    }

    unsigned int argc = 0;
    for(const char *pos = string; *pos;) {
        size_t arglen = strcspn(pos, " ");
        if(arglen > 0) {
            strncpy(argv[argc], pos, arglen);
            argv[argc][arglen] = '\0';
            ++argc;
        }
        pos += arglen;
        pos += strspn(pos, " ");
    }

    PLConsoleVariable *var;
    PLConsoleCommand *cmd;

    if((var = plGetConsoleVariable(argv[0])) != NULL) {
        // todo, should the var not be set by defacto here?

        if(argc > 1) {
            plSetConsoleVariable(var, argv[1]);
        } else {
            Print("    %s\n", var->var);
            Print("    %s\n", var->description);
            Print("    %-10s : %s\n", var->value, var->default_value);
        }
    } else if((cmd = plGetConsoleCommand(argv[0])) != NULL) {
        if(cmd->Callback != NULL) {
            cmd->Callback(argc, argv);
        } else {
            Print("    Invalid command, no callback provided!\n");
            Print("    %s\n", cmd->cmd);
            Print("    %s\n", cmd->description);
        }
    } else {
        Print("Unknown variable/command, %s!\n", argv[0]);
    }
}

#define _MAX_ROWS       2
#define _MAX_COLUMNS    2

// todo, correctly handle rows and columns.
static void ResizeConsoles(void) {
#if defined(PL_USE_GRAPHICS)
    unsigned int screen_w = gfx_state.current_viewport->w;
    unsigned int screen_h = gfx_state.current_viewport->h;
    if(screen_w == 0 || screen_h == 0) {
        screen_w = 640;
        screen_h = 480;
    }
    unsigned int width = screen_w; // / num_console_panes;
    if(num_console_panes > 1) {
        width = screen_w / 2;
    }
    unsigned int height = screen_h;
    if(num_console_panes > 2) {
        height = screen_h / 2;
    }
    unsigned int position_x = 0, position_y = 0;
    for(unsigned int i = 0; i < num_console_panes; i++) {
        if(i > 0) {
            position_x += width;
            if(position_x >= screen_w) {
                // Move onto the next row
                position_x = 0;
                position_y += height;
                if(position_y > screen_h) {
                    // Return back to the first row (this shouldn't occur...)
                    position_y = 0;
                }
            }
        }

        console_panes[i].display.wh.x = (float)width;
        console_panes[i].display.wh.y = (float)height;
        console_panes[i].display.xy.x = (float)position_x;
        console_panes[i].display.xy.x = (float)position_y;
    }
#endif
}

static bool IsConsolePaneVisible(unsigned int id) {
    if(!console_visible) {
        return false;
    }

    if(
        console_panes[id].display.ll.a == 0 &&
        console_panes[id].display.lr.a == 0 &&
        console_panes[id].display.ul.a == 0 &&
        console_panes[id].display.ur.a == 0 ) {
        return false;
    }

    return true;
}

// INPUT

void plConsoleInput(int m_x, int m_y, unsigned int m_buttons, bool is_pressed) {
    if(!is_pressed) {
        return;
    }

    for(unsigned int i = 0; i < num_console_panes; i++) {
        if(!IsConsolePaneVisible(i)) {
            continue;
        }

        PLConsolePane *pane = &console_panes[i];

        int pane_min_x = (int)pane->display.xy.x;
        int pane_max_x = pane_min_x + (int)(pane->display.wh.x);
        if(m_x < pane_min_x || m_x > pane_max_x) {
            continue;
        }

        int pane_min_y = (int)pane->display.xy.y;
        int pane_max_y = pane_min_y + (int)(pane->display.wh.y);
        if(m_y < pane_min_y || m_y > pane_max_y) {
            continue;
        }

#if 0
        if(m_buttons & PLINPUT_MOUSE_LEFT) {
            active_console_pane = i;

            pane->display.xy.x += m_x; pane->display.xy.y += m_y;
            if(pane->display.xy.x <= gfx_state.viewport_x) {
                pane->display.xy.x = gfx_state.viewport_x + 1;
            } else if(pane->display.xy.x >= gfx_state.viewport_width) {
                pane->display.xy.x = gfx_state.viewport_width - 1;
            } else if(pane->display.xy.y <= gfx_state.viewport_y) {
                pane->display.xy.y = gfx_state.viewport_y + 1;
            } else if(pane->display.xy.y >= gfx_state.viewport_height) {
                pane->display.xy.y = gfx_state.viewport_height - 1;
            }

            static int old_x = 0, old_y = 0;

            return;
        }

        if(m_buttons & PLINPUT_MOUSE_RIGHT) {
        // todo, display context menu
            return;
        }
#endif

        return;
    }

    // If we reached here, then we failed to hit anything...

#if 0
    if(m_buttons & PLINPUT_MOUSE_RIGHT) {
    // todo, display context menu
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// PUBLIC

// GENERAL

void plSetupConsole(unsigned int num_instances) {
    if(num_instances == 0) {
        return;
    }

    if(num_instances > CONSOLE_MAX_INSTANCES) {
        num_instances = CONSOLE_MAX_INSTANCES;
    }

    memset(&console_panes, 0, sizeof(PLConsolePane) * num_instances);

    for(unsigned int i = 0; i < num_instances; i++) {
        console_panes[i].colour = plCreateColour4b(CONSOLE_DEFAULT_COLOUR);
        plSetRectangleUniformColour(&console_panes[i].display, console_panes[i].colour);
    }

    num_console_panes = num_instances;
    ResizeConsoles();
}

void plShowConsole(bool show) {
    console_visible = show;
}

void plSetConsoleColour(unsigned int id, PLColour colour) {
    plSetRectangleUniformColour(&console_panes[id-1].display, colour);
}

#define _COLOUR_INACTIVE_ALPHA_TOP      128
#define _COLOUR_INACTIVE_ALPHA_BOTTOM   80

#define _COLOUR_HEADER_INACTIVE         20, 0, 0, _COLOUR_INACTIVE_ALPHA_TOP

#define _COLOUR_ACTIVE_ALPHA_TOP        255
#define _COLOUR_ACTIVE_ALPHA_BOTTOM     128

#define _COLOUR_HEADER_ACTIVE_TOP       128, 0, 0, _COLOUR_ACTIVE_ALPHA_TOP
#define _COLOUR_HEADER_ACTIVE_BOTTOM    82, 0, 0, _COLOUR_ACTIVE_ALPHA_BOTTOM

#if defined(PL_USE_GRAPHICS)
void plDrawConsole(void) {
#if 0
    ResizeConsoles();

    for(unsigned int i = 0; i < num_console_panes; i++) {
        if(!IsConsolePaneVisible(i)) {
            continue;
        }

        if(i == active_console_pane) {
            plDrawFilledRectangle( &console_panes[i].display );

            plDrawFilledRectangle(plCreateRectangle(
                    PLVector2(
                            console_panes[i].display.xy.x + 4,
                            console_panes[i].display.xy.y + 4
                    ),

                    PLVector2(
                            console_panes[i].display.wh.x - 8,
                            20
                    ),

                    plCreateColour4b(_COLOUR_HEADER_ACTIVE_TOP),
                    plCreateColour4b(_COLOUR_HEADER_ACTIVE_TOP),
                    plCreateColour4b(_COLOUR_HEADER_ACTIVE_BOTTOM),
                    plCreateColour4b(_COLOUR_HEADER_ACTIVE_BOTTOM)
            ));

            // todo, console title
            // todo, display scroll bar
        } else {
            plDrawFilledRectangle(plCreateRectangle(
                    PLVector2(
                            console_panes[i].display.xy.x,
                            console_panes[i].display.xy.y
                    ),

                    PLVector2(
                            console_panes[i].display.wh.x,
                            console_panes[i].display.wh.y
                    ),

                    PLColour(
                            (uint8_t) (console_panes[i].display.ul.r / 2),
                            (uint8_t) (console_panes[i].display.ul.g / 2),
                            (uint8_t) (console_panes[i].display.ul.b / 2),
                            _COLOUR_INACTIVE_ALPHA_TOP
                    ),

                    PLColour(
                            (uint8_t) (console_panes[i].display.ur.r / 2),
                            (uint8_t) (console_panes[i].display.ur.g / 2),
                            (uint8_t) (console_panes[i].display.ur.b / 2),
                            _COLOUR_INACTIVE_ALPHA_TOP
                    ),

                    PLColour(
                            (uint8_t) (console_panes[i].display.ll.r / 2),
                            (uint8_t) (console_panes[i].display.ll.g / 2),
                            (uint8_t) (console_panes[i].display.ll.b / 2),
                            _COLOUR_INACTIVE_ALPHA_BOTTOM
                    ),

                    PLColour(
                            (uint8_t) (console_panes[i].display.lr.r / 2),
                            (uint8_t) (console_panes[i].display.lr.g / 2),
                            (uint8_t) (console_panes[i].display.lr.b / 2),
                            _COLOUR_INACTIVE_ALPHA_BOTTOM
                    )
            ));

            plDrawFilledRectangle(plCreateRectangle(
                    PLVector2(
                            console_panes[i].display.xy.x + 4,
                            console_panes[i].display.xy.y + 4
                    ),

                    PLVector2(
                            console_panes[i].display.wh.x - 8,
                            20
                    ),

                    plCreateColour4b(_COLOUR_HEADER_INACTIVE),
                    plCreateColour4b(_COLOUR_HEADER_INACTIVE),
                    plCreateColour4b(_COLOUR_HEADER_INACTIVE),
                    plCreateColour4b(_COLOUR_HEADER_INACTIVE)
            ));

            // todo, dim console title
        }

        // todo, display buffer size display
        // todo, display buffer text
    }

    if(console_font == NULL) {
        return;
    }

#if 0
#if defined(PL_MODE_OPENGL)
    glEnable(GL_TEXTURE_RECTANGLE);

    glBindTexture(GL_TEXTURE_RECTANGLE, console_font->texture->id);
#endif
#endif

#if 0
#if defined(PL_MODE_OPENGL)
    glDisable(GL_TEXTURE_RECTANGLE);
#endif
#endif
#endif
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

/*	Log System	*/

#define MAX_LOG_LEVELS  512

typedef struct LogLevel {
    bool isReserved;
    char prefix[64];    // e.g. 'warning, 'error'
    PLColour colour;
    PLConsoleVariable *var;
} LogLevel;
static LogLevel levels[MAX_LOG_LEVELS];

int LOG_LEVEL_LOW = 0;
int LOG_LEVEL_MEDIUM = 0;
int LOG_LEVEL_HIGH = 0;
int LOG_LEVEL_DEBUG = 0;

int LOG_LEVEL_GRAPHICS = 0;
int LOG_LEVEL_FILESYSTEM = 0;
int LOG_LEVEL_MODEL = 0;

static void InitializeDefaultLogLevels( void ) {
	memset( levels, 0, sizeof( LogLevel ) * MAX_LOG_LEVELS );

	LOG_LEVEL_LOW = plAddLogLevel( "libplatform", ( PLColour ){ 255, 255, 255, 255 }, true );
	LOG_LEVEL_MEDIUM = plAddLogLevel( "libplatform/warning", ( PLColour ){ 255, 255, 0, 255 }, true );
	LOG_LEVEL_HIGH = plAddLogLevel( "libplatform/error", ( PLColour ){ 255, 0, 0, 255 }, true );
	LOG_LEVEL_DEBUG = plAddLogLevel( "libplatform/debug", ( PLColour ){ 255, 255, 255, 255 },
#if !defined( NDEBUG )
	                 true
#else
	                 false
#endif
	);
	LOG_LEVEL_GRAPHICS = plAddLogLevel( "libplatform/graphics", ( PLColour ){ 0, 255, 255, 255 },
#if !defined( NDEBUG )
	                 true
#else
	                 false
#endif
	);
	LOG_LEVEL_FILESYSTEM = plAddLogLevel( "libplatform/filesystem", ( PLColour ){ 0, 255, 255, 255 }, true );
	LOG_LEVEL_MODEL = plAddLogLevel( "libplatform/model", ( PLColour ){ 0, 255, 255, 255 }, true );
}

/**
 * Converts an external log level id into an
 * internal one.
 */
static LogLevel *GetLogLevelForId( int id ) {
	if ( id >= MAX_LOG_LEVELS ) {
		ReportError( PL_RESULT_MEMORY_EOA, "failed to find slot for log level %d", id );
		return NULL;
	}

	return &levels[ id ];
}

/**
 * Fetches the next unreserved slot.
 */
static int GetNextFreeLogLevel( void ) {
	for ( int i = 0; i < MAX_LOG_LEVELS; ++i ) {
		if( !levels[ i ].isReserved ) {
			return i;
		}
	}

	return -1;
}

/////////////////////////////////////////////////////////////////////////////////////
// public

static char logOutputPath[ PL_SYSTEM_MAX_PATH ] = { '\0' };

void plSetupLogOutput(const char *path) {
    if(path == NULL || path[0] == '\0') {
        return;
    }

    strncpy( logOutputPath, path, sizeof( logOutputPath ));
    if(plFileExists( logOutputPath )) {
        unlink( logOutputPath );
    }
}

int plAddLogLevel( const char *prefix, PLColour colour, bool status ) {
	int i = GetNextFreeLogLevel();
	if ( i == -1 ) {
		return -1;
	}

	LogLevel *l = &levels[ i ];
    l->colour = colour;
    l->isReserved = true;

    if ( prefix != NULL && prefix[ 0 ] != '\0' ) {
        snprintf( l->prefix, sizeof( l->prefix ), "%s", prefix );
    }

    char var[32];
    snprintf(var, sizeof(var), "pl.log.level.%s", prefix);
    l->var = plRegisterConsoleVariable(var, status ? "1" : "0", pl_bool_var, NULL, "Console output level.");

	return i;
}

void plSetLogLevelStatus( int id, bool status ) {
	LogLevel *l = GetLogLevelForId( id );
	if ( l == NULL ) {
		return;
	}

	plSetConsoleVariable( l->var, status ? "1" : "0" );
}

void plLogMessage(int id, const char *msg, ...) {
    LogLevel *l = GetLogLevelForId( id );
    if(l == NULL || !l->var->b_value) {
        return;
    }

    char buf[4096] = {'\0'};

    // add the prefix to the start
    int c = 0;
    if(l->prefix[0] != '\0') {
        c = snprintf(buf, sizeof(buf), "[%s] %s: ", plGetFormattedTime(), l->prefix);
    } else {
        c = snprintf(buf, sizeof(buf), "[%s]: ", plGetFormattedTime());
    }

    va_list args;
    va_start(args, msg);
    c += vsnprintf(buf + c, sizeof(buf) - c, msg, args);
    va_end(args);

    if(buf[c - 1] != '\n') {
        strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    }

    printf("%s", buf);

#if defined( _WIN32 )
    OutputDebugString( buf );
#endif

    // todo, decide how we're going to pass it to the console/log

    if(ConsoleOutputCallback != NULL) {
        ConsoleOutputCallback(id, buf);
    }

    static bool avoid_recursion = false;
    if(!avoid_recursion) {
        if ( logOutputPath[0] != '\0') {
            size_t size = strlen(buf);
            FILE *file = fopen( logOutputPath, "a");
            if (file != NULL) {
                if (fwrite(buf, sizeof(char), size, file) != size) {
                    avoid_recursion = true;
                    ReportError(PL_RESULT_FILEERR, "failed to write to log, %s\n%s", logOutputPath, strerror(errno));
                }
                fclose(file);
                return;
            }

            // todo, needs to be more appropriate; return details on exact issue
            avoid_recursion = true;
            ReportError(PL_RESULT_FILEREAD, "failed to open %s", logOutputPath );
        }
    }
}
