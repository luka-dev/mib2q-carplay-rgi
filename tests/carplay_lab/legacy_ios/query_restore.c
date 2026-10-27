/* Read-only probe. Explicitly targets the disposable virtual iPhone only. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/restore.h>
#include <plist/plist.h>

int main(void)
{
    const char *socket = getenv("USBMUXD_SOCKET_ADDRESS");
    if (!socket || strncmp(socket, "127.0.0.1:", 10) != 0) {
        fputs("Set USBMUXD_SOCKET_ADDRESS to the loopback virtual-device tunnel.\n", stderr);
        return 2;
    }
    idevice_t device = NULL;
    restored_client_t client = NULL;
    char *type = NULL;
    uint64_t version = 0;
    int result = 1;
    if (idevice_new(&device, "00008030-1122334455667788") != IDEVICE_E_SUCCESS)
        return 1;
    if (restored_client_new(device, &client, "mhi2-offline-ios-vm") != RESTORE_E_SUCCESS)
        goto done;
    if (restored_query_type(client, &type, &version) != RESTORE_E_SUCCESS)
        goto done;
    printf("restore_type=%s protocol=%" PRIu64 "\n", type, version);
    const char *keys[] = {"HardwareInfo", "SavedDebugInfo", "SerialNumber"};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        plist_t value = NULL;
        if (restored_query_value(client, keys[i], &value) == RESTORE_E_SUCCESS && value) {
            char *xml = NULL;
            uint32_t length = 0;
            if (plist_to_xml(value, &xml, &length) == PLIST_ERR_SUCCESS && xml) {
                printf("%s=%.*s\n", keys[i], (int)length, xml);
                free(xml);
            }
            plist_free(value);
        }
    }
    result = 0;
done:
    free(type);
    if (client) restored_client_free(client);
    if (device) idevice_free(device);
    return result;
}
