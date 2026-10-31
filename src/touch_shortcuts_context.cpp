#include "touch_shortcuts_context.h"

#if defined(TILES) && !defined(__ANDROID__)
#include <algorithm>

#include "input.h"
#include "input_context.h"
#include "output.h"

namespace touch_shortcuts
{
namespace
{
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

} // namespace

bool text_entry( input_context &context )
{
    const std::string &category = context.get_category();
    return context.allow_text_entry || category == "STRING_INPUT" || category == "STRING_EDITOR";
}

std::vector<touch_shortcuts::entry> collect( input_context &context, bool editing )
{
    std::vector<touch_shortcuts::entry> entries;
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

} // namespace touch_shortcuts
#endif
