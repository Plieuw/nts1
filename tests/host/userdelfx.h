#pragma once
#include "logue_host.h"
enum { k_user_delfx_param_time=0, k_user_delfx_param_depth, k_user_delfx_param_reserved0, k_user_delfx_param_shift_depth };
#define DELFX_INIT _hook_init
#define DELFX_PROCESS _hook_process
#define DELFX_PARAM _hook_param
#define DELFX_SUSPEND _hook_suspend
#define DELFX_RESUME _hook_resume
