"""Compile-only recovery of the VF stock reference; no CarPlay feature edits.

Used to compare the complete replacement against the independently supplied
MU1316 source. Explicit parent constructors match the stock inflator ABI.
"""
from pathlib import Path
import re
import sys
from java_reference import normalize_class_literals

source, destination = map(Path, sys.argv[1:])
s = normalize_class_literals(source.read_text())
s = re.sub(r'(private|public) class (\w+) extends (StateMachine|TMDeviceControl\.TMDeviceControlState)\b',
           r'\1 static class \2 extends \3', s)
s = re.sub(r'de\.audi\.app\.terminalmode\.device\.TMDeviceControl(?:\$[A-Za-z0-9]+)+(?=\.class)',
           lambda m: m[0].replace('$', '.'), s)
s = s.replace('new Runnable(this)', 'new Runnable()')
s = s.replace('new Consumer<ApplicationOwner>(this)', 'new Consumer()')
s = s.replace('this.this$0 = tmdevicecontrol1;', 'this.this$0 = TMDeviceControl.this;')
for args in ('this, this.logTag, tmdevice$useracceptstate', 'this, this.logTag, flag', 'this, this.logTag'):
    s = s.replace('new AbstractDispatcherRunnable(' + args + ')', 'new AbstractDispatcherRunnable(this.logTag)')
s = s.replace('setUserAcceptState(TMDevice.UserAcceptState tmdevice$useracceptstate, boolean flag)',
              'setUserAcceptState(final TMDevice.UserAcceptState tmdevice$useracceptstate, boolean flag)')
s = s.replace('this.val$userAcceptState = tmdevice$useracceptstate1;', 'this.val$userAcceptState = tmdevice$useracceptstate;')
s = s.replace('setStoreUserAcceptState(boolean flag)', 'setStoreUserAcceptState(final boolean flag)')
s = s.replace('this.val$storeAcceptSelection = flag1;', 'this.val$storeAcceptSelection = flag;')
s = s.replace('this.this$3 = tmdevicecontrol$sm$basestate$discoveredphonecallactivestate1;',
              'this.this$3 = DiscoveredPhoneCallActiveState.this;')
s = s.replace('public void accept(ApplicationOwner applicationowner) {',
              'public void accept(Object value) { accept((ApplicationOwner)value); } public void accept(ApplicationOwner applicationowner) {')
s = re.sub(r'new AbstractCommand\(\s*this,', 'new AbstractCommand(', s)
s = s.replace('this.this$4 = tmdevicecontrol$sm$basestate$discoveredstate$busy1;', 'this.this$4 = Busy.this;')
s = s.replace('this.this$6 = tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown$cleaningupanddetaching1;',
              'this.this$6 = CleaningUpAndDetaching.this;')
s = s.replace('"isNativeLittleEndian"', '""').replace('"isNativeLittleEdian"', '""')
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(s)
