#include <stdint.h>
#include <stddef.h>

// Forward declarations
void kprint(const char* str);

// Maximum filename length
#define FS_MAX_FILENAME 32

// Maximum path length
#define FS_MAX_PATH 256

// Maximum number of files in the filesystem
#define FS_MAX_FILES 64

// Maximum number of directories in the filesystem
#define FS_MAX_DIRS 32

// Maximum file size
#define FS_MAX_FILE_SIZE 4096

// File types
#define FS_TYPE_FILE 1
#define FS_TYPE_DIRECTORY 2

// Current working directory path
char current_path[FS_MAX_PATH] = "/";

// File/directory structure
struct FSNode {
    char name[FS_MAX_FILENAME];
    uint8_t type;
    uint32_t size;
    uint32_t parent_index; // Index of parent directory
    bool used;             // Whether this entry is in use

    // For files, this holds the content
    // For directories, this is unused
    char content[FS_MAX_FILE_SIZE];
};

// Root directory (index 0 is always root)
FSNode filesystem[FS_MAX_FILES + FS_MAX_DIRS];

// Current directory index
uint32_t current_dir = 0;

// Forward declarations of functions to resolve circular dependencies
uint32_t fs_mkdir(const char* path);
uint32_t fs_touch(const char* path, const char* content);

// String operations
bool fs_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

void fs_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

size_t fs_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void fs_strcat(char* dest, const char* src) {
    while (*dest) {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Find a node by path
// Returns the index of the node, or -1 if not found
int fs_find_node(const char* path) {
    // Handle empty path
    if (!path || !*path) {
        return -1;
    }

    // Handle absolute vs relative path
    uint32_t start_dir;
    const char* path_ptr = path;

    if (path[0] == '/') {
        // Absolute path, start from root
        start_dir = 0;
        path_ptr++; // Skip the leading /
    } else {
        // Relative path, start from current directory
        start_dir = current_dir;
    }

    // Handle empty path after processing (root directory)
    if (!*path_ptr) {
        return 0; // Root directory
    }

    // Handle "." (current directory)
    if (fs_strcmp(path_ptr, ".")) {
        return current_dir;
    }

    // Handle ".." (parent directory)
    if (fs_strcmp(path_ptr, "..")) {
        return filesystem[current_dir].parent_index;
    }

    // Parse path components
    char component[FS_MAX_FILENAME];
    uint32_t current_index = start_dir;

    while (*path_ptr) {
        // Extract next component
        int i = 0;
        while (*path_ptr && *path_ptr != '/' && i < FS_MAX_FILENAME - 1) {
            component[i++] = *path_ptr++;
        }
        component[i] = '\0';

        // Skip any trailing slash
        if (*path_ptr == '/') {
            path_ptr++;
        }

        // Handle empty component (// in path)
        if (component[0] == '\0') {
            continue;
        }

        // Handle "." (current directory)
        if (fs_strcmp(component, ".")) {
            continue;
        }

        // Handle ".." (parent directory)
        if (fs_strcmp(component, "..")) {
            current_index = filesystem[current_index].parent_index;
            continue;
        }

        // Search for the component in the current directory
        bool found = false;
        for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
            if (filesystem[i].used &&
                filesystem[i].parent_index == current_index &&
                fs_strcmp(filesystem[i].name, component)) {

                current_index = i;
            found = true;
            break;
                }
        }

        if (!found) {
            return -1; // Component not found
        }
    }

    return current_index;
}

// Get the full path of a node
void fs_get_path(uint32_t node_index, char* path_buffer) {
    if (node_index == 0) {
        // Root directory
        path_buffer[0] = '/';
        path_buffer[1] = '\0';
        return;
    }

    // Build path by traversing up to root
    path_buffer[0] = '\0';
    char temp[FS_MAX_PATH];
    uint32_t current = node_index;

    while (current != 0) {
        // Add current node name
        fs_strcpy(temp, path_buffer);
        path_buffer[0] = '/';
        fs_strcpy(path_buffer + 1, filesystem[current].name);
        fs_strcat(path_buffer, temp);

        // Move up to parent
        current = filesystem[current].parent_index;
    }

    // If path is empty, it's root
    if (path_buffer[0] == '\0') {
        path_buffer[0] = '/';
        path_buffer[1] = '\0';
    }
}

// Initialize the filesystem
void fs_init() {
    // Initialize all entries as unused
    for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
        filesystem[i].used = false;
    }

    // Set up the root directory
    fs_strcpy(filesystem[0].name, "/");
    filesystem[0].type = FS_TYPE_DIRECTORY;
    filesystem[0].size = 0;
    filesystem[0].parent_index = 0; // Root is its own parent
    filesystem[0].used = true;

    // Create a few default directories
    fs_mkdir("/bin");
    fs_mkdir("/home");
    fs_mkdir("/etc");

    // Create some default files
    fs_touch("/etc/motd", "Welcome to VicOS Filesystem!\n");
    fs_touch("/home/readme.txt", "This is your home directory.\n");

    kprint("Filesystem initialized.\n");
}

