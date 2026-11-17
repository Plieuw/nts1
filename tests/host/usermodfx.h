#pragma once
#include "logue_host.h"
enum { k_user_modfx_param_time=0, k_user_modfx_param_depth };
#define MODFX_INIT _hook_init
#define MODFX_PROCESS _hook_process
#define MODFX_PARAM _hook_param
#define MODFX_SUSPEND _hook_suspend
#define MODFX_RESUME _hook_resume
