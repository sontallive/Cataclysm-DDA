#include "input_context.h"
#include "touch_shortcuts_context.h"

#include <cassert>
#include <cstdio>

int main()
{
    auto &stack = input_context::input_context_stack;
    const auto initial_size = stack.size();
    {
        input_context menu( "menu" );
        menu.register_action( "CONFIRM" );
        input_event key( 'a', input_event_t::keyboard_code );
        key.modifiers = { keymod_t::shift, keymod_t::ctrl };
        menu.register_manual_key( key, "Action" );
        menu.set_edittext( "composition" );
        {
            input_context copy( menu );
            assert( stack.back() == &copy );
            assert( copy.get_registered_manual_keys().front().event == key );
            assert( copy.get_edittext() == "composition" );
            copy = menu;
            assert( stack.size() == initial_size + 2 );
            {
                const input_context::scoped_activation active( menu );
                assert( stack.back() == &menu );
                {
                    input_context popup( "STRING_INPUT" );
                    assert( stack.back() == &popup );
                    assert( touch_shortcuts::text_entry( popup ) );
                }
                assert( stack.back() == &menu );
            }
            assert( stack.back() == &copy );
        }
        assert( stack.back() == &menu );
        assert( !touch_shortcuts::text_entry( menu ) );
        menu.allow_text_entry = true;
        assert( touch_shortcuts::text_entry( menu ) );
    }
    assert( stack.size() == initial_size );
    puts( "Context copy, assignment, nested activation, cleanup and text-entry detection passed" );
}
