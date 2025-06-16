#ifndef __LIBMOD_BASIC_H  
#define __LIBMOD_BASIC_H  
  
#include "bgddl.h"  
#include "libbggfx.h" // Necesario para GRAPH, REGION, etc.  
  
// Solo declaraciones extern, NO definiciones  
extern DLSYSFUNCS __bgdexport(libmod_basic, functions_exports)[];  
extern char * __bgdexport(libmod_basic, modules_dependency)[];  
  
extern void __bgdexport(libmod_basic, module_initialize)();  
extern void __bgdexport(libmod_basic, module_finalize)();  
  
// Declaraciones de tus funciones  
extern int64_t libmod_basic_init_outrun(INSTANCE *my, int64_t *params);  
extern int64_t libmod_basic_update_outrun(INSTANCE *my, int64_t *params);  
extern int64_t libmod_basic_draw_outrun(INSTANCE *my, int64_t *params);  
extern int64_t libmod_basic_return_42(INSTANCE *my, int64_t *params);  
  
#endif