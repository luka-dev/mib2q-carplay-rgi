"""Join only the owned QEMU vmnet member to the owned vphone NAT bridge."""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

from . import lab
from tests.carplay_lab.common import qnx_receiver

CONFIG="wireless-native-link-config.json"
STATE="wireless-native-link-state.json"


def topology(text: str) -> dict:
    """Parse ifconfig without assigning ownership to an unrelated member."""
    names=set();bridges={};cache={};current=None
    for line in text.splitlines():
        match=re.match(r"^(\w+):",line)
        if match:
            current=match[1];names.add(current)
            if re.fullmatch(r"bridge\d+",current):bridges[current]=set()
        elif current in bridges:
            match=re.search(r"\bmember: (\w+)\b",line)
            if match:bridges[current].add(match[1])
            match=re.match(r"\s+([0-9a-f:]+)\s+Vlan\d+\s+(vmenet\d+)\s",line)
            if match:
                mac=bytes(int(part,16) for part in match[1].split(":"))
                if len(mac)==6:cache[mac.hex()]=(current,match[2])
    return {"names":names,"bridges":bridges,"cache":cache}


def snapshot() -> dict:
    observed=topology(subprocess.run(["/sbin/ifconfig","-a"],capture_output=True,text=True,check=True).stdout)
    # Darwin omits the forwarding cache in `ifconfig -a`; query virtual
    # bridges explicitly to bind the guest MAC to its actual member.
    for bridge,members in observed["bridges"].items():
        if members and all(re.fullmatch(r"vmenet\d+",member) for member in members):
            detail=topology(subprocess.run(["/sbin/ifconfig",bridge],capture_output=True,text=True,check=True).stdout)
            observed["cache"].update(detail["cache"])
    return observed


def state() -> dict:
    path=lab.OUT/STATE
    return json.loads(path.read_text()) if path.exists() else {}


def save_state(value: dict) -> None:
    path=lab.OUT/STATE
    if path.is_symlink() or not path.is_file():raise ValueError("prepare network state as the VM owner first")
    owner=path.stat();temporary=None
    try:
        with tempfile.NamedTemporaryFile(mode="w",dir=lab.OUT,prefix=".wireless-link-",delete=False) as stream:
            temporary=Path(stream.name)
            json.dump(value,stream,indent=2);stream.write("\n")
        temporary.chmod(0o600)
        if os.geteuid()==0:os.chown(temporary,owner.st_uid,owner.st_gid)
        temporary.replace(path)
    finally:
        if temporary is not None:temporary.unlink(missing_ok=True)


def prepare() -> dict:
    if os.geteuid()==0:raise PermissionError("prepare-link must run as the vphone owner")
    if state().get("status") in ("joined","moving"):raise RuntimeError("restore the previous owned link first")
    settings=lab.profile();machine=lab.require_machine(settings,"running")
    info=lab.rpc(settings,"device.info")["result"]
    if info.get("model")!="iPhone99,11" or info.get("ios_version")!="26.6.2":raise ValueError("owned native VM profile differs")
    addresses=[row.split()[1] for row in info["network"]["addresses"] if re.fullmatch(r"en0 192\.168\.64\.\d+",row)]
    if len(addresses)!=1:raise ValueError("owned VM has no unique NAT address")
    arp=subprocess.run(["arp","-n",addresses[0]],capture_output=True,text=True,check=True).stdout
    match=re.search(r" at ([0-9a-f:]+) on (bridge\d+) ",arp)
    if not match:raise ValueError("owned VM has no NAT MAC mapping")
    mac=bytes(int(part,16) for part in match[1].split(":"))
    observed=snapshot()["cache"].get(mac.hex())
    if len(mac)!=6 or mac[0]&1 or not observed or observed[0]!=match[2]:raise ValueError("owned VM bridge member differs")
    result={"udid":machine["udid"],"vm_ip":addresses[0],"vm_mac":mac.hex(),"target_bridge":observed[0],"vm_member":observed[1]}
    lab.save(CONFIG,result)
    path=lab.OUT/STATE
    if not path.exists():path.write_text("{}\n");path.chmod(0o600)
    save_state({"status":"prepared",**result})
    return result


