#!/usr/bin/env python3
"""MV1 actual-library smoke, isolated loopback ports; never touches user SITL.

Run with the existing pymavlink environment:
.venv/bin/python tools/tests/multivehicle_udp_smoke.py build/mgcore.dylib
"""
import ctypes as C
import json
import socket
import sys
import threading
import time
from pathlib import Path

from pymavlink.dialects.v20 import ardupilotmega as mav

lib = C.CDLL(str(Path(sys.argv[1]).resolve()))
lib.mg_create.restype = C.c_void_p
lib.mg_destroy.argtypes = [C.c_void_p]
lib.mg_free_string.argtypes = [C.c_void_p]
lib.mg_connection_open.argtypes = [C.c_void_p, C.c_char_p]
lib.mg_connection_open.restype = C.c_int
lib.mg_vehicle_get_active.argtypes = [C.c_void_p]
lib.mg_vehicle_get_active.restype = C.c_int
for name in ('mg_vehicle_list', 'mg_vehicle_get_summary'):
    fn = getattr(lib, name)
    fn.argtypes = [C.c_void_p]
    fn.restype = C.c_void_p
for name in ('mg_connection_close', 'mg_vehicle_select', 'mg_connection_get_session_state'):
    fn = getattr(lib, name)
    fn.argtypes = [C.c_void_p, C.c_int]
    fn.restype = C.c_void_p


def read(fn, *args):
    pointer = fn(*args)
    assert pointer, 'Null native result'
    try:
        return json.loads(C.string_at(pointer))
    finally:
        lib.mg_free_string(pointer)


def run():
    core = lib.mg_create()
    peers, pumps = [], []
    stop = threading.Event()
    try:
        ids = []
        for sysid in (41, 42):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                probe.bind(('127.0.0.1', 0))
                port = probe.getsockname()[1]
            session = lib.mg_connection_open(core, f'udp://127.0.0.1:{port}'.encode())
            assert session > 0
            peer = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            peer.bind(('127.0.0.1', 0))
            peers.append(peer)
            encoder = mav.MAVLink(None, srcSystem=sysid, srcComponent=1)
            packet = encoder.heartbeat_encode(
                mav.MAV_TYPE_QUADROTOR, mav.MAV_AUTOPILOT_ARDUPILOTMEGA, 1, 4, 4,
            ).pack(encoder)

            def pump(sock=peer, data=packet, destination=('127.0.0.1', port)):
                while not stop.is_set():
                    sock.sendto(data, destination)
                    stop.wait(0.1)

            thread = threading.Thread(target=pump)
            thread.start()
            pumps.append(thread)
            ids.append(session)
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            vehicles = read(lib.mg_vehicle_list, core)
            if {v['system_id'] for v in vehicles} == {41, 42}:
                break
            time.sleep(0.05)
        assert {v['system_id'] for v in vehicles} == {41, 42}, vehicles
        assert lib.mg_vehicle_get_active(core) == -1
        for session, sysid in zip(ids, (41, 42)):
            assert read(lib.mg_vehicle_select, core, session)['success']
            assert read(lib.mg_vehicle_get_summary, core)['data']['system_id'] == sysid
            other = ids[1] if session == ids[0] else ids[0]
            assert read(lib.mg_connection_get_session_state, core, other)['session_id'] == other
            assert lib.mg_vehicle_get_active(core) == session
        assert read(lib.mg_connection_close, core, ids[0])['success']
        assert lib.mg_vehicle_get_active(core) == ids[1]
        assert read(lib.mg_connection_get_session_state, core, ids[1])['heartbeat_alive']
    finally:
        stop.set()
        for thread in pumps:
            thread.join()
        lib.mg_destroy(core)
        for peer in peers:
            peer.close()
    print('PASS: two actual UDP sessions, sysid 41/42, focus/read/close/destroy isolation', flush=True)


for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 1):
    run()
