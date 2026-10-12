#!/usr/bin/env python3
"""Three lightweight MAVLink vehicles for local M-GCS UI validation.

This is intentionally a protocol simulator, not an ArduPilot flight-dynamics
model. Each vehicle listens on its own loopback UDP endpoint, sends heartbeat and
basic telemetry, accepts common GCS commands, and ACKs them so connection,
multi-vehicle switching and guided-action UI paths can be exercised locally.
"""

from __future__ import annotations

import argparse
import math
import signal
import socket
import time
from dataclasses import dataclass
from typing import Optional

from pymavlink.dialects.v20 import ardupilotmega as mav


MAV_TYPE_COPTER = 2
MAV_TYPE_PLANE = 1
MAV_TYPE_VTOL = 22
MAV_AUTOPILOT_ARDUPILOTMEGA = 3
MAV_MODE_FLAG_CUSTOM_MODE_ENABLED = 1
MAV_MODE_FLAG_SAFETY_ARMED = 128
MAV_STATE_STANDBY = 3
MAV_STATE_ACTIVE = 4

MAV_CMD_NAV_RETURN_TO_LAUNCH = 20
MAV_CMD_NAV_LAND = 21
MAV_CMD_NAV_TAKEOFF = 22
MAV_CMD_DO_SET_MODE = 176
MAV_CMD_DO_REPOSITION = 192
MAV_CMD_COMPONENT_ARM_DISARM = 400
MAV_CMD_MISSION_START = 300
MAV_CMD_DO_CHANGE_SPEED = 178
MAV_CMD_SET_MESSAGE_INTERVAL = 511


