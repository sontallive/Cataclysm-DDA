#pragma once
#ifndef CATA_SRC_TOUCH_SHORTCUTS_CONTEXT_H
#define CATA_SRC_TOUCH_SHORTCUTS_CONTEXT_H

#if defined(TILES) && !defined(__ANDROID__)
#include "touch_shortcuts.h"

class input_context;

namespace touch_shortcuts
{
bool text_entry( input_context &context );
// This is the only bridge from CDDA action/manual-key metadata to the model.
std::vector<entry> collect( input_context &context, bool editing );
} // namespace touch_shortcuts
#endif
#endif // CATA_SRC_TOUCH_SHORTCUTS_CONTEXT_H
