// src/string_utils.c
#include <stdarg.h>
#include <stddef.h>

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    size_t count = 0;
    
    while (*format && count < size - 1) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 's': {  // String
                    char* s = va_arg(args, char*);
                    while (*s && count < size - 1) {
                        str[count++] = *s++;
                    }
                    break;
                }
                case 'd': {  // Decimal
                    int num = va_arg(args, int);
                    if (num < 0) {
                        str[count++] = '-';
                        num = -num;
                    }
                    
                    // Convert number to string
                    char temp[20];
                    int idx = 0;
                    
                    if (num == 0) {
                        temp[idx++] = '0';
                    } else {
                        while (num > 0 && idx < 20) {
                            temp[idx++] = '0' + (num % 10);
                            num /= 10;
                        }
                    }
                    
                    // Copy in reverse order
                    while (idx > 0 && count < size - 1) {
                        str[count++] = temp[--idx];
                    }
                    break;
                }
                case 'x': {  // Hexadecimal
                    unsigned int num = va_arg(args, unsigned int);
                    
                    // Convert number to string
                    char temp[20];
                    int idx = 0;
                    
                    if (num == 0) {
                        temp[idx++] = '0';
                    } else {
                        while (num > 0 && idx < 20) {
                            int digit = num % 16;
                            if (digit < 10)
                                temp[idx++] = '0' + digit;
                            else
                                temp[idx++] = 'a' + (digit - 10);
                            num /= 16;
                        }
                    }
                    
                    // Copy in reverse order
                    while (idx > 0 && count < size - 1) {
                        str[count++] = temp[--idx];
                    }
                    break;
                }
                case 'c': {  // Character
                    char c = (char)va_arg(args, int);
                    str[count++] = c;
                    break;
                }
                case 'u': {  // Unsigned decimal
                    unsigned int num = va_arg(args, unsigned int);
                    
                    // Convert number to string
                    char temp[20];
                    int idx = 0;
                    
                    if (num == 0) {
                        temp[idx++] = '0';
                    } else {
                        while (num > 0 && idx < 20) {
                            temp[idx++] = '0' + (num % 10);
                            num /= 10;
                        }
                    }
                    
                    // Copy in reverse order
                    while (idx > 0 && count < size - 1) {
                        str[count++] = temp[--idx];
                    }
                    break;
                }
                case 'l': {  // Long specifier, we only handle 'lu' for now
                    format++;
                    if (*format == 'u') {
                        unsigned long num = va_arg(args, unsigned long);
                        
                        // Convert number to string
                        char temp[20];
                        int idx = 0;
                        
                        if (num == 0) {
                            temp[idx++] = '0';
                        } else {
                            while (num > 0 && idx < 20) {
                                temp[idx++] = '0' + (num % 10);
                                num /= 10;
                            }
                        }
                        
                        // Copy in reverse order
                        while (idx > 0 && count < size - 1) {
                            str[count++] = temp[--idx];
                        }
                    }
                    break;
                }
                case '%':  // Percent sign
                    str[count++] = '%';
                    break;
                default:  // Unknown format specifier
                    str[count++] = '%';
                    str[count++] = *format;
                    break;
            }
        } else {
            str[count++] = *format;
        }
        format++;
    }
    
    // Null-terminate the string
    str[count] = '\0';
    
    va_end(args);
    return count;
}