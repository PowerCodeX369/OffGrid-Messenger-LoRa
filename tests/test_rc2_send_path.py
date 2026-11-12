"""Protect the v1.1 send path from accidentally reintroducing a CAD lockout."""
from pathlib import Path

source = Path(__file__).resolve().parents[1].joinpath('firmware/main/main.c').read_text()
start = source.index('static void service_queues(void)')
end = source.index('static void radio_loop(void *arg)', start)
queue = source[start:end]
assert 'offgrid_sx1278_cad(' not in queue
assert 'offgrid_sx1278_rx_busy(' not in queue
assert 'esp_err_t status=tx(&p->frame);' in queue
assert 'output("SENT|' in queue
assert 'OG_TX_ATTEMPTS' in queue
assert 'FW=1.2.0-rc.2' in source
print('PASS: rc.2 transmit path has no CAD gate; TX completion and retries retained')
