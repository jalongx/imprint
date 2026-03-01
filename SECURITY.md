# Security Policy

Imprint is a backup and imaging tool. Because it touches disks and filesystems,
security and data integrity are taken seriously.

This document explains how to report security issues responsibly.

---

## Supported versions

Imprint is a young project. At this stage, the **most recent release** is the
only version that receives security fixes.

If you are using an older version, please upgrade to the latest release before
reporting an issue, if possible.

---

## Reporting a vulnerability

If you believe you have found a security vulnerability in Imprint:

1. **Do not open a public GitHub issue.**
2. Send a detailed report by email to:

   **<your security/contact email here>**

Please include, when possible:

- **Description:** what you found and why you consider it a vulnerability  
- **Impact:** what an attacker could do or what data could be affected  
- **Version:** Imprint version and how you installed it (ISO, package, source)  
- **Environment:** OS/distro, filesystem type, and any relevant hardware details  
- **Steps to reproduce:** step‑by‑step instructions or a minimal example  

If the issue involves sensitive data, credentials, or anything that could
expose real users, keep your report as narrow as possible while still allowing
it to be reproduced.

---

## What to expect

When you report a potential vulnerability:

- **Acknowledgement:** You should receive a brief acknowledgement within a
  reasonable time frame, workload permitting.
- **Investigation:** The issue will be reviewed, reproduced (if possible),
  and assessed for impact.
- **Fix:** If confirmed, a fix or mitigation will be developed and included
  in a future release.
- **Disclosure:** Security issues may be described in release notes once a
  fix is available. Details may be limited to protect users who have not
  upgraded yet.

At this stage of the project, there are no guaranteed response times or formal
SLA. Commercial users with a separate support agreement may receive priority
handling.

---

## Scope

This policy covers:

- Imprint’s codebase and build artifacts
- The behavior of Imprint when used as documented

It does **not** cover:

- Third‑party tools, libraries, or kernels used alongside Imprint
- Misuse outside documented workflows
- Local system misconfiguration (e.g., unsafe sudo policies, weak passwords)

If you’re unsure whether something is in scope, you can still contact the
maintainer and ask.

---

## Responsible use

Imprint is a powerful tool that can destroy or overwrite data if misused.
Always:

- Test on non‑critical systems first
- Keep independent backups
- Verify images and restores

Security is a shared responsibility between the tool and its operators.
