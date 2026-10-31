#include "touch_shortcuts.h"

#include <algorithm>

namespace touch_shortcuts
{

bool bar::update( const std::string &next_category, const std::vector<entry> &next )
{
    if( category == next_category && available == next ) {
        return false;
    }
    // A dialog or dynamic list changed while a finger was down: do not activate
    // whatever happens to replace the old button at the same position.
    cancel();
    category = next_category;
    available = next;
    page = 0;
    reorder();
    return true;
}

void bar::reorder()
{
    ordered.clear();
    for( const input_event &recent : history[category] ) {
        const auto found = std::find_if( available.begin(), available.end(), [&]( const entry & e ) {
            return e.input == recent;
        } );
        if( found != available.end() ) {
            ordered.push_back( *found );
        }
    }
    for( const entry &candidate : available ) {
        if( std::none_of( ordered.begin(), ordered.end(), [&]( const entry & e ) {
            return e.input == candidate.input;
        } ) ) {
            ordered.push_back( candidate );
        }
    }
}

void bar::remember( const input_event &input )
{
    if( std::none_of( available.begin(), available.end(), [&]( const entry & e ) {
        return e.input == input;
    } ) ) {
        return;
    }
    auto &recent = history[category];
    recent.erase( std::remove( recent.begin(), recent.end(), input ), recent.end() );
    recent.insert( recent.begin(), input );
    if( recent.size() > 12 ) {
        recent.resize( 12 );
    }
    cancel();
    page = 0;
    reorder();
}

void bar::resize( int width, int window_h, int button_h )
{
    width = std::max( 0, width );
    window_h = std::max( 0, window_h );
    button_h = std::clamp( button_h, 0, window_h );
    const int slots = std::max( 3, width / std::max( 64, button_h ) );
    const int next_capacity = static_cast<int>( ordered.size() ) > slots ? slots - 2 : slots;
    if( width != window_width || window_h != window_height || button_h != height ||
        next_capacity != capacity ) {
        cancel();
        window_width = width;
        window_height = window_h;
        height = button_h;
        capacity = next_capacity;
        page = std::min( page, page_count() - 1 );
    }
}

int bar::page_count() const
{
    return std::max( 1, ( static_cast<int>( ordered.size() ) + capacity - 1 ) / capacity );
}

int bar::visible_count() const
{
    return std::min( capacity, static_cast<int>( ordered.size() ) - first() );
}

float bar::button_width() const
{
    return static_cast<float>( window_width ) / columns();
}

bool bar::contains( float x, float y ) const
{
    if( ordered.empty() || window_width <= 0 || height <= 0 ||
        x < 0 || x >= window_width || y < top() || y >= window_height ) {
        return false;
    }
    const int slot = static_cast<int>( x / button_width() );
    // Only painted buttons own input; transparent unused slots remain usable
    // by the underlying menu, including on the last page.
    return entry_for_slot( slot ) >= 0 ||
           ( paged() && ( slot == 0 || slot == columns() - 1 ) );
}

int bar::slot_at( float x, float y ) const
{
    return contains( x, y ) ? static_cast<int>( x / button_width() ) : -1;
}

int bar::entry_for_slot( int slot ) const
{
    const int offset = slot - ( paged() ? 1 : 0 );
    return offset >= 0 && offset < visible_count() ? first() + offset : -1;
}

bool bar::press( float x, float y )
{
    pressed = hovered = slot_at( x, y );
    return pressed >= 0;
}

void bar::move( float x, float y )
{
    hovered = slot_at( x, y );
}

std::optional<input_event> bar::release( float x, float y )
{
    const int slot = slot_at( x, y );
    const bool activate = pressed >= 0 && pressed == slot;
    cancel();
    if( !activate ) {
        return std::nullopt;
    }
    if( paged() && slot == 0 ) {
        page = ( page + page_count() - 1 ) % page_count();
    } else if( paged() && slot == columns() - 1 ) {
        page = ( page + 1 ) % page_count();
    } else {
        const int index = entry_for_slot( slot );
        if( index >= 0 ) {
            return ordered[index].input;
        }
    }
    return std::nullopt;
}

void bar::cancel()
{
    pressed = hovered = -1;
}

} // namespace touch_shortcuts
