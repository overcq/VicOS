#include <stdint.h>
#include <stddef.h>

// Forward declarations from kernel.cpp
void kprint(const char* str);
extern "C" void clear_screen();

// Forward declarations from filesystem.cpp (in-memory FS)
void fs_init();
bool fs_cd(const char* path);
void fs_ls(const char* path);
uint32_t fs_mkdir(const char* path);
uint32_t fs_touch(const char* path, const char* content);
const char* fs_read(const char* path);
const char* fs_pwd();

// Forward declarations from fatfs_integration.h
int fatfs_initialize();
int fatfs_mkdir(const char* path);
void fatfs_list_directory(const char* path);
int fatfs_write_file(const char* path, const char* content, size_t size);
int fatfs_read_file(const char* path, char* buffer, size_t buffer_size, size_t* bytes_read);

// Forward declaration from vnano.cpp
void process_vnano(const char* command);

// Forward declaration from fatfs_installer.cpp
void process_fatfs_install(const char* command);

// Flag to indicate if we're using FatFS or the in-memory filesystem
bool using_fatfs = false;

// Buffer for file reading with FatFS
#define MAX_FILE_BUFFER 4096
char file_read_buffer[MAX_FILE_BUFFER];

// String operations
bool str_equals(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

bool str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) {
            return false;
        }
    }
    return true;
}

// Renamed function to avoid conflict with kernel.cpp
size_t vsh_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Extract n-th argument from a command line
void get_argument(const char* command, int arg_num, char* buffer, size_t buffer_size) {
    // Skip leading spaces
    while (*command == ' ') {
        command++;
    }

    // Skip command name (first argument)
    while (*command && *command != ' ') {
        command++;
    }

    // Skip spaces between arguments
    while (*command == ' ') {
        command++;
    }

    // Find the requested argument
    for (int i = 1; i < arg_num; i++) {
        // Skip current argument
        while (*command && *command != ' ') {
            command++;
        }

        // Skip spaces to next argument
        while (*command == ' ') {
            command++;
        }
    }

    // Extract the argument
    size_t i = 0;
    while (*command && *command != ' ' && i < buffer_size - 1) {
        buffer[i++] = *command++;
    }
    buffer[i] = '\0';
}

// Print help information
void display_help() {
    kprint("VicOS Shell Commands:\n");
    kprint("  help         - Display this help information\n");
    kprint("  clear        - Clear the screen\n");
    kprint("  echo         - Display a message\n");
    kprint("  about        - Display information about VicOS\n");
    kprint("  version      - Display VicOS version\n");
    kprint("Filesystem Commands:\n");
    kprint("  pwd          - Print working directory\n");
    kprint("  ls           - List directory contents\n");
    kprint("  cd           - Change directory\n");
    kprint("  mkdir        - Create a directory\n");
    kprint("  touch        - Create or update a file\n");
    kprint("  cat          - Display file contents\n");
    kprint("  vnano        - Edit files with the VNano editor\n");
    kprint("System Commands:\n");
    kprint("  mount-fatfs  - Mount FatFS filesystem\n");
    kprint("  umount-fatfs - Unmount FatFS, switch to RAM filesystem\n");
    kprint("  perm-install - Install VicOS to a permanent storage device\n");
}

// Display information about VicOS
void display_about() {
    kprint("VicOS - A simple operating system\n");
    kprint("Created as a learning project\n");
    kprint("Features a basic VShell command interpreter\n");
    if (using_fatfs) {
        kprint("Currently using FatFS for persistent storage\n");
    } else {
        kprint("Currently using in-memory filesystem\n");
    }
    kprint("Now with permanent installation capabilities!\n");
}

// Display version information
void display_version() {
    kprint("VicOS version 0.3\n");
    kprint("VShell version 0.2\n");
    kprint("Filesystem version 0.2\n");
    kprint("VNano version 0.1\n");
    kprint("Installer version 0.2 (FatFS enabled)\n");
}

// Process echo command
void process_echo(const char* command) {
    // Skip over "echo "
    const char* message = command + 5;

    // Skip leading spaces
    while (*message == ' ') {
        message++;
    }

    // Print the message
    kprint(message);
    kprint("\n");
}

// Process cat command
void process_cat(const char* command) {
    char filename[256];
    get_argument(command, 1, filename, sizeof(filename));

    if (filename[0] == '\0') {
        kprint("Usage: cat <filename>\n");
        return;
    }

    if (using_fatfs) {
        // Use FatFS to read the file
        size_t bytes_read;
        int result = fatfs_read_file(filename, file_read_buffer, MAX_FILE_BUFFER, &bytes_read);

        if (result < 0) {
            kprint("Error: File not found or couldn't be read: ");
            kprint(filename);
            kprint("\n");
            return;
        }

        // Print file content
        kprint(file_read_buffer);

        // Add newline if content doesn't end with one
        if (bytes_read > 0 && file_read_buffer[bytes_read - 1] != '\n') {
            kprint("\n");
        }
    } else {
        // Use in-memory filesystem
        const char* content = fs_read(filename);
        if (content) {
            kprint(content);
            // Add newline if content doesn't end with one
            if (vsh_strlen(content) > 0 && content[vsh_strlen(content) - 1] != '\n') {
                kprint("\n");
            }
        } else {
            kprint("Error: File not found: ");
            kprint(filename);
            kprint("\n");
        }
    }
}

