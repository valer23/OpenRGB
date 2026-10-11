"""Launch Xtreme Tuner with passive lighting API instrumentation."""
from pathlib import Path
import argparse
import datetime
import json
import sys
import threading
import time

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'analysis-libs'))
import frida


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    device = frida.get_local_device()
    target = Path('C:/Program Files (x86)/XtremeTuner/XtremeTuner/Xtreme Tuner.exe')
    if not args.self_test and any(p.name.lower() == 'xtreme tuner.exe' for p in device.enumerate_processes()):
        raise RuntimeError('Exit Xtreme Tuner from its tray before starting this capture.')
    command = [sys.executable, '-c', 'import time; time.sleep(60)'] if args.self_test else [str(target)]
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    logfile = ROOT / f'Xtreme-{"selftest" if args.self_test else "lighting"}-{stamp}.jsonl'
    ready = threading.Event()
    session = None
    pid = None
    resumed = False
    with logfile.open('w', encoding='utf-8') as output:
        def message(msg, data):
            payload = msg.get('payload') if msg['type'] == 'send' else msg
            output.write(json.dumps({'time':time.time(),'event':payload})+'\n')
            output.flush()
            if isinstance(payload,dict) and payload.get('kind') == 'ready': ready.set()
            if msg['type'] == 'error': print('Trace error:',msg.get('description'),flush=True)
        try:
            pid = device.spawn(command, cwd=str(target.parent) if not args.self_test else str(ROOT))
            session = device.attach(pid)
            script = session.create_script((ROOT / 'xtreme_trace.js').read_text())
            script.on('message', message)
            script.load()
            if not ready.wait(10): raise RuntimeError('Trace did not initialise; do not change colours.')
            device.resume(pid)
            resumed = True
            if args.self_test:
                script.exports_sync.mark('self-test')
                script.exports_sync.stop()
                print('Trace loaded and marker API worked on a disposable Python process; no GPU app accessed.',flush=True)
                return
            print('Xtreme Tuner launched. Trace is ready.',flush=True)
            print('Keep Fan Control running. Do not change clocks, voltage or fan settings.',flush=True)
            input('Wait for Xtreme Tuner to open, select RGB, then press Enter here: ')
            steps = [('RED','Disable MBSync; choose static RED (255,0,0), then Apply.'),
                     ('GREEN','Choose static GREEN (0,255,0), then Apply.'),
                     ('BLUE','Choose static BLUE (0,0,255), then Apply.'),
                     ('RED_AGAIN','Choose static RED (255,0,0), then Apply.'),
                     ('MBSYNC_ON','Enable MBSync, then Apply.')]
            for label, instruction in steps:
                script.exports_sync.mark(label)
                print(instruction,flush=True)
                input('Wait five seconds, note the GPU colour, then press Enter here: ')
            script.exports_sync.stop()
            print('Capture complete. Saved:',logfile,flush=True)
            print('Tell Codex whether the GPU followed the manual red/green/blue/red changes.',flush=True)
        finally:
            if session is not None:
                try: session.detach()
                except frida.InvalidOperationError: pass
            # Leave the user-visible vendor app open, detached. Clean up only
            # our disposable self-test or a process that never resumed.
            if pid is not None and (args.self_test or not resumed):
                try: device.kill(pid)
                except frida.ProcessNotFoundError: pass


if __name__ == '__main__':
    main()
