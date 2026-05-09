import csv

def analyze():
    with open('/Users/rumi/Desktop/log/digital.csv', 'r') as f:
        reader = csv.reader(f)
        header = next(reader)
        data = list(reader)

    times = [float(row[0]) for row in data]
    ch2 = [int(row[1]) for row in data]
    ch3 = [int(row[2]) for row in data]

    def get_stats(vals, name):
        edges = []
        for i in range(1, len(vals)):
            if vals[i] != vals[i-1]:
                edges.append(times[i])
        
        intervals = []
        for i in range(1, len(edges)):
            intervals.append(edges[i] - edges[i-1])
            
        settled = intervals[2:] if len(intervals) > 4 else intervals
        if not settled:
            return 0, 0, 0, 0
        
        return min(settled), max(settled), sum(settled)/len(settled), len(intervals)

    s2 = get_stats(ch2, "Channel 2")
    s3 = get_stats(ch3, "Channel 3")

    print(f"Channel 2: Min={s2[0]:.3f}s, Max={s2[1]:.3f}s, Avg={s2[2]:.3f}s, Count={s2[3]}")
    print(f"Channel 3: Min={s3[0]:.3f}s, Max={s3[1]:.3f}s, Avg={s3[2]:.3f}s, Count={s3[3]}")

    def interpret(avg):
        if abs(avg - 1.0) < 0.1: return "1s pattern"
        if abs(avg - 0.5) < 0.1: return "500ms pattern"
        return "Unknown"

    print(f"\nInterpretation:")
    print(f"Channel 2 behaves as {interpret(s2[2])}")
    print(f"Channel 3 behaves as {interpret(s3[2])}")

analyze()
