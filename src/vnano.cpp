#include <stdint.h>
#include <stddef.h>

// Forward declarations from kernel.cpp
void kprint(const char* str);
void kputchar(char c);
extern "C" void clear_screen();

// Forward declarations for filesystem
const char* fs_read(const char* path);
uint32_t fs_touch(const char* path, const char* content);

// Editor state
#define VNANO_MAX_BUFFER_SIZE 4096
char editor_buffer[VNANO_MAX_BUFFER_SIZE];
size_t editor_length = 0;
size_t editor_cursor = 0;
size_t editor_row = 0;
size_t editor_col = 0;
size_t editor_scroll = 0;
char editor_filename[256];
bool editor_modified = false;

// Screen dimensions - defined here rather than trying to use from kernel
#define VNANO_SCREEN_WIDTH 80
#define VNANO_SCREEN_HEIGHT 25

// Find length of a string
size_t vnano_strlen(const char* str) {
    size_t i = 0;
    while (str[i] != '\0') i++;
    return i;
}

// Copy a string
void vnano_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Initialize the editor
void vnano_init(const char* filename) {
    // Clear editor state
    editor_length = 0;
    editor_cursor = 0;
    editor_row = 0;
    editor_col = 0;
    editor_scroll = 0;
    editor_modified = false;

    // Store filename
    vnano_strcpy(editor_filename, filename);

    // Load file content if it exists
    const char* content = fs_read(filename);
    if (content) {
        // Copy file content to editor buffer
        size_t content_len = vnano_strlen(content);
        if (content_len >= VNANO_MAX_BUFFER_SIZE) {
            content_len = VNANO_MAX_BUFFER_SIZE - 1;
        }

        for (size_t i = 0; i < content_len; i++) {
            editor_buffer[i] = content[i];
        }

        editor_buffer[content_len] = '\0';
        editor_length = content_len;
    } else {
        // New file
        editor_buffer[0] = '\0';
    }
}

// Draw status bar
void vnano_draw_status() {
    // Move to second-last line
    for (int i = 0; i < VNANO_SCREEN_WIDTH; i++) {
        kputchar(' ');
    }

    // Go back to beginning of line
    editor_row = VNANO_SCREEN_HEIGHT - 2;
    editor_col = 0;

    // Display status bar
    kprint("VNano: ");
    kprint(editor_filename);
    if (editor_modified) {
        kprint(" [Modified]");
    }

    // Move to last line
    kputchar('\n');

    // Display help bar
    kprint("^X Exit | ^S Save | ^G Help");
}

// Calculate row and column from cursor position
void vnano_update_cursor_position() {
    editor_row = 2; // Start after the header
    editor_col = 0;

    for (size_t i = 0; i < editor_cursor; i++) {
        if (editor_buffer[i] == '\n') {
            editor_row++;
            editor_col = 0;
        } else {
            editor_col++;
            if (editor_col >= VNANO_SCREEN_WIDTH) {
                editor_row++;
                editor_col = 0;
            }
        }
    }
}

// Refresh the editor display
void vnano_refresh() {
    clear_screen();

    // Display header
    kprint("  VNano Editor - ");
    kprint(editor_filename);
    kprint("\n\n");

    // Display file content
    size_t row = 2; // Start after the header
    size_t col = 0;

    for (size_t i = 0; i < editor_length; i++) {
        if (editor_buffer[i] == '\n') {
            kputchar('\n');
            row++;
            col = 0;
        } else {
            kputchar(editor_buffer[i]);
            col++;
            if (col >= VNANO_SCREEN_WIDTH) {
                kputchar('\n');
                row++;
                col = 0;
            }
        }

        // Stop if we reach the bottom of the screen
        if (row >= VNANO_SCREEN_HEIGHT - 2) {
            break;
        }
    }

    // Fill remaining screen with empty lines
    while (row < VNANO_SCREEN_HEIGHT - 2) {
        kputchar('\n');
        row++;
    }

    // Draw status bar
    vnano_draw_status();

    // Update cursor position
    vnano_update_cursor_position();
}

// Insert a character at cursor
void vnano_insert_char(char c) {
    if (editor_length >= VNANO_MAX_BUFFER_SIZE - 1) {
        return; // Buffer full
    }

    // Shift characters after cursor
    for (size_t i = editor_length; i > editor_cursor; i--) {
        editor_buffer[i] = editor_buffer[i - 1];
    }

    // Insert the character
    editor_buffer[editor_cursor] = c;
    editor_cursor++;
    editor_length++;
    editor_buffer[editor_length] = '\0';
    editor_modified = true;

    vnano_refresh();
}

// Delete the character before cursor
void vnano_backspace() {
    if (editor_cursor == 0) {
        return; // At beginning of file
    }

    // Shift characters after cursor
    for (size_t i = editor_cursor - 1; i < editor_length; i++) {
        editor_buffer[i] = editor_buffer[i + 1];
    }

    editor_cursor--;
    editor_length--;
    editor_modified = true;

    vnano_refresh();
}

