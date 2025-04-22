#include <stdint.h>
#include "vstdint.h"
#include <stddef.h>

// Forward declarations
void kprint(const char* str);

// USB PCI detection - these are simplified for demonstration
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// USB Device Class Codes
#define USB_CLASS_MASS_STORAGE 0x08

// I/O port functions for PCI
static inline void outl(vic_uint16 port, vic_uint32 val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline vic_uint32 inl(vic_uint16 port) {
    vic_uint32 ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Structure for USB device information
struct USBDevice {
    vic_uint16 vendor_id;
    vic_uint16 device_id;
    vic_uint8 class_code;
    vic_uint8 subclass_code;
    vic_uint8 protocol;
    bool is_mass_storage;
    char vendor_name[32];
    char device_name[32];
};

// Known USB vendor names (simplified mapping)
const char* get_vendor_name(vic_uint16 vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel";
        case 0x1022: return "AMD";
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "ATI";
        case 0x0B05: return "ASUS";
        case 0x0781: return "SanDisk";
        case 0x13FE: return "Kingston";
        case 0x1058: return "Western Digital";
        case 0x0930: return "Toshiba";
        case 0x125F: return "A-DATA";
        case 0x054C: return "Sony";
        case 0x046D: return "Logitech";
        case 0x1005: return "Acer";
        case 0x04E8: return "Samsung";
        case 0x18A5: return "Verbatim";
        case 0x1F75: return "Innostor";
        default: return "Unknown Vendor";
    }
}

// Read PCI configuration space
vic_uint32 pci_read_config(vic_uint8 bus, vic_uint8 device, vic_uint8 func, vic_uint8 offset) {
    vic_uint32 address = 0x80000000 | (bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// Check if a PCI device exists
bool pci_device_exists(vic_uint8 bus, vic_uint8 device, vic_uint8 func) {
    vic_uint32 vendor = pci_read_config(bus, device, func, 0) & 0xFFFF;
    return vendor != 0xFFFF;
}

// Get the class code of a PCI device
vic_uint8 pci_get_class(vic_uint8 bus, vic_uint8 device, vic_uint8 func) {
    vic_uint32 class_reg = pci_read_config(bus, device, func, 0x08);
    return (class_reg >> 24) & 0xFF;
}

// Get the subclass code of a PCI device
vic_uint8 pci_get_subclass(vic_uint8 bus, vic_uint8 device, vic_uint8 func) {
    vic_uint32 class_reg = pci_read_config(bus, device, func, 0x08);
    return (class_reg >> 16) & 0xFF;
}

// Get the vendor ID of a PCI device
vic_uint16 pci_get_vendor(vic_uint8 bus, vic_uint8 device, vic_uint8 func) {
    vic_uint32 vendor_dev = pci_read_config(bus, device, func, 0x00);
    return vendor_dev & 0xFFFF;
}

// Get the device ID of a PCI device
vic_uint16 pci_get_device_id(vic_uint8 bus, vic_uint8 device, vic_uint8 func) {
    vic_uint32 vendor_dev = pci_read_config(bus, device, func, 0x00);
    return (vendor_dev >> 16) & 0xFFFF;
}

// Find USB controllers and devices
int detect_usb_devices(USBDevice* devices, int max_devices) {
    int count = 0;

    kprint("Scanning for USB controllers...\n");

    // Loop through all PCI devices looking for USB controllers
    for (vic_uint16 bus = 0; bus < 256; bus++) {
        for (vic_uint8 device = 0; device < 32; device++) {
            for (vic_uint8 func = 0; func < 8; func++) {
                if (!pci_device_exists(bus, device, func)) {
                    continue;
                }

                vic_uint8 class_code = pci_get_class(bus, device, func);
                vic_uint8 subclass = pci_get_subclass(bus, device, func);

                // Check if this is a USB controller (class 0x0C, subclass 0x03)
                if (class_code == 0x0C && subclass == 0x03) {
                    vic_uint16 vendor_id = pci_get_vendor(bus, device, func);
                    vic_uint16 device_id = pci_get_device_id(bus, device, func);

                    kprint("Found USB controller: ");
                    kprint(get_vendor_name(vendor_id));
                    kprint("\n");

                    // In a real implementation, this is where we would scan the USB bus
                    // to find connected devices. For this demo, we'll simulate finding
                    // a mass storage device connected to each controller.

                    if (count < max_devices) {
                        USBDevice* usb_dev = &devices[count++];
                        usb_dev->vendor_id = vendor_id;
                        usb_dev->device_id = device_id;
                        usb_dev->class_code = USB_CLASS_MASS_STORAGE;
                        usb_dev->is_mass_storage = true;

                        // Set vendor name
                        const char* vendor_name = get_vendor_name(vendor_id);
                        int i;
                        for (i = 0; vendor_name[i] != '\0'; i++) {
                            usb_dev->vendor_name[i] = vendor_name[i];
                        }
                        usb_dev->vendor_name[i] = '\0';

                        // Set device name
                        const char* device_name = "USB Mass Storage Device";
                        for (i = 0; device_name[i] != '\0'; i++) {
                            usb_dev->device_name[i] = device_name[i];
                        }
                        usb_dev->device_name[i] = '\0';
                    }
                }
            }
        }
    }

    return count;
}

// Scan for USB storage devices
int scan_usb_storage(USBDevice* usb_devices, int max_devices) {
    kprint("Scanning for USB storage devices...\n");

    int num_devices = detect_usb_devices(usb_devices, max_devices);

    int storage_count = 0;
    for (int i = 0; i < num_devices; i++) {
        if (usb_devices[i].is_mass_storage) {
            storage_count++;
        }
    }

    return storage_count;
}
