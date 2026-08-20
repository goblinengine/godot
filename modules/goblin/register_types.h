/**************************************************************************/
/*  register_types.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#ifndef GOBLIN_REGISTER_TYPES_H
#define GOBLIN_REGISTER_TYPES_H

#include "modules/register_module_types.h"

void initialize_goblin_module(ModuleInitializationLevel p_level);
void uninitialize_goblin_module(ModuleInitializationLevel p_level);
/* Used for docgen */
void preregister_goblin_types();

#endif // GOBLIN_REGISTER_TYPES_H
