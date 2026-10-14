/* Fetch the synthetic pixel-probe outputs or the native display capture.
 * Build with libimobiledevice-1.0; run from the repository root.
 */
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int copy_probe(afc_client_t client, const char *name, size_t limit)
{
    char remote[96], local[256];
    snprintf(remote, sizeof(remote), "/%s", name);
    snprintf(local, sizeof(local), "build/ios-vm-lab/combined-usb/%s", name);
    uint64_t handle = 0;
    afc_error_t opened = afc_file_open(client, remote, AFC_FOPEN_RDONLY, &handle);
    if (opened != AFC_E_SUCCESS) {
        fprintf(stderr, "%s open failed: %d\n", name, opened);
        return 1;
    }
    FILE *file = fopen(local, "wb");
    if (!file) { afc_file_close(client, handle); return 1; }
    size_t total = 0;
    int failed = 0;
    for (;;) {
        char data[16384];
        uint32_t size = 0;
        if (afc_file_read(client, handle, data, sizeof(data), &size) != AFC_E_SUCCESS || size > sizeof(data)) {
            failed = 1; break;
        }
        if (!size) break;
        if (total + size > limit || fwrite(data, 1, size, file) != size) { failed = 1; break; }
        total += size;
    }
    if (fclose(file)) failed = 1;
    if (afc_file_close(client, handle) != AFC_E_SUCCESS) failed = 1;
    if (failed) unlink(local);
    printf("%s bytes=%zu status=%d\n", name, total, failed);
    return failed;
}

int main(int argc, char **argv)
{
    int screen = argc == 2 && !strcmp(argv[1], "--screen");
    int cluster = argc == 2 && !strcmp(argv[1], "--cluster");
    if (argc != 1 && !screen && !cluster) {
        fprintf(stderr, "Usage: fetch-video-probe [--screen|--cluster]\n");
        return 1;
    }
    const char *address = getenv("USBMUXD_SOCKET_ADDRESS");
    if (!address || strcmp(address, "127.0.0.1:32229")) {
        fprintf(stderr, "Set USBMUXD_SOCKET_ADDRESS=127.0.0.1:32229 for the owned emulator.\n");
        return 1;
    }
    alarm(60);
    idevice_t device = NULL;
    lockdownd_client_t lockdown = NULL;
    lockdownd_service_descriptor_t service = NULL;
    afc_client_t client = NULL;
    int failed = 1;
    idevice_error_t device_result = idevice_new(&device, "00008030-1122334455667788");
    if (device_result != IDEVICE_E_SUCCESS) {
        fprintf(stderr, "Owned emulator connection failed: %d\n", device_result);
        goto done;
    }
    lockdownd_error_t lockdown_result = lockdownd_client_new_with_handshake(device, &lockdown, "mhi2-video-probe");
    if (lockdown_result != LOCKDOWN_E_SUCCESS) {
        fprintf(stderr, "Lockdown handshake failed: %d\n", lockdown_result);
        goto done;
    }
    lockdown_result = lockdownd_start_service(lockdown, "com.apple.afc", &service);
    if (lockdown_result != LOCKDOWN_E_SUCCESS) {
        fprintf(stderr, "AFC service startup failed: %d\n", lockdown_result);
        goto done;
    }
    afc_error_t afc_result = afc_client_new(device, service, &client);
    if (afc_result != AFC_E_SUCCESS) {
        fprintf(stderr, "AFC connection failed: %d\n", afc_result);
        goto done;
    }
    if (cluster) {
        failed = copy_probe(client, "mhi2-carplay-cluster.json", 4096);
        if (!failed) failed |= copy_probe(client, "mhi2-carplay-cluster.bgra", 8388608);
    } else if (screen) {
        failed = copy_probe(client, "mhi2-carplay-screen.json", 4096);
        if (!failed) failed |= copy_probe(client, "mhi2-carplay-screen.bgra", 8388608);
    } else {
        failed = copy_probe(client, "mhi2-video-probe-avc1-sw.h264", 1048576);
        failed |= copy_probe(client, "mhi2-video-probe-avc1-default.h264", 1048576);
    }
done:
    if (cluster && failed) {
        unlink("build/ios-vm-lab/combined-usb/mhi2-carplay-cluster.json");
        unlink("build/ios-vm-lab/combined-usb/mhi2-carplay-cluster.bgra");
    }
    if (screen && failed) {
        /* A failed handshake must not leave an old frame looking freshly fetched. */
        unlink("build/ios-vm-lab/combined-usb/mhi2-carplay-screen.json");
        unlink("build/ios-vm-lab/combined-usb/mhi2-carplay-screen.bgra");
    }
    if (client) afc_client_free(client);
    if (service) lockdownd_service_descriptor_free(service);
    if (lockdown) lockdownd_client_free(lockdown);
    if (device) idevice_free(device);
    alarm(0);
    return failed;
}