// Change directory
bool fs_cd(const char* path) {
    int node_index = fs_find_node(path);

    if (node_index == -1) {
        kprint("Error: Directory not found: ");
        kprint(path);
        kprint("\n");
        return false;
    }

    if (filesystem[node_index].type != FS_TYPE_DIRECTORY) {
        kprint("Error: Not a directory: ");
        kprint(path);
        kprint("\n");
        return false;
    }

    // Update current directory
    current_dir = node_index;

    // Update current path
    fs_get_path(current_dir, current_path);

    return true;
}

// List directory contents
void fs_ls(const char* path) {
    // If path is empty or null, use current directory
    if (!path || !*path) {
        path = ".";
    }

    int dir_index = fs_find_node(path);

    if (dir_index == -1) {
        kprint("Error: Directory not found: ");
        kprint(path);
        kprint("\n");
        return;
    }

    if (filesystem[dir_index].type != FS_TYPE_DIRECTORY) {
        kprint("Error: Not a directory: ");
        kprint(path);
        kprint("\n");
        return;
    }

    // Print directory contents
    kprint("Contents of ");
    char path_buffer[FS_MAX_PATH];
    fs_get_path(dir_index, path_buffer);
    kprint(path_buffer);
    kprint(":\n");

    bool found = false;

    // First, list directories
    for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
        if (filesystem[i].used &&
            filesystem[i].parent_index == (uint32_t)dir_index &&
            filesystem[i].type == FS_TYPE_DIRECTORY) {

            found = true;
        kprint("[DIR] ");
        kprint(filesystem[i].name);
        kprint("\n");
            }
    }

    // Then, list files
    for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
        if (filesystem[i].used &&
            filesystem[i].parent_index == (uint32_t)dir_index &&
            filesystem[i].type == FS_TYPE_FILE) {

            found = true;
        kprint("[FILE] ");
        kprint(filesystem[i].name);
        kprint(" (");

        // Convert size to string (simple implementation)
        char size_str[16];
        uint32_t size = filesystem[i].size;
        if (size == 0) {
            size_str[0] = '0';
            size_str[1] = '\0';
        } else {
            int idx = 0;
            while (size > 0 && idx < 15) {
                size_str[idx++] = '0' + (size % 10);
                size /= 10;
            }
            size_str[idx] = '\0';

            // Reverse the string
            for (int j = 0; j < idx / 2; j++) {
                char temp = size_str[j];
                size_str[j] = size_str[idx - j - 1];
                size_str[idx - j - 1] = temp;
            }
        }

        kprint(size_str);
        kprint(" bytes)\n");
            }
    }

    if (!found) {
        kprint("  [Empty directory]\n");
    }
}

// Create a directory
uint32_t fs_mkdir(const char* path) {
    // Parse path to get parent directory and new directory name
    char parent_path[FS_MAX_PATH];
    char dir_name[FS_MAX_FILENAME];

    // Find the last slash in the path
    const char* last_slash = path;
    const char* ptr = path;

    while (*ptr) {
        if (*ptr == '/') {
            last_slash = ptr;
        }
        ptr++;
    }

    // Extract parent path and directory name
    if (last_slash == path) {
        // Creating a directory in root
        parent_path[0] = '/';
        parent_path[1] = '\0';
        fs_strcpy(dir_name, last_slash + 1);
    } else {
        // Copy parent path
        size_t parent_len = last_slash - path;
        for (size_t i = 0; i < parent_len; i++) {
            parent_path[i] = path[i];
        }
        parent_path[parent_len] = '\0';

        // Handle empty parent path (creating in root)
        if (parent_path[0] == '\0') {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        }

        // Copy directory name
        fs_strcpy(dir_name, last_slash + 1);
    }

    // Check if the directory name is empty
    if (dir_name[0] == '\0') {
        kprint("Error: Directory name cannot be empty\n");
        return (uint32_t)-1;
    }

    // Check if parent directory exists
    int parent_index = fs_find_node(parent_path);
    if (parent_index == -1) {
        kprint("Error: Parent directory not found: ");
        kprint(parent_path);
        kprint("\n");
        return (uint32_t)-1;
    }

    if (filesystem[parent_index].type != FS_TYPE_DIRECTORY) {
        kprint("Error: Parent is not a directory: ");
        kprint(parent_path);
        kprint("\n");
        return (uint32_t)-1;
    }

    // Check if directory already exists
    char full_path[FS_MAX_PATH];
    fs_strcpy(full_path, parent_path);
    if (full_path[fs_strlen(full_path) - 1] != '/') {
        fs_strcat(full_path, "/");
    }
    fs_strcat(full_path, dir_name);

    if (fs_find_node(full_path) != -1) {
        kprint("Error: Directory already exists: ");
        kprint(full_path);
        kprint("\n");
        return (uint32_t)-1;
    }

    // Find free slot for the new directory
    uint32_t dir_index = (uint32_t)-1;
    for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
        if (!filesystem[i].used) {
            dir_index = i;
            break;
        }
    }

    if (dir_index == (uint32_t)-1) {
        kprint("Error: Filesystem full, cannot create directory\n");
        return (uint32_t)-1;
    }

    // Create the directory
    fs_strcpy(filesystem[dir_index].name, dir_name);
    filesystem[dir_index].type = FS_TYPE_DIRECTORY;
    filesystem[dir_index].size = 0;
    filesystem[dir_index].parent_index = parent_index;
    filesystem[dir_index].used = true;

    return dir_index;
}

