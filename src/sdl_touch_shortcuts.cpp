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

bool text_entry( const input_context &context )
{
    return context.allow_text_entry || cataimgui::client::want_text_input();
}

void append( std::vector<touch_shortcuts::entry> &entries, const input_event &event,
             const std::string &description, bool editing )
{
    if( event.sequence.size() != 1 ||
        ( event.type != input_event_t::keyboard_char && event.type != input_event_t::keyboard_code ) ) {
        return;
    }
    const int key = event.sequence.front();
    // Do not offer gameplay letters while a text field owns the keyboard.
    if( editing && key >= ' ' && key < 0x100 ) {
        return;
    }
    if( std::none_of( entries.begin(), entries.end(), [&]( const touch_shortcuts::entry & e ) {
        return e.input == event;
    } ) ) {
        entries.push_back( { event, event.short_description(), remove_color_tags( description ) } );
    }
}

std::vector<touch_shortcuts::entry> collect( input_context &context )
{
    std::vector<touch_shortcuts::entry> entries;
    const bool editing = text_entry( context );
    // Desktop has no Android swipe-to-confirm/back gestures, so keep these
    // commands accessible in the strip as well as through the controller.
    std::vector<std::string> actions = { "CONFIRM", "QUIT", "NEXT_TAB", "PREV_TAB", "HELP_KEYBINDINGS" };
    const auto &registered = context.get_registered_actions();
    actions.insert( actions.end(), registered.begin(), registered.end() );
    for( const std::string &action : actions ) {
        if( !context.is_registered_action( action ) || action == "ANY_INPUT" ||
            action == "COORDINATE" || action == "toggle_language_to_en" ) {
            continue;
        }
        auto keys = context.keys_bound_to( action, -1, false, true );
        if( keys.empty() ) {
            continue;
        }
        // Follow Android's preference for a simple printable shortcut, while
        // retaining the event type and every modifier needed by desktop SDL.
        std::stable_sort( keys.begin(), keys.end(), []( const input_event & a, const input_event & b ) {
            const auto rank = []( const input_event & event ) {
                const int key = event.sequence.front();
                return static_cast<int>( event.modifiers.size() ) * 2 +
                       ( key > ' ' && key < 127 ? 0 : 1 );
            };
            return rank( a ) < rank( b );
        } );
        append( entries, keys.front(), context.get_action_name( action ), editing );
    }
    for( const auto &manual : context.get_registered_manual_keys() ) {
        input_event event = manual.event;
        // Legacy manual hotkeys use character codes. Keep already-typed events
        // (including Shift/Ctrl) from uilists and NPC dialogue intact.
        if( !context.is_event_type_enabled( event.type ) &&
            event.type == input_event_t::keyboard_char && manual.key > 0 && manual.key < 128 ) {
            event.type = input_event_t::keyboard_code;
            if( manual.key >= 'A' && manual.key <= 'Z' ) {
                event.sequence.front() = manual.key - 'A' + 'a';
                event.modifiers.insert( keymod_t::shift );
            } else if( manual.key == '\n' ) {
                event.sequence.front() = keycode::return_;
            }
        }
        if( context.is_event_type_enabled( event.type ) ) {
            append( entries, event, manual.text, editing );
        }
    }
    return entries;
}

void draw_label( const SDL_Renderer_Ptr &renderer, Font &font, const std::string &text,
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
    RenderSetScale( renderer, scale, scale );
    const int text_x = ( x + ( width - cells * font.width * scale ) * 0.5f ) / scale;
    const int text_y = ( y + ( height - font.height * scale ) * 0.5f ) / scale;
    font.OutputChar( renderer, text, point( text_x, text_y ), color );
    RenderSetScale( renderer, 1.0f, 1.0f );
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
    input_context *context = input_context::input_context_stack.back();
    if( context ) {
        shortcuts.update( context->get_category(), collect( *context ) );
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
    shortcuts.update( context.get_category(), collect( context ) );
    shortcuts.remember( input );
}

void draw( const SDL_Renderer_Ptr &renderer, Font &font )
{
    if( !active || !focused || shortcuts.entries().empty() ) {
        return;
    }
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
        draw_label( renderer, font, label, slot * width, top, width, height, color );
    }
    const int selected = shortcuts.entry_for_slot( pressed );
    if( pressed >= 0 && selected >= 0 ) {
        const auto &entry = shortcuts.entries()[selected];
        const SDL_Rect hint{ 0, top - height, window_width, height };
        SetRenderDrawColor( renderer, 0, 0, 0, 220 );
        RenderFillRect( renderer, &hint );
        draw_label( renderer, font, entry.input.long_description() + "  " + entry.description,
                    0, top - height, window_width, height, color );
    }
    SetRenderDrawBlendMode( renderer, SDL_BLENDMODE_NONE );
}

