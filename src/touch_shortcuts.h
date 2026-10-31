#pragma once
#ifndef CATA_SRC_TOUCH_SHORTCUTS_H
#define CATA_SRC_TOUCH_SHORTCUTS_H

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "input_enums.h"

namespace touch_shortcuts
{

struct entry {
    input_event input;
    std::string label;
    std::string description;

    bool operator==( const entry &other ) const {
        return input == other.input && label == other.label && description == other.description;
    }
};

// Platform-independent state for the desktop port of Android's shortcut strip.
// History only reorders keys that are still valid in the current input context.
class bar
{
    public:
        bool update( const std::string &category, const std::vector<entry> &available );
        void remember( const input_event &input );
        void resize( int width, int height, int button_height );
        bool contains( float x, float y ) const;
        bool press( float x, float y );
        void move( float x, float y );
        std::optional<input_event> release( float x, float y );
        void cancel();

        const std::vector<entry> &entries() const {
            return ordered;
        }
        int first() const {
            return page * capacity;
        }
        int visible_count() const;
        int page_count() const;
        int current_page() const {
            return page;
        }
        int columns() const {
            return capacity + ( paged() ? 2 : 0 );
        }
        int top() const {
            return window_height - height;
        }
        int button_height() const {
            return height;
        }
        float button_width() const;
        bool paged() const {
            return page_count() > 1;
        }
        int pressed_slot() const {
            return pressed == hovered ? pressed : -1;
        }
        int entry_for_slot( int slot ) const;
        bool is_pressed() const {
            return pressed >= 0;
        }

    private:
        void reorder();
        int slot_at( float x, float y ) const;
        std::string category;
        std::vector<entry> available;
        std::vector<entry> ordered;
        std::map<std::string, std::vector<input_event>> history;
        int window_width = 0;
        int window_height = 0;
        int height = 0;
        int capacity = 1;
        int page = 0;
        int pressed = -1;
        int hovered = -1;
};

} // namespace touch_shortcuts
#endif // CATA_SRC_TOUCH_SHORTCUTS_H