// Process cursor movement
void vnano_move_cursor(int direction) {
    switch (direction) {
        case 0: // Left
            if (editor_cursor > 0) {
                editor_cursor--;
            }
            break;
        case 1: // Right
            if (editor_cursor < editor_length) {
                editor_cursor++;
            }
            break;
        case 2: // Up
        {
            // Find start of current line
            size_t line_start = editor_cursor;
            while (line_start > 0 && editor_buffer[line_start - 1] != '\n') {
                line_start--;
            }

            // Find start of previous line
            size_t prev_line_start = line_start;
            if (prev_line_start > 0) {
                prev_line_start--; // Skip the newline
                while (prev_line_start > 0 && editor_buffer[prev_line_start - 1] != '\n') {
                    prev_line_start--;
                }
            }

            // Calculate column position
            size_t current_col = editor_cursor - line_start;

            // Move to same column in previous line
            size_t prev_line_length = line_start - prev_line_start - 1;
            if (prev_line_start < line_start) { // If we found a previous line
                if (current_col <= prev_line_length) {
                    editor_cursor = prev_line_start + current_col;
                } else {
                    editor_cursor = prev_line_start + prev_line_length;
                }
            }
        }
        break;
        case 3: // Down
        {
            // Find start of current line
            size_t line_start = editor_cursor;
            while (line_start > 0 && editor_buffer[line_start - 1] != '\n') {
                line_start--;
            }

            // Find end of current line
            size_t line_end = editor_cursor;
            while (line_end < editor_length && editor_buffer[line_end] != '\n') {
                line_end++;
            }

            // Find end of next line
            size_t next_line_end = line_end;
            if (next_line_end < editor_length) {
                next_line_end++; // Skip the newline
                while (next_line_end < editor_length && editor_buffer[next_line_end] != '\n') {
                    next_line_end++;
                }
            }

            // Calculate column position
            size_t current_col = editor_cursor - line_start;

            // Move to same column in next line
            if (line_end < editor_length) { // If we found a next line
                size_t next_line_start = line_end + 1;
                size_t next_line_length = next_line_end - next_line_start;

                if (current_col <= next_line_length) {
                    editor_cursor = next_line_start + current_col;
                } else {
                    editor_cursor = next_line_start + next_line_length;
                }
            }
        }
        break;
    }

    vnano_refresh();
}

// Save file
void vnano_save() {
    fs_touch(editor_filename, editor_buffer);
    editor_modified = false;
    vnano_refresh();
}

// Display help screen
void vnano_help() {
    clear_screen();

    kprint("VNano Editor Help\n");
    kprint("----------------\n\n");
    kprint("Keyboard controls:\n");
    kprint("  Ctrl+X     Exit editor\n");
    kprint("  Ctrl+S     Save file\n");
    kprint("  Ctrl+G     Display this help screen\n");
    kprint("  Arrow keys Move cursor\n");
    kprint("  Backspace  Delete character before cursor\n\n");
    kprint("Press any key to return to editor...");

    // Wait for a key (this would be implemented in actual code)
    // For now, we'll just return immediately
    vnano_refresh();
}

// Process keyboard input for the editor
void vnano_process_keypress(char c, bool ctrl_pressed) {
    if (ctrl_pressed) {
        switch (c) {
            case 'x': // Ctrl+X - Exit
            case 'X':
                // In a real implementation, we would ask to save if modified
                return;
            case 's': // Ctrl+S - Save
            case 'S':
                vnano_save();
                break;
            case 'g': // Ctrl+G - Help
            case 'G':
                vnano_help();
                break;
        }
    } else {
        switch (c) {
            case '\b': // Backspace
                vnano_backspace();
                break;
            case '\n': // Enter
                vnano_insert_char('\n');
                break;
            default:
                if (c >= 32 && c <= 126) { // Printable ASCII
                    vnano_insert_char(c);
                }
                break;
        }
    }
}

// Main entry point for the editor
void vnano_edit(const char* filename) {
    // Initialize editor with the file
    vnano_init(filename);

    // Refresh display
    vnano_refresh();

    // In real implementation, we would have a loop here to process keypresses
    // but for our demo, the editor will be controlled by the shell command handler
}

// This function would be called from vshellhandler.cpp
void process_vnano(const char* command) {
    char filename[256];

    // Extract filename from command
    const char* cmd_ptr = command + 6; // Skip "vnano "

    // Skip spaces
    while (*cmd_ptr == ' ') {
        cmd_ptr++;
    }

    // Copy filename
    size_t i = 0;
    while (*cmd_ptr && *cmd_ptr != ' ' && i < 255) {
        filename[i++] = *cmd_ptr++;
    }
    filename[i] = '\0';

    if (filename[0] == '\0') {
        kprint("Usage: vnano <filename>\n");
        return;
    }

    // Start editor
    vnano_edit(filename);
}
