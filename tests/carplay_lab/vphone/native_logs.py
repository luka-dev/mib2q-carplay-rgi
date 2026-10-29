"""Bounded native service logs through the existing vphone guest RPC."""
from concurrent.futures import ThreadPoolExecutor
import time
from . import lab

PROCESSES=("accessoryd","carkitd","ACCCarPlayServic","caraccessoryd","airplayd","wifid")


def capture(seconds=45):
    if type(seconds) is not int or not 1<=seconds<=45:
        raise ValueError("native log capture must be 1..45 seconds")
    settings=lab.profile()
    lab.require_machine(settings,"running")
    def read(process):
        before=time.time_ns()
        try:
            reply=lab.rpc(settings,"logs.syslog",{"seconds":seconds,"process":process,"max_lines":1500})
            return {"process":process,"start_ns":before,"end_ns":time.time_ns(),"reply":reply}
        except Exception as error:
            return {"process":process,"start_ns":before,"end_ns":time.time_ns(),"error":str(error)}
    with ThreadPoolExecutor(max_workers=len(PROCESSES)) as pool:
        result=list(pool.map(read,PROCESSES))
    lab.save("wireless-native-guest-logs.json",result)
    return result
