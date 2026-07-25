# Security policy

## Supported versions

Security fixes are provided for the latest published ClouDS Music release.
Older builds may be asked to upgrade before a report can be investigated.

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub's private vulnerability
reporting feature when it is enabled. Until then, email `cadl@duck.com` with a
concise description, affected version, impact, and minimal reproduction steps.
Do not open a public issue for an unpatched vulnerability.

Never send or attach:

- `/3ds/ClouDS-Music/auth.bin` or a `MUSIC_U` value;
- cookies, QR `codekey` values, complete media URLs, or authorization
  parameters;
- `dspfirm.cdc` or any Nintendo firmware;
- another user's account data or copyrighted media cache; or
- an entire SD-card or application-data directory.

Use a test account when account state is required. Redact diagnostic excerpts
to the smallest relevant set of lines. The maintainers may request additional
information, but will not request passwords, cookies, firmware, or reusable
login credentials.

## Scope

Useful reports include credential exposure, unsafe file handling, TLS or
redirect validation failures, memory-corruption paths, malicious response
handling, and release-pipeline compromise. NetEase service outages, account
entitlement decisions, and requests to bypass VIP, region, purchase, or
takedown restrictions are not security vulnerabilities in this project.