// Process touch command
void process_touch(const char* command) {
    char filename[256];
    get_argument(command, 1, filename, sizeof(filename));

    if (filename[0] == '\0') {
        kprint("Usage: touch <filename> [content]\n");
        return;
    }

    // Check if content is provided
    const char* content_ptr = command + 6; // Skip "touch "

    // Skip filename
    while (*content_ptr && *content_ptr != ' ') {
        content_ptr++;
    }

    // Skip spaces after filename
    while (*content_ptr == ' ') {
        content_ptr++;
    }

    if (using_fatfs) {
        // Use FatFS to create/update the file
        int result;
        if (*content_ptr) {
            // Content provided
            result = fatfs_write_file(filename, content_ptr, vsh_strlen(content_ptr));
        } else {
            // No content, pass empty string
            result = fatfs_write_file(filename, "", 0);
        }

        if (result < 0) {
            kprint("Error: Failed to create/update file: ");
            kprint(filename);
            kprint("\n");
            return;
        }
    } else {
        // Use in-memory filesystem
        if (*content_ptr) {
            // Content provided
            fs_touch(filename, content_ptr);
        } else {
            // No content, pass empty string
            fs_touch(filename, "");
        }
    }

    kprint("File created/updated: ");
    kprint(filename);
    kprint("\n");
}

// Process mkdir command
void process_mkdir(const char* command) {
    char dirname[256];
    get_argument(command, 1, dirname, sizeof(dirname));

    if (dirname[0] == '\0') {
        kprint("Usage: mkdir <directory>\n");
        return;
    }

    if (using_fatfs) {
        // Use FatFS to create directory
        int result = fatfs_mkdir(dirname);
        if (result < 0) {
            kprint("Error: Failed to create directory: ");
            kprint(dirname);
            kprint("\n");
            return;
        }
    } else {
        // Use in-memory filesystem
        if (fs_mkdir(dirname) == (uint32_t)-1) {
            kprint("Error: Failed to create directory: ");
            kprint(dirname);
            kprint("\n");
            return;
        }
    }

    kprint("Directory created: ");
    kprint(dirname);
    kprint("\n");
}

// Process ls command
void process_ls(const char* command) {
    char path[256];
    get_argument(command, 1, path, sizeof(path));

    // If no path provided, use current directory
    if (path[0] == '\0') {
        if (using_fatfs) {
            fatfs_list_directory(".");
        } else {
            fs_ls(".");
        }
    } else {
        if (using_fatfs) {
            fatfs_list_directory(path);
        } else {
            fs_ls(path);
        }
    }
}

// Process cd command
void process_cd(const char* command) {
    char path[256];
    get_argument(command, 1, path, sizeof(path));

    if (using_fatfs) {
        // FatFS doesn't track current directory in the same way
        // We would need to implement this separately
        kprint("Note: When using FatFS, only absolute paths are fully supported.\n");
    }

    if (path[0] == '\0') {
        // No argument, change to root
        if (!using_fatfs) {
            fs_cd("/");
        }
    } else {
        if (!using_fatfs) {
            fs_cd(path);
        }
    }
}

// Process pwd command
void process_pwd(const char* /* command */) {
    if (using_fatfs) {
        // FatFS doesn't track current directory in the same way
        kprint("Current directory: / (root)\n");
    } else {
        kprint("Current directory: ");
        kprint(fs_pwd());
        kprint("\n");
    }
}

// Process mounting FatFS
void process_mount_fatfs(const char* /* command */) {
    if (using_fatfs) {
        kprint("FatFS is already mounted.\n");
        return;
    }

    kprint("Attempting to mount FatFS...\n");
    int result = fatfs_initialize();

    if (result < 0) {
        kprint("Failed to mount FatFS. Continuing with in-memory filesystem.\n");
        return;
    }

    using_fatfs = true;
    kprint("FatFS mounted successfully. Now using persistent storage.\n");
}

// Process unmounting FatFS
void process_umount_fatfs(const char* /* command */) {
    if (!using_fatfs) {
        kprint("FatFS is not currently mounted.\n");
        return;
    }

    // In a more complete implementation, we would call a function to unmount FatFS
    using_fatfs = false;
    kprint("Switched back to in-memory filesystem.\n");
}

// Initialize VShell
void vshell_init() {
    // Display welcome message
    kprint("Welcome to VicOS! You have now entered VShell.\n");
    kprint("Type 'help' for available commands.\n");

    // Initialize in-memory filesystem
    fs_init();

    // By default, start with in-memory filesystem
    using_fatfs = false;
}

// Execute a command
void vshell_execute_command(const char* command) {
    // Skip leading spaces
    while (*command == ' ') {
        command++;
    }

    // Empty command
    if (*command == '\0') {
        return;
    }

    // Process commands
    if (str_equals(command, "help")) {
        display_help();
    }
    else if (str_equals(command, "clear")) {
        clear_screen();
    }
    else if (str_equals(command, "about")) {
        display_about();
    }
    else if (str_equals(command, "version")) {
        display_version();
    }
    else if (str_starts_with(command, "echo ")) {
        process_echo(command);
    }
    else if (str_equals(command, "pwd")) {
        process_pwd(command);
    }
    else if (str_starts_with(command, "cd")) {
        process_cd(command);
    }
    else if (str_starts_with(command, "ls")) {
        process_ls(command);
    }
    else if (str_starts_with(command, "mkdir ")) {
        process_mkdir(command);
    }
    else if (str_starts_with(command, "touch ")) {
        process_touch(command);
    }
    else if (str_starts_with(command, "cat ")) {
        process_cat(command);
    }
    else if (str_starts_with(command, "vnano ")) {
        process_vnano(command);
    }
    else if (str_equals(command, "perm-install")) {
        process_fatfs_install(command);
    }
    else if (str_equals(command, "mount-fatfs")) {
        process_mount_fatfs(command);
    }
    else if (str_equals(command, "umount-fatfs")) {
        process_umount_fatfs(command);
    }
    else {
        kprint("Unknown command: ");
        kprint(command);
        kprint("\nType 'help' for available commands.\n");
    }
}
