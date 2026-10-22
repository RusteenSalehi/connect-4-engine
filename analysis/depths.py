# For each first mistake: the values of Impossible's alternatives and the search depth its move rule
# needs to pick a non-losing move.
#   depths.py <dir>   reads <dir>/classified.json, <dir>/alts.txt and <dir>/probe.txt, writes <dir>/depths.json
import json, sys
S = sys.argv[1]
MIN_WIN = 9958
c = json.load(open(S + '/classified.json'))
sol = {l.split()[0]: l.split()[1] for l in open(S + '/alts.txt')}
flip = {'W': 'L', 'L': 'W', 'D': 'D', '?': '?'}
rows = []
for o in c:
    b = o['first_b']
    if not b: continue
    pre = o['moves'][:b['i']]
    imp_val = {k: flip[sol[pre + str(k)]] for k in range(1, 8) if pre + str(k) in sol}
    good = sorted(k for k, v in imp_val.items() if v in 'WD')
    best = 'W' if 'W' in imp_val.values() else 'D'
    lines = [p for p in (l.split() for l in open(S + '/probe.txt')) if p[0] == pre]
    probe = [(int(p[2]), int(p[4]), int(p[6]), int(p[8]), float(p[10])) for p in lines]  # d pick pickscore watch ms
    at_actual = next((p for p in probe if p[0] == b['depth']), None)
    consistent = at_actual is not None and at_actual[1] == b['col']
    maxd = probe[-1][0]
    stable = None
    for idx, p in enumerate(probe):
        if all(q[1] in good for q in probe[idx:]):
            stable = p; break
    sees_loss = next((p for p in probe if p[3] <= -MIN_WIN), None)
    rows.append(dict(g=o['g'], opening=o['opening'], imp=o['imp'], ply=b['i'], pre=pre, played=b['col'],
                     depth=b['depth'], pre_val=b['pre'], imp_val=imp_val, good=good, best=best,
                     consistent=consistent, maxd=maxd, maxms=probe[-1][4],
                     stable_d=stable[0] if stable else None, stable_pick=stable[1] if stable else None,
                     stable_ms=stable[4] if stable else None,
                     sees_loss_d=sees_loss[0] if sees_loss else None, sees_loss_ms=sees_loss[4] if sees_loss else None,
                     picks=[(p[0], p[1]) for p in probe]))
json.dump(rows, open(S + '/depths.json', 'w'), indent=1)
for r in rows:
    print(f"g{r['g']} open {r['opening']} imp {r['imp']} ply {r['ply']} played {r['played']} at d{r['depth']} | "
          f"values {''.join(r['imp_val'].get(k,'-') for k in range(1,8))} | consistent {r['consistent']} | "
          f"stable non-losing from d{r['stable_d']} (col {r['stable_pick']}, {r['stable_ms']} ms) | "
          f"loss of played move visible at d{r['sees_loss_d']} | probed to d{r['maxd']} ({r['maxms']:.0f} ms)")
    print('   picks', ' '.join(f"{d}:{p}" for d, p in r['picks']))
