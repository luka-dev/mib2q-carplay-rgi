"""Pinned Wi-Fi runtime installation/stock rollback for CPython or MicroPython.

Never restarts a process or the HU. Unknown inputs and stale backups are
preserved. A verified on-disk io-pkt is required for install/check; the running
stack still enforces its own native guards when loaded on a later owner reboot.
"""
import sys
import os
try:
    import hashlib
except ImportError:
    import uhashlib as hashlib
try:
    import binascii
except ImportError:
    import ubinascii as binascii
try:
    import json
except ImportError:
    import ujson as json

TARGET = '47a370bc30ca1b038fa5baee1c3eb6f3ef567788a70f57aed3957e98a35a1f05'
STOCK = 'd7fbf49b297665cbb8c6ba71b41ac892ccd062e0dcbd8339f3286555db1f394a'
STACK = 'baf6e5df318ba3ea22cf669017b04185a4da8abb77adb4cd6e40629a8e80dae7'
STACK_ON_DEVICE = '2f8b1ae6a80a0d0b925996c16288375c76628a470992ed350742dfc06adbf890'
STACKS = (STACK, STACK_ON_DEVICE)
FOREIGN_STOCKS = (
    '2cb49fff8d3ef81d675d310598ce17048b5e48e23841337a9e789435a6538f5f',
)
FOREIGN_STACKS = (
    '3e5188208c226d3bb953349a83e889df178ea9d1900d59b53630bf610dc18170',
    'ede352fa50dffa689eadf3c2d69bb2537c0d26d5b2495a7ce1e5e626e9d157bb',
)
KNOWN = (
    STOCK,
    'ca529b2b2526ce2c4ae1245966289aac58d95c33e35aa41dea33da03e764b2e7',
    'ff67efd8b8ec28c8fa10955c7474db9ec3ca0d15fe18072a3c3f4b6d6c5dd198',
    '189a63151bc66f27e745dcea98464bd482f11a8a7d44d2178783403b46ff233f',
    '662c24dac7370ae7445eff0a62e4ad8b122ac14a91b8762188de96a4c4b083df',
    TARGET,
)

def stat(path):
    try:
        result = os.stat(path)
    except OSError as error:
        if error.args and error.args[0] == 2:
            return None
        raise
    if result[0] & 0xf000 != 0x8000:
        raise ValueError('not a regular file: ' + path)
    return result

def digest(path):
    if stat(path) is None:
        raise ValueError('missing file: ' + path)
    checksum = hashlib.sha256()
    with open(path, 'rb') as source:
        while True:
            data = source.read(16384)
            if not data:
                break
            checksum.update(data)
    return binascii.hexlify(checksum.digest()).decode()

def resources(directory):
    with open(directory + '/manifest.json') as source:
        meta = json.load(source)
    if (meta.get('target_sha256') != TARGET or meta.get('stock_sha256') != STOCK
            or meta.get('io_pkt_sha256') != STACK
            or meta.get('io_pkt_on_device_sha256') != STACK_ON_DEVICE):
        raise ValueError('unsupported or damaged Wi-Fi resource manifest')
    if digest(directory + '/driver.so') != TARGET:
        raise ValueError('Wi-Fi runtime payload SHA mismatch')
    if digest(directory + '/stock.so') != STOCK:
        raise ValueError('Wi-Fi stock reference SHA mismatch')

def chmod755(path):
    if hasattr(os, 'chmod'):
        os.chmod(path, 0o755)
    else:
        # MicroPython's Unix/QNX os exposes system(), but not chmod().
        quoted = "'" + path.replace("'", "'\"'\"'") + "'"
        if os.system('chmod 755 ' + quoted):
            raise ValueError('chmod failed: ' + path)

def sync():
    if hasattr(os, 'sync'):
        os.sync()
    elif os.system('sync'):
        raise ValueError('sync failed')

def copy_new(source, target, expected):
    if stat(target) is not None:
        raise ValueError('refusing to overwrite staging file: ' + target)
    made = False
    try:
        with open(source, 'rb') as incoming:
            with open(target, 'wb') as outgoing:
                made = True
                while True:
                    data = incoming.read(16384)
                    if not data:
                        break
                    outgoing.write(data)
        if digest(target) != expected:
            raise ValueError('copy changed input bytes: ' + target)
        chmod755(target)
        sync()
    except Exception:
        if made:
            os.remove(target)
        raise

def backup(source, target, expected):
    if stat(target) is not None:
        if digest(target) != expected:
            raise ValueError('existing backup is not authentic; preserved: ' + target)
        return
    temporary = target + '.carplay-new'
    copy_new(source, temporary, expected)
    try:
        if stat(target) is not None:
            raise ValueError('backup appeared during transaction: ' + target)
        os.rename(temporary, target)
        sync()
    finally:
        if stat(temporary) is not None:
            os.remove(temporary)

