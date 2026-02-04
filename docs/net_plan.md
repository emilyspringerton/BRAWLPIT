# TIPJAR Networking Plan (Fast-Follow)

## Authoritative simulation location
- Server authoritative simulation.
- Clients send compact `UserCmd` inputs with timestamps.

## Client-side prediction
- Predict local movement immediately from `UserCmd`.
- Reconcile with server snapshots on receipt.

## Reconciliation tolerance for service actions
- Orders, item spawns, and deliveries are server-authored.
- Clients may play provisional feedback, then correct on snapshot if needed.

## Snapshot format
- Fixed-tick snapshots containing compact entity arrays (id, type, position,
  owner_id, state flags).
- Snapshot deltas allowed but full snapshots must reconstruct world state for
  late joins.

## Late-join support
- A client can reconstruct state from the latest full snapshot plus deltas.
- Results screen derived from authoritative server stats.