// Create or update a file
uint32_t fs_touch(const char* path, const char* content) {
    // Parse path to get parent directory and filename
    char parent_path[FS_MAX_PATH];
    char file_name[FS_MAX_FILENAME];

    // Find the last slash in the path
    const char* last_slash = path;
    const char* ptr = path;

    while (*ptr) {
        if (*ptr == '/') {
            last_slash = ptr;
        }
        ptr++;
    }

    // Extract parent path and file name
    if (last_slash == path) {
        // Creating a file in root
        parent_path[0] = '/';
        parent_path[1] = '\0';
        fs_strcpy(file_name, last_slash + 1);
    } else {
        // Copy parent path
        size_t parent_len = last_slash - path;
        for (size_t i = 0; i < parent_len; i++) {
            parent_path[i] = path[i];
        }
        parent_path[parent_len] = '\0';

        // Handle empty parent path (creating in root)
        if (parent_path[0] == '\0') {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        }

        // Copy file name
        fs_strcpy(file_name, last_slash + 1);
    }

    // Check if the file name is empty
    if (file_name[0] == '\0') {
        kprint("Error: File name cannot be empty\n");
        return (uint32_t)-1;
    }

    // Check if parent directory exists
    int parent_index = fs_find_node(parent_path);
    if (parent_index == -1) {
        kprint("Error: Parent directory not found: ");
        kprint(parent_path);
        kprint("\n");
        return (uint32_t)-1;
    }

    if (filesystem[parent_index].type != FS_TYPE_DIRECTORY) {
        kprint("Error: Parent is not a directory: ");
        kprint(parent_path);
        kprint("\n");
        return (uint32_t)-1;
    }

    // Check if file already exists
    char full_path[FS_MAX_PATH];
    fs_strcpy(full_path, parent_path);
    if (full_path[fs_strlen(full_path) - 1] != '/') {
        fs_strcat(full_path, "/");
    }
    fs_strcat(full_path, file_name);

    int file_index = fs_find_node(full_path);

    if (file_index != -1) {
        // File exists, check if it's a file
        if (filesystem[file_index].type != FS_TYPE_FILE) {
            kprint("Error: Path exists but is not a file: ");
            kprint(full_path);
            kprint("\n");
            return (uint32_t)-1;
        }

        // Update file content if provided
        if (content) {
            size_t content_len = fs_strlen(content);
            if (content_len >= FS_MAX_FILE_SIZE) {
                kprint("Error: Content too large for file\n");
                return (uint32_t)-1;
            }

            fs_strcpy(filesystem[file_index].content, content);
            filesystem[file_index].size = content_len;
        }

        return file_index;
    }

    // Find free slot for the new file
    uint32_t new_file_index = (uint32_t)-1;
    for (uint32_t i = 0; i < FS_MAX_FILES + FS_MAX_DIRS; i++) {
        if (!filesystem[i].used) {
            new_file_index = i;
            break;
        }
    }

    if (new_file_index == (uint32_t)-1) {
        kprint("Error: Filesystem full, cannot create file\n");
        return (uint32_t)-1;
    }

    // Create the file
    fs_strcpy(filesystem[new_file_index].name, file_name);
    filesystem[new_file_index].type = FS_TYPE_FILE;
    filesystem[new_file_index].parent_index = parent_index;
    filesystem[new_file_index].used = true;

    // Set content if provided
    if (content) {
        size_t content_len = fs_strlen(content);
        if (content_len >= FS_MAX_FILE_SIZE) {
            kprint("Error: Content too large for file\n");
            filesystem[new_file_index].used = false;
            return (uint32_t)-1;
        }

        fs_strcpy(filesystem[new_file_index].content, content);
        filesystem[new_file_index].size = content_len;
    } else {
        // Empty file
        filesystem[new_file_index].content[0] = '\0';
        filesystem[new_file_index].size = 0;
    }

    return new_file_index;
}

// Read file content
const char* fs_read(const char* path) {
    int file_index = fs_find_node(path);

    if (file_index == -1) {
        kprint("Error: File not found: ");
        kprint(path);
        kprint("\n");
        return nullptr;
    }

    if (filesystem[file_index].type != FS_TYPE_FILE) {
        kprint("Error: Not a file: ");
        kprint(path);
        kprint("\n");
        return nullptr;
    }

    return filesystem[file_index].content;
}

// Get current directory path
const char* fs_pwd() {
    return current_path;
}
