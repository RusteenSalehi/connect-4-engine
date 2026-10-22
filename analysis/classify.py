# Classifies each Impossible loss from the replay log and the solver results.
#   classify.py <dir>   reads <dir>/replay.txt and <dir>/solved.txt, writes <dir>/classified.json,
#                       plus the inputs for the depth step: <dir>/alts.in (c4solve) and <dir>/probe.in (c4probe)
import sys, json
S = sys.argv[1]
val = {}
for f in [S + '/solved.txt']:
    for l in open(f):
        p = l.split()
        if len(p) >= 2 and p[1] != '?': val[p[0]] = p[1]
        elif len(p) >= 2: val.setdefault(p[0], '?')
flip = {'W': 'L', 'L': 'W', 'D': 'D', '?': '?', None: '?'}
games = []
cur = None
for l in open(S + '/replay.txt'):
    p = l.split()
    if p[0] == 'game':
        cur = dict(g=int(p[1]), opening=p[3], imp=p[5], result=p[7], moves=p[9], log=[])
        games.append(cur)
    else:
        cur['log'].append(dict(ply=int(p[1]), who=p[2], col=int(p[3]), depth=int(p[5]), score=int(p[7])))
out = []
for G in games:
    if G['result'] != 'loss': continue
    m = G['moves']; impFirst = G['imp'] == 'first'
    decisions = []
    for e in G['log']:
        if e['who'] != 'imp': continue
        i = e['ply']
        pre = val.get(m[:i], '?')                 # for Impossible (to move)
        post = flip[val.get(m[:i + 1], '?')]      # for Impossible (Hard to move)
        if i + 1 == len(m): post = 'W'            # Impossible's own winning move cannot happen in a loss
        decisions.append(dict(i=i, col=e['col'], depth=e['depth'], score=e['score'], pre=pre, post=post))
    pres = [d['pre'] for d in decisions]
    first_b = next((d for d in decisions if d['pre'] in 'WD' and d['post'] == 'L'), None)
    if first_b: cat = 'b'
    elif all(p == 'L' for p in pres): cat = 'a'
    else: cat = 'c'
    hard_blunders = sum(1 for e in G['log'] if e['who'] == 'hard' and val.get(m[:e['ply']], '?') == 'W' and val.get(m[:e['ply'] + 1], '?') in 'WD')
    out.append(dict(g=G['g'], opening=G['opening'], imp=G['imp'], moves=m, cat=cat,
                    start=pres[0] if pres else '?', unknown=pres.count('?'),
                    first_b=first_b, hard_blunders=hard_blunders, n=len(m),
                    decisions=decisions))
json.dump(out, open(S + '/classified.json', 'w'), indent=1)
# At each first mistake: every alternative move, to solve, and the position with the played column, to probe.
with open(S + '/alts.in', 'w') as alts, open(S + '/probe.in', 'w') as probe:
    for o in out:
        if o['first_b']:
            pre = o['moves'][:o['first_b']['i']]
            alts.writelines(pre + str(k) + '\n' for k in range(1, 8))
            probe.write(f"{pre} {o['first_b']['col']}\n")
for o in out:
    b = o['first_b']
    print(o['g'], o['opening'], 'imp', o['imp'], 'cat', o['cat'], 'start-for-imp', o['start'], 'unknown-pre', o['unknown'],
          'hard-blunders', o['hard_blunders'],
          ('first mistake ply %d col %d depth %d score %d pre %s' % (b['i'], b['col'], b['depth'], b['score'], b['pre'])) if b else '')
