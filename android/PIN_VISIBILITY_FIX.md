# PIN visibility control

The operator PIN field now includes a show/hide control in the trailing action.

Behavior:
- PIN remains masked by default.
- `Показати` reveals the PIN while editing.
- `Сховати` masks it again.
- The control is disabled after authentication together with the credential fields.

This file also records the UI regression fix that should be covered during device testing.
