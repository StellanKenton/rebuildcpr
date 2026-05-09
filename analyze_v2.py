import csv

def analyze():
    with open('/Users/rumi/Desktop/log/digital.csv', 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    
    times = [float(r['Time [s]']) for r in rows]
    
    for ch_name in ['Channel 2', 'Channel 3']:
        vals = [int(r[ch_name]) for r in rows]
        edges = []
        last_v = vals[0]
        last_t = times[0]
        
        for t, v in zip(times[1:], vals[1:]):
            if v != last_v:
                edges.append(t - last_t)
                last_v = v
                last_t = t
        
        print(f"--- {ch_name} ---")
        print(f"Edges: {['%.6f' % e for e in edges]}")
        
        settled = edges[1:]
        if settled:
            avg = sum(settled) / len(settled)
            mn, mx = min(settled), max(settled)
            print(f"Settled (min/max/avg): {mn:.6f}, {mx:.6f}, {avg:.6f}")
            target = 1.0 if ch_name == 'Channel 2' else 0.5
            print(f"Ratio to {target}s: {avg/target:.6f}")
        else:
            print("No settled intervals")

analyze()
