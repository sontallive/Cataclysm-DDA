#include <string>
#include <vector>

#include "cata_catch.h"
#include "touch_shortcuts.h"

static touch_shortcuts::entry shortcut_key( int key )
{
    return { input_event( key, input_event_t::keyboard_code ), std::to_string( key ), "test" };
}

TEST_CASE( "touch_shortcuts_preserve_modified_events", "[touch_shortcuts]" )
{
    touch_shortcuts::bar bar;
    auto shortcut = shortcut_key( 'a' );
    shortcut.input.modifiers = { keymod_t::shift, keymod_t::ctrl };
    bar.update( "menu", { shortcut } );
    bar.resize( 1280, 800, 56 );
    REQUIRE( bar.press( 20, 770 ) );
    const auto result = bar.release( 20, 770 );
    REQUIRE( result );
    CHECK( ( *result == shortcut.input ) );
    CHECK_FALSE( bar.release( 20, 770 ) );
}

TEST_CASE( "touch_shortcuts_cancel_stale_or_dragged_presses", "[touch_shortcuts]" )
{
    touch_shortcuts::bar bar;
    bar.update( "menu", { shortcut_key( 'a' ) } );
    bar.resize( 1280, 800, 56 );
    REQUIRE( bar.press( 20, 770 ) );
    SECTION( "drag outside" ) {
        bar.move( 20, 500 );
        CHECK_FALSE( bar.release( 20, 500 ) );
    }
    SECTION( "new dialog" ) {
        bar.update( "dialog", { shortcut_key( 'a' ) } );
        CHECK_FALSE( bar.release( 20, 770 ) );
    }
    SECTION( "same category but changed list" ) {
        bar.update( "menu", { shortcut_key( 'b' ) } );
        CHECK_FALSE( bar.release( 20, 770 ) );
    }
    SECTION( "resize" ) {
        bar.resize( 800, 800, 56 );
        CHECK_FALSE( bar.release( 20, 770 ) );
    }
    SECTION( "focus lost or touch canceled" ) {
        bar.cancel();
        CHECK_FALSE( bar.release( 20, 770 ) );
    }
}

TEST_CASE( "touch_shortcuts_history_is_contextual_and_validated", "[touch_shortcuts]" )
{
    touch_shortcuts::bar bar;
    const auto a = shortcut_key( 'a' );
    const auto b = shortcut_key( 'b' );
    bar.update( "menu", { a, b } );
    bar.remember( b.input );
    CHECK( ( bar.entries().front().input == b.input ) );
    bar.remember( b.input );
    CHECK( bar.entries().size() == 2 );
    bar.remember( shortcut_key( 'x' ).input );
    CHECK( bar.entries().size() == 2 );
    bar.update( "other", { a, b } );
    CHECK( ( bar.entries().front().input == a.input ) );
    bar.update( "menu", { a, b } );
    CHECK( ( bar.entries().front().input == b.input ) );
    bar.update( "menu", { a } );
    REQUIRE( bar.entries().size() == 1 );
    CHECK( ( bar.entries().front().input == a.input ) );
}

TEST_CASE( "touch_shortcuts_page_without_emitting_game_input", "[touch_shortcuts]" )
{
    touch_shortcuts::bar bar;
    std::vector<touch_shortcuts::entry> entries;
    for( int key = 'a'; key <= 'z'; ++key ) {
        entries.push_back( shortcut_key( key ) );
    }
    bar.update( "menu", entries );
    bar.resize( 640, 480, 56 );
    REQUIRE( bar.paged() );
    REQUIRE( bar.press( 620, 450 ) );
    CHECK_FALSE( bar.release( 620, 450 ) );
    REQUIRE( bar.current_page() == 1 );
    REQUIRE( bar.press( 80, 450 ) );
    auto result = bar.release( 80, 450 );
    REQUIRE( result );
    CHECK( ( *result == entries[bar.first()].input ) );
    REQUIRE( bar.press( 20, 450 ) );
    CHECK_FALSE( bar.release( 20, 450 ) );
    CHECK( bar.current_page() == 0 );
    CHECK_FALSE( bar.contains( 20, 300 ) );
}

TEST_CASE( "touch_shortcuts_handle_empty_and_zero_size_windows", "[touch_shortcuts]" )
{
    touch_shortcuts::bar bar;
    bar.update( "empty", {} );
    bar.resize( 0, 0, 56 );
    CHECK_FALSE( bar.press( 0, 0 ) );
    CHECK_FALSE( bar.release( 0, 0 ) );
    bar.update( "menu", { shortcut_key( 'a' ) } );
    CHECK_FALSE( bar.press( 0, 0 ) );
    bar.resize( 1280, 800, 56 );
    CHECK( bar.press( 20, 770 ) );
    bar.update( "empty", {} );
    CHECK_FALSE( bar.release( 20, 770 ) );
}
