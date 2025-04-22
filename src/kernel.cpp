#include "stdint.h"
#include <stddef.h>

// Forward declaration of VShell handler
void vshell_init();
void vshell_execute_command(const char* command);

// VGA buffer address
volatile uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;
const uint8_t VGA_WIDTH = 80;
const uint8_t VGA_HEIGHT = 25;
uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

// Command buffer
char command_buffer[256];
int command_position = 0;

// IO port functions
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// PS/2 keyboard ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Keyboard scan codes for special keys
#define SC_ENTER 0x1C
#define SC_BACKSPACE 0x0E
#define SC_ARROW_UP 0x48
#define SC_ARROW_DOWN 0x50
#define SC_ARROW_LEFT 0x4B
#define SC_ARROW_RIGHT 0x4D

// Basic US keyboard layout scan code to ASCII
const char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Simple keyboard state
bool shift_pressed = false;
bool caps_lock = false;
bool last_was_e0 = false;
bool keyboard_initialized = false;

// Command history
#define HISTORY_SIZE 10
char command_history[HISTORY_SIZE][256];
int history_count = 0;
int history_position = -1;

// String functions
void str_copy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int str_length(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Update hardware cursor position
void update_cursor() {
    unsigned short position = cursor_y * VGA_WIDTH + cursor_x;

    // Tell VGA hardware the cursor position
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

// Clear the screen - now exported with extern "C"
extern "C" void clear_screen() {
    // Black background, white text (blank)
    const uint16_t blank = 0x0F00;

    // Fill entire screen with blank characters
    for(int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }

    // Reset cursor position
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// Move cursor to next line
void new_line() {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= VGA_HEIGHT) {
        // Simple scrolling - move everything up one line
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
            }
        }

        // Clear the last line
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = 0x0F00;
        }

        cursor_y = VGA_HEIGHT - 1;
    }

    update_cursor();
}

// Print a single character
void kputchar(char c) {
    // Handle special characters
    if (c == '\n') {
        new_line();
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            const unsigned int position = cursor_y * VGA_WIDTH + cursor_x;
            VGA_MEMORY[position] = 0x0F00;  // White on black space
            update_cursor();
        }
        return;
    }

    // Calculate position
    const unsigned int position = cursor_y * VGA_WIDTH + cursor_x;

    // Write character with white on black
    VGA_MEMORY[position] = (uint16_t)c | 0x0F00;

    // Move cursor
    cursor_x++;

    // Handle line wrapping
    if (cursor_x >= VGA_WIDTH) {
        new_line();
    } else {
        update_cursor();
    }
}

// Print a string
void kprint(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        kputchar(str[i]);
    }
}

// Display command prompt
void display_prompt() {
    kprint("\n> ");
    command_position = 0;
    command_buffer[0] = '\0';
    history_position = -1;
}

// Execute the current command
void execute_command() {
    // Null terminate the command
    command_buffer[command_position] = '\0';

    // Add to history if not empty
    if (command_position > 0) {
        // Shift history if needed
        if (history_count == HISTORY_SIZE) {
            for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                str_copy(command_history[i], command_history[i + 1]);
            }
            history_count--;
        }

        // Add current command to history
        str_copy(command_history[history_count], command_buffer);
        history_count++;
    }

    // Execute the command
    vshell_execute_command(command_buffer);

    // Show a new prompt
    display_prompt();
}

// Update command line with current buffer
void update_command_line() {
    // Move cursor to start of command
    cursor_x = 2; // After the "> "

    // Clear the current line
    for (int i = cursor_x; i < VGA_WIDTH; i++) {
        const unsigned int position = cursor_y * VGA_WIDTH + i;
        VGA_MEMORY[position] = 0x0F00;  // White on black space
    }

    // Display the command buffer
    for (int i = 0; i < command_position; i++) {
        kputchar(command_buffer[i]);
    }

    update_cursor();
}

// Handle history navigation (up/down arrows)
void handle_history_up() {
    if (history_count > 0 && history_position < history_count - 1) {
        history_position++;
        str_copy(command_buffer, command_history[history_count - 1 - history_position]);
        command_position = str_length(command_buffer);
        update_command_line();
    }
}

