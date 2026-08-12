# Web UI access-control boundary

HomeGuard-S3 Web UI control commands use the same persisted `AccessControl` policy as the rest of the project.

- Read-only state/telemetry remains available to monitoring views.
- Security control requests require `actor` and a 4–12 digit PIN credential.
- Admin may execute all protected commands.
- User may monitor, arm/disarm, and operate water valves according to the project role policy.
- Guest remains read-only and cannot execute control commands.
- Authorization failures are fail-closed and recorded in the bounded access audit log.
- The browser does not persist the PIN; the PIN field is cleared after every command attempt.

The HTTP endpoint `/api/v1/system/security-command` must not mutate `SystemModel` before `AccessControl::authorize()` returns `Allowed`.
