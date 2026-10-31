#include "sdl_touch_shortcuts.h"

#if defined(TILES) && !defined(__ANDROID__)
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cata_imgui.h"
#include "catacharset.h"
#include "input.h"
#include "input_context.h"
#include "output.h"
#include "sdl_font.h"
#include "sdltiles.h"
#include "touch_shortcuts.h"
#include "touch_shortcuts_context.h"
#include "ui_manager.h"

namespace sdl_touch_shortcuts
{
namespace
{
bool active = false;
bool focused = true;
bool mouse_owned = false;
bool suppress_touch_mouse = false;
std::optional<SDL_FingerID> finger_owned;
touch_shortcuts::bar shortcuts;
int window_width = 0;
int window_height = 0;

bool text_entry( input_context &context )
{
    return touch_shortcuts::text_entry( context ) ||
           ( ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput );
}

void draw_label( const SDL_Renderer_Ptr &renderer, const GeometryRenderer_Ptr &geometry, Font &font,
                 const std::string &text,
                 float x, float y, float width, float height, unsigned char color )
{
    const int cells = utf8_width( text );
    if( cells <= 0 || font.width <= 0 || font.height <= 0 ) {
        return;
    }
    const float scale = std::min( { 1.5f, height * 0.65f / font.height,
                                    ( width - 12.0f ) / ( cells * font.width ) } );
    if( scale <= 0.0f ) {
        return;
    }
    float base_x = 1.0f;
    float base_y = 1.0f;
    SDL_RenderGetScale( renderer.get(), &base_x, &base_y );
    SDL_RenderSetScale( renderer.get(), base_x * scale, base_y * scale );
    const int text_x = ( x + ( width - cells * font.width * scale ) * 0.5f ) / scale;
    const int text_y = ( y + ( height - font.height * scale ) * 0.5f ) / scale;
    font.OutputChar( renderer, geometry, text, point( text_x, text_y ), color );
    SDL_RenderSetScale( renderer.get(), base_x, base_y );
}
} // namespace

void enable()
{
    active = true;
}

bool enabled()
{
    return active;
}

void sync( int width, int height, int font_height )
{
    if( !active ) {
        return;
    }
    window_width = width;
    window_height = height;
    input_context *context = input_context::input_context_stack.empty() ? nullptr :
                             input_context::input_context_stack.back();
    if( context ) {
        shortcuts.update( context->get_category(), touch_shortcuts::collect( *context,
                text_entry( *context ) ) );
    } else {
        shortcuts.update( "", {} );
    }
    shortcuts.resize( width, height, std::clamp( font_height * 3, 56, 96 ) );
}

void remember( input_context &context, const input_event &input )
{
    if( !active || text_entry( context ) ) {
        return;
    }
    shortcuts.update( context.get_category(), touch_shortcuts::collect( context, false ) );
    shortcuts.remember( input );
}

void draw( const SDL_Renderer_Ptr &renderer, const GeometryRenderer_Ptr &geometry, Font &font )
{
    if( !active || !focused || shortcuts.entries().empty() ||
        window_width <= 0 || window_height <= 0 ) {
        return;
    }
    // Hit testing uses logical window coordinates. Presenting uses drawable
    // pixels, which can differ on HiDPI displays or when changing monitors.
    int output_width = 0;
    int output_height = 0;
    SDL_GetRendererOutputSize( renderer.get(), &output_width, &output_height );
    float old_scale_x = 1.0f;
    float old_scale_y = 1.0f;
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_RenderGetScale( renderer.get(), &old_scale_x, &old_scale_y );
    SDL_GetRenderDrawBlendMode( renderer.get(), &old_blend );
    SDL_RenderSetScale( renderer.get(), static_cast<float>( output_width ) / window_width,
                        static_cast<float>( output_height ) / window_height );
    const auto &palette = windowsPalette;
    const auto brightest = std::max_element( palette.begin(), palette.end(),
        []( const SDL_Color & a, const SDL_Color & b ) {
        return a.r * 299 + a.g * 587 + a.b * 114 < b.r * 299 + b.g * 587 + b.b * 114;
    } );
    const unsigned char color = static_cast<unsigned char>( brightest - palette.begin() );
    const float width = shortcuts.button_width();
    const int height = shortcuts.button_height();
    const int top = shortcuts.top();
    const int pressed = shortcuts.pressed_slot();
    SetRenderDrawBlendMode( renderer, SDL_BLENDMODE_BLEND );
    for( int slot = 0; slot < shortcuts.columns(); ++slot ) {
        const int index = shortcuts.entry_for_slot( slot );
        std::string label;
        if( shortcuts.paged() && slot == 0 ) {
            label = "<";
        } else if( shortcuts.paged() && slot == shortcuts.columns() - 1 ) {
            label = ">";
        } else if( index >= 0 ) {
            label = shortcuts.entries()[index].label;
        } else {
            continue;
        }
        const SDL_Rect rect{ static_cast<int>( slot * width ) + 2, top,
                             static_cast<int>( width ) - 4, height };
        const Uint8 shade = slot == pressed ? 64 : 0;
        SetRenderDrawColor( renderer, shade, shade, shade, slot == pressed ? 240 : 190 );
        RenderFillRect( renderer, &rect );
        draw_label( renderer, geometry, font, label, slot * width, top, width, height, color );
    }
    const int selected = shortcuts.entry_for_slot( pressed );
    if( pressed >= 0 && selected >= 0 ) {
        const auto &entry = shortcuts.entries()[selected];
        const SDL_Rect hint{ 0, top - height, window_width, height };
        SetRenderDrawColor( renderer, 0, 0, 0, 220 );
        RenderFillRect( renderer, &hint );
        draw_label( renderer, geometry, font, entry.input.long_description() + "  " + entry.description,
                    0, top - height, window_width, height, color );
    }
    SetRenderDrawBlendMode( renderer, old_blend );
    SDL_RenderSetScale( renderer.get(), old_scale_x, old_scale_y );
}

bool handle_event( const SDL_Event &event, std::optional<input_event> &selected )
{
    if( !active ) {
        return false;
    }
    if( event.type == SDL_WINDOWEVENT ) {
        switch( event.window.event ) {
            case SDL_WINDOWEVENT_FOCUS_LOST:
            case SDL_WINDOWEVENT_MINIMIZED:
                focused = false;
                shortcuts.cancel();
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                focused = true;
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
                shortcuts.cancel();
                break;
            default:
                break;
        }
    }
    if( !focused ) {
        switch( event.type ) {
            case SDL_FINGERUP:
                if( finger_owned == event.tfinger.fingerId ) {
                    finger_owned.reset();
                }
                return true;
            case SDL_MOUSEBUTTONUP:
                mouse_owned = false;
                suppress_touch_mouse = false;
                return true;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
                return true;
            default:
                break;
        }
        return false;
    }
    switch( event.type ) {
        case SDL_FINGERDOWN: {
            if( finger_owned || mouse_owned ) {
                return true;
            }
            const float x = event.tfinger.x * window_width;
            const float y = event.tfinger.y * window_height;
            suppress_touch_mouse = shortcuts.contains( x, y );
            if( suppress_touch_mouse ) {
                finger_owned = event.tfinger.fingerId;
                shortcuts.press( x, y );
                return true;
            }
            return false;
        }
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
            if( finger_owned ) {
                if( *finger_owned == event.tfinger.fingerId ) {
                    const float x = event.tfinger.x * window_width;
                    const float y = event.tfinger.y * window_height;
                    if( event.type == SDL_FINGERUP ) {
                        selected = shortcuts.release( x, y );
                        finger_owned.reset();
                    } else {
                        shortcuts.move( x, y );
                    }
                }
                return true;
            }
            return false;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if( event.button.which == SDL_TOUCH_MOUSEID && suppress_touch_mouse ) {
                if( event.type == SDL_MOUSEBUTTONUP ) {
                    suppress_touch_mouse = false;
                }
                return true;
            }
            if( finger_owned ) {
                return true;
            }
            if( event.button.button != SDL_BUTTON_LEFT ) {
                return shortcuts.contains( event.button.x, event.button.y );
            }
            if( event.type == SDL_MOUSEBUTTONDOWN ) {
                mouse_owned = shortcuts.press( event.button.x, event.button.y );
                return mouse_owned;
            }
            if( mouse_owned ) {
                selected = shortcuts.release( event.button.x, event.button.y );
                mouse_owned = false;
                return true;
            }
            return shortcuts.contains( event.button.x, event.button.y );
        case SDL_MOUSEMOTION:
            if( event.motion.which == SDL_TOUCH_MOUSEID && suppress_touch_mouse ) {
                return true;
            }
            if( mouse_owned ) {
                shortcuts.move( event.motion.x, event.motion.y );
                return true;
            }
            return shortcuts.contains( event.motion.x, event.motion.y );
        default:
            return false;
    }
}

void forward_to_imgui( const input_event &input, SDL_Window *window )
{
    if( !imclient || input.sequence.size() != 1 ) {
        return;
    }
    SDL_Event key{};
    key.type = SDL_KEYDOWN;
    key.key.windowID = SDL_GetWindowID( window );
    key.key.state = SDL_PRESSED;
    key.key.keysym.sym = input.sequence.front();
    if( input.type == input_event_t::keyboard_char ) {
        switch( input.sequence.front() ) {
            case '\n':
                key.key.keysym.sym = SDLK_RETURN;
                break;
            case KEY_BACKSPACE:
                key.key.keysym.sym = SDLK_BACKSPACE;
                break;
            case KEY_UP:
                key.key.keysym.sym = SDLK_UP;
                break;
            case KEY_DOWN:
                key.key.keysym.sym = SDLK_DOWN;
                break;
            case KEY_LEFT:
                key.key.keysym.sym = SDLK_LEFT;
                break;
            case KEY_RIGHT:
                key.key.keysym.sym = SDLK_RIGHT;
                break;
            case KEY_PPAGE:
                key.key.keysym.sym = SDLK_PAGEUP;
                break;
            case KEY_NPAGE:
                key.key.keysym.sym = SDLK_PAGEDOWN;
                break;
            case KEY_HOME:
                key.key.keysym.sym = SDLK_HOME;
                break;
            case KEY_END:
                key.key.keysym.sym = SDLK_END;
                break;
            case KEY_DC:
                key.key.keysym.sym = SDLK_DELETE;
                break;
            case KEY_BTAB:
                key.key.keysym.sym = SDLK_TAB;
                key.key.keysym.mod = KMOD_SHIFT;
                break;
            default:
                break;
        }
    }
    for( keymod_t modifier : input.modifiers ) {
        switch( modifier ) {
            case keymod_t::shift:
                key.key.keysym.mod |= KMOD_SHIFT;
                break;
            case keymod_t::ctrl:
                key.key.keysym.mod |= KMOD_CTRL;
                break;
            case keymod_t::alt:
                key.key.keysym.mod |= KMOD_ALT;
                break;
        }
    }
    key.key.keysym.scancode = SDL_GetScancodeFromKey( key.key.keysym.sym );
    imclient->process_input( &key );
    key.type = SDL_KEYUP;
    key.key.state = SDL_RELEASED;
    key.key.keysym.mod = KMOD_NONE;
    imclient->process_input( &key );
}

} // namespace sdl_touch_shortcuts
#endif