void handle_history_down() {
    if (history_position > 0) {
        history_position--;
        str_copy(command_buffer, command_history[history_count - 1 - history_position]);
        command_position = str_length(command_buffer);
        update_command_line();
    } else if (history_position == 0) {
        history_position = -1;
        command_position = 0;
        command_buffer[0] = '\0';
        update_command_line();
    }
}

// Handle left/right arrow keys
void handle_arrow_left() {
    if (command_position > 0) {
        command_position--;
        cursor_x--;
        update_cursor();
    }
}

void handle_arrow_right() {
    if (command_buffer[command_position] != '\0') {
        command_position++;
        cursor_x++;
        update_cursor();
    }
}

// Initialize keyboard
void init_keyboard() {
    // Basic initialization - more comprehensive would be needed for a real OS
    keyboard_initialized = true;
}

// Check if keyboard has input
bool keyboard_has_input() {
    return (inb(KEYBOARD_STATUS_PORT) & 1);
}

// Process keyboard input
void process_keypress() {
    if (!keyboard_has_input()) {
        return;
    }

    // Read scan code
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle extended keys (E0 prefix)
    if (scancode == 0xE0) {
        last_was_e0 = true;
        return;
    }

    // Handle key release (high bit set)
    if (scancode & 0x80) {
        scancode &= 0x7F; // Clear high bit to get the key code

        // Handle shift key release
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = false;
        }

        last_was_e0 = false;
        return;
    }

    // Handle arrow keys (after E0 prefix)
    if (last_was_e0) {
        last_was_e0 = false;

        switch (scancode) {
            case SC_ARROW_UP:
                handle_history_up();
                return;
            case SC_ARROW_DOWN:
                handle_history_down();
                return;
            case SC_ARROW_LEFT:
                handle_arrow_left();
                return;
            case SC_ARROW_RIGHT:
                handle_arrow_right();
                return;
            default:
                return; // Other extended keys not handled
        }
    }

    // Handle special keys
    if (scancode == 0x2A || scancode == 0x36) {  // Left or right shift
        shift_pressed = true;
        return;
    }

    if (scancode == 0x3A) {  // Caps lock
        caps_lock = !caps_lock;
        return;
    }

    if (scancode == SC_ENTER) {  // Enter key
        kputchar('\n'); // Move to next line
        execute_command();
        return;
    }

    if (scancode == SC_BACKSPACE) {  // Backspace key
        if (command_position > 0) {
            // Move everything in the buffer back one position
            for (int i = command_position - 1; i < 255 && command_buffer[i] != '\0'; i++) {
                command_buffer[i] = command_buffer[i + 1];
            }
            command_position--;

            // Update display
            cursor_x = 2; // Move to start of prompt
            for (int i = 0; i < command_position; i++) {
                kputchar(command_buffer[i]);
            }
            kputchar(' '); // Clear the last character
            cursor_x--; // Move back one
            update_cursor();
        }
        return;
    }

    // Convert scancode to ASCII
    char c = kbdus[scancode];

    // Apply shift/caps logic for letters
    if (c >= 'a' && c <= 'z') {
        if ((shift_pressed && !caps_lock) || (!shift_pressed && caps_lock)) {
            c = c - 'a' + 'A';  // Convert to uppercase
        }
    } else if (shift_pressed) {
        // Apply shift for non-letter keys
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

    // Add character to command buffer and display it
    if (c && command_position < 255) {
        // Insert character at current position
        for (int i = 255; i > command_position; i--) {
            command_buffer[i] = command_buffer[i - 1];
        }
        command_buffer[command_position] = c;
        command_position++;
        command_buffer[command_position] = '\0';

        // Print the character
        kputchar(c);
    }
}

// Simple delay function
void delay(int count) {
    for (int i = 0; i < count; i++) {
        asm volatile("nop");
    }
}

// Kernel entry point
extern "C" void kernel_main() {
    // Clear the screen
    clear_screen();

    // Initialize keyboard
    init_keyboard();

    // Initialize VShell
    vshell_init();

    // Display initial prompt
    display_prompt();

    // Main loop
    while(1) {
        // Process any keyboard input
        process_keypress();

        // Small delay to reduce CPU usage
        delay(1000);
    }
}
