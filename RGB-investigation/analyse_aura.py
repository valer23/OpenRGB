"""Summarize captured output reports without accessing hardware."""
from pathlib import Path
from collections import Counter
import json

root = Path(__file__).parent
rows = []
for line in (root / 'Aura-payloads.tsv').read_text(encoding='utf-8-sig').splitlines():
    fields = line.split('\t')
    if len(fields) < 6:
        continue
    payload = bytes.fromhex(fields[-1].replace(':', ''))
    rows.append((int(fields[0]), float(fields[1]), payload))

print('Reports:', len(rows))
print('Lengths:', dict(Counter(len(p) for _, _, p in rows)))
print('Commands:', dict(Counter(p[:3].hex() for _, _, p in rows)))
print('Unique reports:', len(set(p for _, _, p in rows)))
print('Header patterns:')
for h, count in Counter(p[:18].hex() for _, _, p in rows).items():
    print(h, count)

last = None
transitions = []
for frame, time, p in rows:
    # Command 05 places GRB colour data after an 18-byte header.
    colour = p[18:21].hex()
    if colour != last:
        transitions.append({'frame': frame, 'time': time, 'colour_bytes': colour})
        last = colour
print('Colour transitions:', json.dumps(transitions))
unique = list(dict.fromkeys(p for _, _, p in rows))
(root / 'Aura-unique-reports.json').write_text(json.dumps([p.hex() for p in unique], indent=2))

# Consecutive groups of 26 reports appear to form a complete three-header update.
for frame, time, p in rows[:28]:
    print(frame, f'{time:.3f}', ' '.join(f'{b:02x}' for b in p[:22]))