def current_driver(path):
    if not path.endswith('/devnp-mrvl_wlan-sdiorm.so'):
        raise ValueError('unexpected Wi-Fi target filename')
    value = digest(path)
    if value not in KNOWN:
        raise ValueError('unknown/newer Wi-Fi driver preserved: ' + value)
    return value

def validate_backup(path):
    saved = path + '.carplay-stock'
    if stat(saved) is not None and digest(saved) != STOCK:
        raise ValueError('stale/non-stock .carplay-stock preserved; cannot use it')

def check(directory, path, stack, train=None):
    resources(directory)
    if not path.endswith('/devnp-mrvl_wlan-sdiorm.so'):
        raise ValueError('unexpected Wi-Fi target filename')
    stack_sha = digest(stack)
    driver_sha = digest(path)
    if driver_sha in FOREIGN_STOCKS or stack_sha in FOREIGN_STACKS:
        return 4, {'status': 'skipped', 'reason': 'stock binary from another firmware variant',
                   'current_sha256': driver_sha, 'io_pkt_sha256': stack_sha}
    if stack_sha not in STACKS:
        raise ValueError('io-pkt provenance mismatch; Wi-Fi unchanged')
    value = current_driver(path)
    if stat(path + '.carplay-new') is not None:
        raise ValueError('existing staging file preserved')
    validate_backup(path)
    previous = path + '.carplay-preupgrade.' + value
    if stat(previous) is not None and digest(previous) != value:
        raise ValueError('preupgrade backup mismatch; preserved')
    return (3 if value == TARGET else 0), {'status': 'already' if value == TARGET else 'ready',
        'current_sha256': value, 'target_sha256': TARGET, 'io_pkt_sha256': stack_sha,
        'io_pkt_path': stack, 'provenance': 'on-disk binary, activated only on owner reboot'}

def install(directory, path, stack, train=None):
    code, result = check(directory, path, stack, train)
    if code not in (0, 3):
        return code, result
    lock = path + '.carplay-install-lock'
    os.mkdir(lock)
    staged = path + '.carplay-new'
    staged_owned = False
    try:
        # Recheck after acquiring the cooperative installer lock.
        code, result = check(directory, path, stack, train)
        if code not in (0, 3):
            return code, result
        value = result['current_sha256']
        backup(directory + '/stock.so', path + '.carplay-stock', STOCK)
        if code == 3:
            return code, result
        backup(path, path + '.carplay-preupgrade.' + value, value)
        copy_new(directory + '/driver.so', staged, TARGET)
        staged_owned = True
        if digest(path) != value or digest(stack) != result['io_pkt_sha256']:
            raise ValueError('driver or stack changed during transaction; target preserved')
        os.rename(staged, path)
        sync()
        if digest(path) != TARGET:
            raise ValueError('activated file readback mismatch; verified backups retained')
        result['status'] = 'installed-on-disk'
        result['activation'] = 'manual owner reboot required; no restart performed'
        return 0, result
    finally:
        # Only delete our own verified stage. An unknown pre-existing stage is
        # left untouched by copy_new(), and must not be removed here.
        try:
            if staged_owned and stat(staged) is not None:
                os.remove(staged)
        finally:
            os.rmdir(lock)

def restore(directory, path):
    resources(directory)
    value = current_driver(path)
    validate_backup(path)
    saved = path + '.carplay-stock'
    if stat(saved) is None:
        raise ValueError('no verified first-stock backup; driver preserved')
    if value == STOCK:
        return 3, {'status': 'already-stock', 'current_sha256': value}
    lock = path + '.carplay-install-lock'
    os.mkdir(lock)
    staged = path + '.carplay-new'
    staged_owned = False
    try:
        if current_driver(path) != value:
            raise ValueError('driver changed before rollback; preserved')
        validate_backup(path)
        backup(path, path + '.carplay-preupgrade.' + value, value)
        copy_new(saved, staged, STOCK)
        staged_owned = True
        if digest(path) != value:
            raise ValueError('driver changed during rollback; preserved')
        os.rename(staged, path)
        sync()
        return 0, {'status': 'stock-restored-on-disk', 'previous_sha256': value,
                   'stock_sha256': STOCK, 'activation': 'manual owner reboot required'}
    finally:
        try:
            if staged_owned and stat(staged) is not None:
                os.remove(staged)
        finally:
            os.rmdir(lock)

def main(argv):
    try:
        if len(argv) in (5, 6) and argv[1] in ('check', 'install'):
            fn = check if argv[1] == 'check' else install
            code, result = fn(argv[2], argv[3], argv[4], argv[5] if len(argv) == 6 else None)
        elif len(argv) == 4 and argv[1] in ('restore', 'rollback'):
            code, result = restore(argv[2], argv[3])
        else:
            print('usage: wifi_runtime.py check|install RES DRIVER IO_PKT [TRAIN] | restore|rollback RES DRIVER')
            return 1
        print(json.dumps(result))
        return code
    except Exception as error:
        print('Wi-Fi runtime refused: ' + str(error))
        return 1

if __name__ == '__main__':
    sys.exit(main(sys.argv))