class UdpWriter:
    def __init__(self, target_port: int) -> None:
        self.target = ("127.0.0.1", target_port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.setblocking(False)

    def write(self, data: bytes) -> int:
        try:
            self.sock.sendto(data, self.target)
        except OSError:
            return 0
        return len(data)

    def close(self) -> None:
        self.sock.close()


@dataclass
class Target:
    latitude: float
    longitude: float
    altitude_m: float


class SimulatedVehicle:
    def __init__(self, sysid: int, vehicle_type: int, udp_port: int,
                 latitude: float, longitude: float, name: str) -> None:
        self.sysid = sysid
        self.vehicle_type = vehicle_type
        self.name = name
        self.writer = UdpWriter(udp_port)
        self.mav = mav.MAVLink(self.writer, srcSystem=sysid, srcComponent=1)
        self.home = Target(latitude, longitude, 0.0)
        self.position = Target(latitude, longitude, 0.0)
        self.target: Optional[Target] = None
        self.altitude_m = 0.0
        self.relative_altitude_m = 0.0
        self.armed = False
        self.flying = False
        self.mode = 0
        self.speed_m_s = 0.0
        self.ground_speed_m_s = 0.0
        self.heading_deg = 0.0
        self.started_at = time.monotonic()
        self.last_heartbeat = 0.0
        self.last_telemetry = 0.0
        self.last_home = 0.0
        self._sent_ready_text = False

    def boot_ms(self) -> int:
        return int((time.monotonic() - self.started_at) * 1000)

    def send_heartbeat(self) -> None:
        base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
        if self.armed:
            base_mode |= MAV_MODE_FLAG_SAFETY_ARMED
        system_status = MAV_STATE_ACTIVE if self.flying else MAV_STATE_STANDBY
        self.mav.heartbeat_send(
            self.vehicle_type,
            MAV_AUTOPILOT_ARDUPILOTMEGA,
            base_mode,
            self.mode,
            system_status,
        )

    def send_telemetry(self) -> None:
        now = time.monotonic()
        self.mav.sys_status_send(
            0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
            12, 12600, 1200, 88, 0, 0, 0, 0, 0, 0,
        )
        self.mav.gps_raw_int_send(
            int((now - self.started_at) * 1_000_000),
            3,
            int(self.position.latitude * 1e7),
            int(self.position.longitude * 1e7),
            int(self.altitude_m * 1000),
            100, 100, int(self.ground_speed_m_s * 100),
            int(self.heading_deg * 100), 14,
        )
        self.mav.global_position_int_send(
            self.boot_ms(),
            int(self.position.latitude * 1e7),
            int(self.position.longitude * 1e7),
            int(self.altitude_m * 1000),
            int(self.relative_altitude_m * 1000),
            int(math.cos(math.radians(self.heading_deg)) * self.ground_speed_m_s * 100),
            int(math.sin(math.radians(self.heading_deg)) * self.ground_speed_m_s * 100),
            0,
            int(self.heading_deg * 100),
        )
        self.mav.attitude_send(
            self.boot_ms(), 0.0, 0.0, math.radians(self.heading_deg),
            0.0, 0.0, 0.0,
        )
        self.mav.vfr_hud_send(
            self.speed_m_s, self.ground_speed_m_s, int(self.heading_deg) % 360,
            40 if self.flying else 0, self.altitude_m, 0.0,
        )
        if now - self.last_home >= 1.0:
            self.last_home = now
            self.mav.home_position_send(
                int(self.home.latitude * 1e7),
                int(self.home.longitude * 1e7),
                int(self.home.altitude_m * 1000),
                0.0, 0.0, 0.0, [1.0, 0.0, 0.0, 0.0],
                0.0, 0.0, 0.0,
            )
        if not self._sent_ready_text:
            self._sent_ready_text = True
            self.mav.statustext_send(6, f"{self.name} simulator ready".encode())

    def _ack(self, message, result: int = 0) -> None:
        self.mav.command_ack_send(
            int(message.command),
            result,
            target_system=int(getattr(message, "target_system", 255)),
            target_component=int(getattr(message, "target_component", 190)),
        )

    def handle_message(self, message) -> None:
        msg_type = message.get_type()
        if msg_type == "COMMAND_LONG":
            command = int(message.command)
            p1 = float(message.param1)
            p2 = float(message.param2)
            p3 = float(message.param3)
            p4 = float(message.param4)
            p5 = float(message.param5)
            p6 = float(message.param6)
            p7 = float(message.param7)
            if command == MAV_CMD_COMPONENT_ARM_DISARM:
                self.armed = p1 >= 0.5
                self.flying = self.armed and self.relative_altitude_m > 0.5
                self._ack(message)
            elif command == MAV_CMD_DO_SET_MODE:
                self.mode = int(p2)
                self._ack(message)
            elif command == MAV_CMD_NAV_TAKEOFF:
                self.armed = True
                self.flying = True
                self.mode = 4 if self.vehicle_type == MAV_TYPE_COPTER else 15
                altitude = p7 if p7 > 0 else max(self.relative_altitude_m, 30.0)
                self.target = Target(self.position.latitude, self.position.longitude, altitude)
                self._ack(message)
            elif command == MAV_CMD_DO_REPOSITION:
                self.armed = True
                self.flying = True
                self.mode = 4 if self.vehicle_type == MAV_TYPE_COPTER else 15
                self.target = Target(p5, p6, p7 if p7 > 0 else max(self.relative_altitude_m, 30.0))
                self._ack(message)
            elif command == MAV_CMD_NAV_RETURN_TO_LAUNCH:
                self.armed = True
                self.flying = True
                self.mode = 6 if self.vehicle_type == MAV_TYPE_COPTER else 11
                self.target = Target(self.home.latitude, self.home.longitude, max(self.relative_altitude_m, 30.0))
                self._ack(message)
            elif command == MAV_CMD_NAV_LAND:
                self.mode = 9 if self.vehicle_type == MAV_TYPE_COPTER else 19
                self.target = Target(self.position.latitude, self.position.longitude, 0.0)
                self._ack(message)
            elif command == MAV_CMD_MISSION_START:
                self.armed = True
                self.flying = True
                self.mode = 3 if self.vehicle_type == MAV_TYPE_COPTER else 10
                self._ack(message)
            elif command == MAV_CMD_DO_CHANGE_SPEED:
                self.speed_m_s = max(0.0, p2)
                self._ack(message)
            elif command == MAV_CMD_SET_MESSAGE_INTERVAL:
                self._ack(message)
            else:
                self._ack(message, 3)
        elif msg_type == "COMMAND_INT":
            command = int(message.command)
            if command == MAV_CMD_DO_REPOSITION:
                self.armed = True
                self.flying = True
                self.mode = 4 if self.vehicle_type == MAV_TYPE_COPTER else 15
                latitude = float(message.x) / 1e7
                longitude = float(message.y) / 1e7
                altitude = float(message.z) if message.z > 0 else 30.0
                self.target = Target(latitude, longitude, altitude)
                self._ack(message)
            else:
                self._ack(message, 3)
        elif msg_type == "MISSION_REQUEST_LIST":
            self.mav.mission_count_send(
                255, 190, 0, 0,
            )

    def tick(self, dt: float) -> None:
        if self.target is not None:
            north = (self.target.latitude - self.position.latitude) * 111_320.0
            east = (self.target.longitude - self.position.longitude) * 111_320.0 * math.cos(
                math.radians(self.position.latitude)
            )
            horizontal = math.hypot(north, east)
            if horizontal > 0.1:
                self.heading_deg = (math.degrees(math.atan2(east, north)) + 360.0) % 360.0
                speed = self.speed_m_s if self.speed_m_s > 0 else 10.0
                step = min(speed * dt, horizontal)
                self.position.latitude += (north / horizontal) * step / 111_320.0
                self.position.longitude += (
                    (east / horizontal) * step
                    / (111_320.0 * math.cos(math.radians(self.position.latitude)))
                )
                self.ground_speed_m_s = speed
            else:
                self.ground_speed_m_s = 0.0
            vertical = self.target.altitude_m - self.relative_altitude_m
            vertical_step = min(abs(vertical), 4.0 * dt)
            self.relative_altitude_m += math.copysign(vertical_step, vertical) if vertical else 0.0
            self.altitude_m = self.home.altitude_m + self.relative_altitude_m
            if horizontal <= 1.0 and abs(vertical) <= 0.5:
                self.target = None
                self.ground_speed_m_s = 0.0
                if self.relative_altitude_m <= 0.5:
                    self.relative_altitude_m = 0.0
                    self.altitude_m = self.home.altitude_m
                    self.armed = False
                    self.flying = False
        else:
            self.ground_speed_m_s = 0.0

        now = time.monotonic()
        if now - self.last_heartbeat >= 0.25:
            self.last_heartbeat = now
            self.send_heartbeat()
        if now - self.last_telemetry >= 0.1:
            self.last_telemetry = now
            self.send_telemetry()

    def poll(self) -> None:
        while True:
            try:
                data, _addr = self.writer.sock.recvfrom(4096)
            except BlockingIOError:
                break
            for message in self.mav.parse_buffer(data) or []:
                self.handle_message(message)

    def close(self) -> None:
        self.writer.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ports", default="14650,14651,14652")
    args = parser.parse_args()
    ports = [int(value) for value in args.ports.split(",")]
    if len(ports) != 3:
        raise SystemExit("exactly three ports are required")
    vehicles = [
        SimulatedVehicle(1, MAV_TYPE_COPTER, ports[0], 31.2243, 121.4692, "Copter"),
        SimulatedVehicle(2, MAV_TYPE_PLANE, ports[1], 31.2320, 121.4780, "Plane"),
        SimulatedVehicle(3, MAV_TYPE_VTOL, ports[2], 31.2150, 121.4860, "VTOL"),
    ]
    print("M-GCS simulated vehicles:", flush=True)
    for vehicle, port in zip(vehicles, ports):
        print(f"  sysid={vehicle.sysid}  udp://127.0.0.1:{port}  {vehicle.name}", flush=True)
    print("Press Ctrl-C to stop.", flush=True)

    running = True

    def stop(_signum, _frame) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    last = time.monotonic()
    try:
        while running:
            now = time.monotonic()
            dt = min(0.1, max(0.0, now - last))
            last = now
            for vehicle in vehicles:
                vehicle.poll()
                vehicle.tick(dt)
            time.sleep(0.02)
    finally:
        for vehicle in vehicles:
            vehicle.close()


if __name__ == "__main__":
    main()
