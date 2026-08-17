#!/usr/bin/env python3
import time
from pathlib import Path
import dronecan
import pytest

pytestmark = pytest.mark.filterwarnings(
    "ignore:The 'bustype' argument is deprecated.*:DeprecationWarning"
)

APP_NODE_ID = 50
APP_IFACE = "slcan0"
NODE_ID_TESTER = 100
MESSAGE_TIMEOUT = 1.2
SERVICE_TIMEOUT = 1.0
MAX_STRING_LENGTH = 56
PARAM_DESCRIPTION_INDEX = 1
PARAM_SYSTEM_NAME_INDEX = 2
PARAM_DESCRIPTION_NAME = "uavcan.node.description"
PARAM_SYSTEM_NAME = "system.name"

def make_dronecan_node() -> dronecan.node.Node:
    node = dronecan.make_node(
        APP_IFACE,
        node_id=NODE_ID_TESTER,
        bitrate=1_000_000,
        baudrate=1_000_000,
        node_status_interval=3600,
    )
    node.mode = dronecan.uavcan.protocol.NodeStatus().MODE_OPERATIONAL
    node.health = dronecan.uavcan.protocol.NodeStatus().HEALTH_OK
    return node

def request(node: dronecan.node.Node, payload):
    response_transfer_event = None

    def _callback(transfer_event: dronecan.node.TransferEvent):
        nonlocal response_transfer_event
        if transfer_event is None or transfer_event.transfer.source_node_id != APP_NODE_ID:
            return
        response_transfer_event = transfer_event

    node.request(payload, APP_NODE_ID, _callback, timeout=SERVICE_TIMEOUT)
    deadline = time.monotonic() + SERVICE_TIMEOUT + 0.1
    while response_transfer_event is None and time.monotonic() < deadline:
        node.spin(0.01)
    assert response_transfer_event is not None, f"{payload} response is missing"
    return response_transfer_event.response

def as_string(value) -> str:
    return str(value).rstrip("\x00")

@pytest.mark.dependency()
def test_node_status():
    node = make_dronecan_node()

    node_status_transfer_event = None
    def _on_node_status(transfer_event: dronecan.node.TransferEvent):
        if transfer_event is None or transfer_event.transfer.source_node_id != APP_NODE_ID:
            return  # ignore other nodes if present
        nonlocal node_status_transfer_event
        node_status_transfer_event = transfer_event

    node.add_handler(dronecan.uavcan.protocol.NodeStatus, _on_node_status)
    node.spin(MESSAGE_TIMEOUT)
    assert node_status_transfer_event is not None, (
        f"No NodeStatus from node {APP_NODE_ID} on iface {APP_IFACE}. "
        "Hint: ensure the SITL app is running (make ubuntu) and slcan0 is up (scripts/vcan.sh slcan0)."
    )
    assert node_status_transfer_event.transfer.source_node_id == APP_NODE_ID
    assert node_status_transfer_event.message.health == 0  # HEALTH_OK
    assert node_status_transfer_event.message.mode == 0  # MODE_OPERATIONAL

@pytest.mark.dependency(depends=["test_node_status"])
def test_get_node_info_request():
    node = make_dronecan_node()

    req = dronecan.uavcan.protocol.GetNodeInfo.Request()
    get_node_info_response_transfer_event = None
    def _get_info_callback(transfer_event: dronecan.node.TransferEvent):
        if transfer_event is None or transfer_event.transfer.source_node_id != APP_NODE_ID:
            return  # ignore other nodes if present
        nonlocal get_node_info_response_transfer_event
        print(transfer_event)
        get_node_info_response_transfer_event = transfer_event

    node.request(req, APP_NODE_ID, _get_info_callback)
    node.spin(SERVICE_TIMEOUT)
    assert get_node_info_response_transfer_event is not None, (
        "GetNodeInfo response is missing. Hint: check node id, interface, and that the app is running."
    )
    assert get_node_info_response_transfer_event.transfer.source_node_id == APP_NODE_ID
    assert get_node_info_response_transfer_event.transfer.dest_node_id == NODE_ID_TESTER
    assert get_node_info_response_transfer_event.transfer.dest_node_id == NODE_ID_TESTER
    assert get_node_info_response_transfer_event.response.name.decode("utf-8").rstrip("\x00") == "com.example.libdcnode.sitl"

@pytest.mark.dependency(depends=["test_node_status"])
def test_max_length_string_param():
    node = make_dronecan_node()
    max_string = "S" * MAX_STRING_LENGTH
    neighbor_marker = "AFTER_MAX_STRING"

    req = dronecan.uavcan.protocol.param.GetSet.Request(name=PARAM_SYSTEM_NAME)
    req.value = dronecan.uavcan.protocol.param.Value(string_value=neighbor_marker)
    response = request(node, req)
    assert as_string(response.name) == PARAM_SYSTEM_NAME
    assert as_string(response.value.string_value) == neighbor_marker

    req = dronecan.uavcan.protocol.param.GetSet.Request(name=PARAM_DESCRIPTION_NAME)
    req.value = dronecan.uavcan.protocol.param.Value(string_value=max_string)
    response = request(node, req)
    assert as_string(response.name) == PARAM_DESCRIPTION_NAME
    assert as_string(response.value.string_value) == max_string

    req = dronecan.uavcan.protocol.param.GetSet.Request(index=PARAM_DESCRIPTION_INDEX)
    response = request(node, req)
    assert as_string(response.name) == PARAM_DESCRIPTION_NAME
    assert as_string(response.value.string_value) == max_string
    assert neighbor_marker not in as_string(response.value.string_value)

    req = dronecan.uavcan.protocol.param.GetSet.Request(index=PARAM_SYSTEM_NAME_INDEX)
    response = request(node, req)
    assert as_string(response.name) == PARAM_SYSTEM_NAME
    assert as_string(response.value.string_value) == neighbor_marker

if __name__ == "__main__":
    raise SystemExit(pytest.main([str(Path(__file__).resolve())]))
