import csv

def get_stats(data):
    if not data: return (0, 0, 0)
    return min(data), max(data), sum(data)/len(data)

def analyze():
    # 1 & 2: Digital
    with open('/Users/rumi/Desktop/log/digital.csv', 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    
    times = [float(r['Time [s]']) for r in rows]
    ch2 = [int(r['Channel 2']) for r in rows]
    ch3 = [int(r['Channel 3']) for r in rows]
    
    results = {}
    for name, vals in [('Channel 2', ch2), ('Channel 3', ch3)]:
        edges = []
        rising = []
        falling = []
        last_v = vals[0]
        last_t = times[0]
        last_rising_t = None
        last_falling_t = None
        
        for t, v in zip(times[1:], vals[1:]):
            if v != last_v:
                edges.append(t - last_t)
                if v == 1:
                    if last_rising_t is not None:
                        rising.append(t - last_rising_t)
                    last_rising_t = t
                else:
                    if last_falling_t is not None:
                        falling.append(t - last_falling_t)
                    last_falling_t = t
                last_v = v
                last_t = t
        results[name] = {'edges': edges, 'rising': rising, 'falling': falling, 'last_rising': last_rising_t}

    # 3: UART
    def parse_uart(file, frame_id):
        starts = []
        with open(file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        for i in range(len(rows)-2):
            if rows[i]['Value'] == '0x7E' and rows[i+2]['Value'] == frame_id:
                starts.append(float(rows[i]['Time [s]']))
        diffs = [starts[i] - starts[i-1] for i in range(1, len(starts))]
        return starts, diffs

    a1_starts, a1_diffs = parse_uart('/Users/rumi/Desktop/log/1.txt', '0xA1')
    c2_1_starts, c2_1_diffs = parse_uart('/Users/rumi/Desktop/log/1.txt', '0xC2')
    c2_2_starts, c2_2_diffs = parse_uart('/Users/rumi/Desktop/log/2.txt', '0xC2')

    print("1) Edge Intervals (min/max/avg):")
    for ch in ['Channel 2', 'Channel 3']:
        print(f"  {ch}: {get_stats(results[ch]['edges'])}")
    
    print("\n2) Same-direction (Rising-to-Rising):")
    for ch in ['Channel 2', 'Channel 3']:
        print(f"  {ch} Rising: {get_stats(results[ch]['rising'])}")

    print("\n3) UART Stats:")
    print(f"  A1 (1.txt) Interval: {get_stats(a1_diffs)}")
    print(f"  C2 (1.txt) Interval: {get_stats(c2_1_diffs)}")
    print(f"  C2 (2.txt) Interval: {get_stats(c2_2_diffs)}")

    print("\n4) Correlation:")
    # A1 vs Channel 3 rising
    c3_risings = []
    last_v = ch3[0]
    for t, v in zip(times[1:], ch3[1:]):
        if v == 1 and last_v == 0:
            c3_risings.append(t)
        last_v = v
    
    offsets = []
    for a1_t in a1_starts:
        closest = min(c3_risings, key=lambda x: abs(x - a1_t))
        offsets.append(closest - a1_t)
    print(f"  C3 rising count: {len(c3_risings)}, A1 count: {len(a1_starts)}")
    print(f"  Avg C3 rising - A1 start: {sum(offsets)/len(offsets) if offsets else 0:.6f}s")

analyze()
