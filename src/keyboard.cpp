#include "stdint.h"
#include <stddef.h>

// Forward declarations
void kputchar(char c);

// I/O port functions
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Keyboard port definitions
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// US keyboard layout scan code to ASCII mapping
static const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Keyboard state
static bool shift_pressed = false;
static bool caps_lock_on = false;
static bool last_was_e0 = false;

// Checks if the keyboard has a key available
bool keyboard_has_key() {
    return (inb(KEYBOARD_STATUS_PORT) & 1);
}

// Waits for a keypress and returns the ASCII character
extern "C" char keyboard_read_char() {
    uint8_t scancode;
    char c = 0;

    // Wait for a key to be pressed
    while (!c) {
        // Wait until a key is available
        while (!keyboard_has_key()) {
            // Small delay to reduce CPU usage
            for (int i = 0; i < 1000; i++) {
                asm volatile("nop");
            }
        }

        // Read the scan code
        scancode = inb(KEYBOARD_DATA_PORT);

        // Handle extended keys
        if (scancode == 0xE0) {
            last_was_e0 = true;
            continue;
        }

        // Handle key release
        if (scancode & 0x80) {
            scancode &= 0x7F; // Clear high bit

            if (scancode == 0x2A || scancode == 0x36) { // Shift keys
                shift_pressed = false;
            }

            last_was_e0 = false;
            continue;
        }

        // Handle special keys
        if (scancode == 0x2A || scancode == 0x36) { // Shift keys
            shift_pressed = true;
            continue;
        }

        if (scancode == 0x3A) { // Caps lock
            caps_lock_on = !caps_lock_on;
            continue;
        }

        // Handle extended keys
        if (last_was_e0) {
            last_was_e0 = false;
            continue; // Skip extended keys for now
        }

        // Get ASCII character
        if (scancode < 128) {
            c = scancode_to_ascii[scancode];

            // Apply shift for letters
            if (c >= 'a' && c <= 'z') {
                if ((shift_pressed && !caps_lock_on) || (!shift_pressed && caps_lock_on)) {
                    c = c - 'a' + 'A'; // Convert to uppercase
                }
            } else if (shift_pressed) {
                // Apply shift for numbers and symbols
                switch (c) {
                    case '1': c = '!'; break;
                    case '2': c = '@'; break;
                    case '3': c = '#'; break;
                    case '4': c = '$'; break;
                    case '5': c = '%'; break;
                    case '6': c = '^'; break;
                    case '7': c = '&'; break;
                    case '8': c = '*'; break;
                    case '9': c = '('; break;
                    case '0': c = ')'; break;
                    case '-': c = '_'; break;
                    case '=': c = '+'; break;
                    case '[': c = '{'; break;
                    case ']': c = '}'; break;
                    case '\\': c = '|'; break;
                    case ';': c = ':'; break;
                    case '\'': c = '"'; break;
                    case ',': c = '<'; break;
                    case '.': c = '>'; break;
                    case '/': c = '?'; break;
                    case '`': c = '~'; break;
                }
            }
        }
    }

    return c;
}

// Initialize the keyboard
void keyboard_init() {
    // Reset keyboard state
    shift_pressed = false;
    caps_lock_on = false;
    last_was_e0 = false;

    // Flush the keyboard buffer
    while (keyboard_has_key()) {
        inb(KEYBOARD_DATA_PORT);
    }
}
