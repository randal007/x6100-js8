#pragma once

#include <stdbool.h>

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_navtex;
bool dialog_navtex_is_active(void);

#ifdef __cplusplus
}
#endif
