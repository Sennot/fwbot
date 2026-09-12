#!/usr/bin/env python3
"""Summarize a single FWL .debug.json; accepts ordinary reports too. Stdlib only."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    parser.add_argument('--input', type=int, help='Show repeated trials for one 1-based input number')
    parser.add_argument('--offset', type=int, help='Limit trial output to one offset')
    args = parser.parse_args()
    doc = json.loads(args.file.read_text(encoding='utf-8'))
    report = doc.get('analysis', doc)
    context = doc.get('context', {})
    print('Status:', report.get('status', 'unknown'))
    print('Version:', context.get('modVersion', 'not recorded'), '| Geode:', context.get('runtimeGeode', 'not recorded'))
    print('Settings:', json.dumps(report.get('settings', {}), ensure_ascii=False))
    print('Trace truncation:', doc.get('dropped', 'not a debug bundle'))
    for row in report.get('rows', []):
        if args.input is not None and row['input'] != args.input:
            continue
        print(f"#{row['input']} {row.get('gameMode', '')} {row['edge']} tick {row['frame']}: "
              f"local={row.get('localWindowAtOriginalInput')} (end {row.get('localEndTick')}), "
              f"goal={row.get('windowAtOriginalInput')}, complete={row.get('complete')}")
        print('  Late-failure offsets:', row.get('laterFailureOffsets', 'not recorded'))
    if args.input is not None:
        for trial in doc.get('trials', []):
            if trial.get('row') != args.input:
                continue
            if args.offset is not None and trial.get('offset') != args.offset:
                continue
            outcome = trial.get('outcome', {})
            details = outcome.get('details', {})
            print(f"Trial {trial.get('trial')} [{trial.get('phase')}] offset={trial.get('offset')} "
                  f"repeat={trial.get('repeat')}: local={outcome.get('localResult')}, "
                  f"goal={outcome.get('result')}, stop={outcome.get('stoppedAtBoundary')}, "
                  f"object={details.get('collisionObjectID')}")
            state = trial.get('localBoundaryState')
            if state:
                print('  At local boundary:', json.dumps({k: state.get(k) for k in
                      ('tick', 'nativeProgress', 'timeWarp', 'seed', 'players')}, ensure_ascii=False))
    events = doc.get('events', [])
    for event in events:
        if event.get('kind') in ('error', 'stop', 'level_closed'):
            print('Event:', json.dumps(event, ensure_ascii=False))
    loaded = [m['id'] for m in context.get('mods', []) if m.get('loaded')]
    print('Loaded mods:', ', '.join(loaded) or 'not recorded')


if __name__ == '__main__':
    main()
