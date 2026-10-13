"""Bounded CoreLocation simulation in the owned guest, with host event timestamps."""
import argparse
import json
import math
import time

from . import lab

# A laboratory path near Apple Park; not an assertion of a drivable road route.
START = (37.3349, -122.0090)
END = (37.3320, -122.0160)


def run(seconds: int = 40) -> dict:
    if type(seconds) is not int or not 2 <= seconds <= 60:
        raise ValueError("moving route duration must be 2..60 seconds")
    settings = lab.profile()
    machine = lab.require_machine(settings, "running")
    if machine.get("syncsHostLocation") is not False:
        raise RuntimeError("disable host location synchronization on the owned guest first")
    radians = math.pi/180
    dx = (END[1]-START[1])*radians*6371000*math.cos(START[0]*radians)
    dy = (END[0]-START[0])*radians*6371000
    course = math.degrees(math.atan2(dx, dy)) % 360
    speed = math.hypot(dx, dy)/seconds
    result = {"machine": settings["machine"], "seconds": seconds,
              "simulation": "straight laboratory path, not road-matched navigation",
              "host_location_modified": False, "events": []}
    begin = time.monotonic()
    try:
        for i in range(seconds+1):
            time.sleep(max(0, begin+i-time.monotonic()))
            params = {"latitude": START[0]+(END[0]-START[0])*i/seconds,
                      "longitude": START[1]+(END[1]-START[1])*i/seconds,
                      "speed": speed, "course": course, "horizontal_accuracy": 5}
            before = time.time_ns()
            reply = lab.rpc(settings, "location.set", params)
            result["events"].append({"index": i, "host_start_ns": before,
                                      "host_end_ns": time.time_ns(), "params": params,
                                      "result": reply})
        result["complete"] = True
    finally:
        # Leave the guest at the final observed coordinate, stationary.
        try:
            if result["events"]:
                final = dict(result["events"][-1]["params"], speed=0)
                result["stationary_result"] = lab.rpc(settings, "location.set", final)
        finally:
            lab.save("moving-route.json", result)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=40)
    print(json.dumps(run(parser.parse_args().seconds)))
