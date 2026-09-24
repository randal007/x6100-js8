#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dialog.h"

extern dialog_t *dialog_channels;

/*
 * Recall the previous/next Channel Memory entry.
 * direction < 0: previous; direction > 0: next.
 */
bool dialog_channels_recall_relative(int8_t direction);

/*
 * Name of the last successfully recalled Channel Memory entry.
 * Returns an empty string if no channel is currently recalled.
 */
const char *dialog_channels_last_recalled_name(void);

/*
 * Request that the next opening of Channel Memory immediately
 * adds the current radio settings as a new channel.
 */
void dialog_channels_add_on_open(void);
