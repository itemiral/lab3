#ifndef __PREF_BOP_PARAM_H__
#define __PREF_BOP_PARAM_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* extern all of the variables defined in core.param.def */

#define DEF_PARAM(name, variable, type, func, def, const) \
  extern const type variable;
#include "pref_bop.param.def"
#undef DEF_PARAM

/**************************************************************************************/

#endif