bool handle_event( const SDL_Event &event, std::optional<input_event> &selected )
{
    if( !active ) {
        return false;
    }
    if( event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
        focused = false;
        // Keep ownership until release, including a release after refocusing.
        shortcuts.cancel();
    } else if( event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ) {
        focused = true;
    } else if( event.type == SDL_EVENT_WINDOW_RESIZED ||
               event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ) {
        // Keep ownership through release, but cancel the old hit target.
        shortcuts.cancel();
    }
    if( !focused ) {
        switch( event.type ) {
            case CATA_FINGERUP:
            case CATA_FINGERCANCELED:
                if( finger_owned == GetFingerID( event ) ) {
                    finger_owned.reset();
                }
                return true;
            case CATA_MOUSEBUTTONUP:
                mouse_owned = false;
                suppress_touch_mouse = false;
                return true;
            case CATA_FINGERDOWN:
            case CATA_FINGERMOTION:
            case CATA_MOUSEBUTTONDOWN:
            case CATA_MOUSEMOTION:
                return true;
            default:
                break;
        }
        return false;
    }
    switch( event.type ) {
        case CATA_FINGERDOWN: {
            if( finger_owned || mouse_owned ) {
                return true;
            }
            const float x = GetFingerX( event, window_width );
            const float y = GetFingerY( event, window_height );
            suppress_touch_mouse = shortcuts.contains( x, y );
            if( suppress_touch_mouse ) {
                finger_owned = GetFingerID( event );
                shortcuts.press( x, y );
                return true;
            }
            return false;
        }
        case CATA_FINGERMOTION:
        case CATA_FINGERUP:
        case CATA_FINGERCANCELED:
            if( finger_owned ) {
                if( *finger_owned == GetFingerID( event ) ) {
                    const float x = GetFingerX( event, window_width );
                    const float y = GetFingerY( event, window_height );
                    if( event.type == CATA_FINGERUP ) {
                        selected = shortcuts.release( x, y );
                        finger_owned.reset();
                    } else if( event.type == CATA_FINGERCANCELED ) {
                        shortcuts.cancel();
                        finger_owned.reset();
                    } else {
                        shortcuts.move( x, y );
                    }
                }
                return true;
            }
            return false;
        case CATA_MOUSEBUTTONDOWN:
        case CATA_MOUSEBUTTONUP:
            if( event.button.which == SDL_TOUCH_MOUSEID && suppress_touch_mouse ) {
                if( event.type == CATA_MOUSEBUTTONUP ) {
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
            if( event.type == CATA_MOUSEBUTTONDOWN ) {
                mouse_owned = shortcuts.press( event.button.x, event.button.y );
                return mouse_owned;
            }
            if( mouse_owned ) {
                selected = shortcuts.release( event.button.x, event.button.y );
                mouse_owned = false;
                return true;
            }
            return shortcuts.contains( event.button.x, event.button.y );
        case CATA_MOUSEMOTION:
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

void forward_to_imgui( const input_event &input, int width, int height, int scaling_factor )
{
    if( !imclient || input.sequence.size() != 1 ) {
        return;
    }
    SDL_Event key{};
    key.type = CATA_KEYDOWN;
    key.key.windowID = SDL_GetWindowID( get_sdl_window() );
    key.key.down = true;
    key.key.key = input.sequence.front();
    if( input.type == input_event_t::keyboard_char ) {
        switch( input.sequence.front() ) {
            case '\n':
                key.key.key = SDLK_RETURN;
                break;
            case KEY_BACKSPACE:
                key.key.key = SDLK_BACKSPACE;
                break;
            case KEY_UP:
                key.key.key = SDLK_UP;
                break;
            case KEY_DOWN:
                key.key.key = SDLK_DOWN;
                break;
            case KEY_LEFT:
                key.key.key = SDLK_LEFT;
                break;
            case KEY_RIGHT:
                key.key.key = SDLK_RIGHT;
                break;
            case KEY_PPAGE:
                key.key.key = SDLK_PAGEUP;
                break;
            case KEY_NPAGE:
                key.key.key = SDLK_PAGEDOWN;
                break;
            case KEY_HOME:
                key.key.key = SDLK_HOME;
                break;
            case KEY_END:
                key.key.key = SDLK_END;
                break;
            case KEY_DC:
                key.key.key = SDLK_DELETE;
                break;
            case KEY_BTAB:
                key.key.key = SDLK_TAB;
                key.key.mod = SDL_KMOD_SHIFT;
                break;
            default:
                break;
        }
    }
    for( keymod_t modifier : input.modifiers ) {
        switch( modifier ) {
            case keymod_t::shift:
                key.key.mod |= SDL_KMOD_SHIFT;
                break;
            case keymod_t::ctrl:
                key.key.mod |= SDL_KMOD_CTRL;
                break;
            case keymod_t::alt:
                key.key.mod |= SDL_KMOD_ALT;
                break;
        }
    }
    key.key.scancode = SDL_GetScancodeFromKey( key.key.key, nullptr );
    imclient->process_input( &key, width, height, scaling_factor );
    key.type = CATA_KEYUP;
    key.key.down = false;
    key.key.mod = SDL_KMOD_NONE;
    imclient->process_input( &key, width, height, scaling_factor );
}

} // namespace sdl_touch_shortcuts
#endif
