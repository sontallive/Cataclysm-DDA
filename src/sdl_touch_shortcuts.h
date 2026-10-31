#pragma once
#ifndef CATA_SRC_SDL_TOUCH_SHORTCUTS_H
#define CATA_SRC_SDL_TOUCH_SHORTCUTS_H

#if defined(TILES) && !defined(__ANDROID__)
#include <optional>

#include "input_enums.h"
#include "sdl_wrappers.h"
#include "sdl_geometry.h"

class Font;
class input_context;

namespace sdl_touch_shortcuts
{
void enable();
bool enabled();
void sync( int width, int height, int font_height );
void draw( const SDL_Renderer_Ptr &renderer, const GeometryRenderer_Ptr &geometry, Font &font );
// Runs before ImGui or map hit testing. A captured gesture must never fall
// through to controls under the strip, even after a context or window change.
bool handle_event( const SDL_Event &event, std::optional<input_event> &selected );
void remember( input_context &context, const input_event &input );
void forward_to_imgui( const input_event &input, SDL_Window *window );
} // namespace sdl_touch_shortcuts
#endif
#endif // CATA_SRC_SDL_TOUCH_SHORTCUTS_H