def validate_target(config: dict, observed: dict) -> None:
    machine=json.loads((lab.OUT/"machine.json").read_text())
    target=config.get("target_bridge","")
    if (machine.get("udid")!=config.get("udid") or machine.get("name")!=lab.profile()["machine"] or
        not re.fullmatch(r"bridge\d+",target) or not re.fullmatch(r"vmenet\d+",config.get("vm_member","")) or
        observed["cache"].get(config.get("vm_mac"))!=(target,config["vm_member"])):
        raise ValueError("owned vphone NAT bridge changed")
    if any(not re.fullmatch(r"vmenet\d+",member) for member in observed["bridges"].get(target,set())):
        raise ValueError("refusing a bridge with physical network members")


def new_member(before: dict, after: dict) -> tuple[str,str]:
    candidates={name for name in after["names"]-before["names"] if re.fullmatch(r"vmenet\d+",name)}
    if len(candidates)!=1:raise ValueError("expected one newly created QEMU vmnet member")
    member=candidates.pop()
    sources=[bridge for bridge,members in after["bridges"].items() if member in members]
    if len(sources)!=1 or after["bridges"][sources[0]]!={member}:raise ValueError("new QEMU member is not on its own virtual bridge")
    return sources[0],member


def change(bridge: str, operation: str, member: str) -> None:
    if not re.fullmatch(r"bridge\d+",bridge) or not re.fullmatch(r"vmenet\d+",member) or operation not in ("addm","deletem"):
        raise ValueError("only owned virtual bridge operations are allowed")
    subprocess.run(["/sbin/ifconfig",bridge,operation,member],check=True)


def join(before: dict) -> dict:
    if os.geteuid()!=0:raise PermissionError("vmnet link setup requires sudo")
    if state().get("status")!="prepared":raise ValueError("run prepare-link before starting the native QNX VM")
    config=json.loads((lab.OUT/CONFIG).read_text())
    deadline=time.monotonic()+10
    while True:
        observed=snapshot()
        if any(re.fullmatch(r"vmenet\d+",name) for name in observed["names"]-before["names"]):break
        if time.monotonic()>=deadline:raise RuntimeError("QEMU vmnet interface did not appear")
        time.sleep(.1)
    validate_target(config,observed)
    source,member=new_member(before,observed)
    pid=qnx_receiver.running()
    if not pid:raise RuntimeError("owned QNX VM exited before network join")
    record={**config,"qnx_pid":pid,"source_bridge":source,"qnx_member":member,"status":"moving"}
    save_state(record)
    try:
        change(source,"deletem",member)
        change(config["target_bridge"],"addm",member)
    except BaseException:
        leave()
        raise
    record["status"]="joined";save_state(record)
    return record


def leave() -> dict:
    record=state()
    if record.get("status") not in ("joined","moving"):return record
    if os.geteuid()!=0:raise PermissionError("restoring the owned vmnet link requires sudo")
    observed=snapshot();member=record["qnx_member"];source=record["source_bridge"];target=record["target_bridge"]
    pid=qnx_receiver.running()
    if pid is None and member not in observed["names"]:
        # vmnet removed its own adapter after an exited QEMU; no host mutation
        # is needed. A still-present member requires manual ownership review.
        record["status"]="restored";record["qnx_exited"]=True;save_state(record)
        return record
    if pid!=record["qnx_pid"]:raise RuntimeError("owned QNX process changed; link recovery needs review")
    if member not in observed["names"] or member==record["vm_member"]:
        raise RuntimeError("owned QNX virtual member disappeared or differs")
    memberships={bridge for bridge,members in observed["bridges"].items() if member in members}
    if memberships-{source,target}:raise RuntimeError("owned QNX member moved to an unexpected bridge")
    if record["vm_member"] not in observed["bridges"].get(target,set()):
        raise RuntimeError("owned vphone member moved")
    for bridge in (source,target):
        if bridge not in observed["bridges"] or any(not re.fullmatch(r"vmenet\d+",item) for item in observed["bridges"][bridge]):
            raise RuntimeError("refusing restoration through a physical or missing bridge")
    if member in observed["bridges"].get(target,set()):change(target,"deletem",member)
    if member not in observed["bridges"].get(source,set()):change(source,"addm",member)
    record["status"]="restored";save_state(record)
    return record
