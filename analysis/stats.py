"""Summary statistics shared by the plots and tables: mean, sample std and a 95% confidence interval."""

import math

# Two-sided 95% Student t critical values by degrees of freedom; 1.96 beyond the table.
_T95 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262,
        10: 2.228, 11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131, 16: 2.120, 17: 2.110,
        18: 2.101, 19: 2.093, 20: 2.086, 25: 2.060, 30: 2.042}


def summary(values):
    """Return (n, mean, std, ci95 half-width) of a list of numbers."""
    n = len(values)
    if n == 0:
        return 0, 0.0, 0.0, 0.0
    mean = sum(values) / n
    if n == 1:
        return 1, mean, 0.0, 0.0
    std = math.sqrt(sum((v - mean) ** 2 for v in values) / (n - 1))
    df = n - 1
    t = _T95.get(df) or next((_T95[k] for k in sorted(_T95) if k >= df), 1.96)
    return n, mean, std, t * std / math.sqrt(n)


if __name__ == "__main__":
    n, mean, std, ci = summary([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
    assert n == 10 and mean == 5.5 and abs(std - 3.02765) < 1e-4 and abs(ci - 2.16570) < 1e-4
    assert summary([4.0]) == (1, 4.0, 0.0, 0.0) and summary([]) == (0, 0.0, 0.0, 0.0)
    print("stats self-check ok")
