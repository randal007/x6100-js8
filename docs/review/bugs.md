# Bugs found in the feature review

Bugs found by reading the code feature by feature, following the check
sheet in [features.md](features.md). **No code was changed.** Started
2026-09-28 on `main` at `9391f80`; line numbers refer to that commit.

Findings from the earlier hunt ([bug-hunt-2026-09-28.md](../bug-hunt-2026-09-28.md),
`BH-n`) aren't repeated here.

- **Confidence:** *confirmed* (the failure path is clear in the code),
  *likely* (the code allows it; not reproduced), *possible* (worth a look).
- **Severity:** *high* (wrong frequency or power on the air, the radio
  left in a wrong state, messages or settings lost), *medium* (wrong
  behaviour you'd notice), *low* (cosmetic or rare).

## Summary

| ID | Feature | Bug | Severity | Confidence |
|---|---|---|---|---|
