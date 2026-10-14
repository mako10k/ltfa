# Turbo Ring forwarding analysis

The offline forwarding analysis separates physical availability from logical packet forwarding.
A healthy backup segment can be physically available and logically blocking. It is not a failed
link. A declared physical failure releases a healthy backup segment; the remaining topology
then determines which switches are reachable. A second failure may partition the ring.

Use a `control-domain` with `protocol=turbo-ring`. Register every switch and physical point-to-point
link through `member_of`. Each link connects exactly two network endpoints owned by different
member switches. The complete member graph must form one simple cycle of at least three switches.
Register exactly one backup segment on its membership relation with `normal-state=standby`,
`blocking`, or `discarding`; register the others with `normal-state=forwarding`.

```text
entity ring:plant kind=control-domain plane=communication layer=l2 scope=site:a protocol=turbo-ring
relate switch:1 -> ring:plant kind=member_of role=manager
relate switch:2 -> ring:plant kind=member_of role=member
relate switch:3 -> ring:plant kind=member_of role=member
relate link:1-2 -> ring:plant kind=member_of normal-state=forwarding
relate link:2-3 -> ring:plant kind=member_of normal-state=forwarding
relate link:3-1 -> ring:plant kind=member_of role=backup normal-state=standby
```

This fragment illustrates membership only; use the complete runnable fixture referenced below.
Backup placement is an explicit input. LTFA does not infer an elected master, a MAC-address order,
a redundant port or a version-specific backup-selection algorithm.

Unknown physical state is not silently converted into a fault or a healthy link. Results distinguish
definite reachability from possible reachability and qualify uncertainty. An unavailable switch
removes its ring connectivity; a failed port affects its attached link. A domain that is unavailable
cannot forward; uncertain domain state prevents an unqualified claim. Budget exhaustion produces
an incomplete result rather than proving that a target is unreachable.

STP, RSTP, MSTP, ERPS, MRP and `other` remain distinguishable model protocols. Their protocol-specific
forwarding algorithms are not substituted with Turbo Ring behavior. Coupled or mesh domains do not
meet the single-ring topology contract and receive an explicit reason.

The result evaluates the converged forwarding state after the supplied conditions. Detection and
convergence time remain unknown; an assumed or vendor maximum is not an observed interruption.
Re-evaluating a repaired, healthy physical snapshot returns the normal blocked-backup state.
Physical snapshot input remains unchanged throughout analysis. An endpoint selected as source
constrains ingress by that endpoint's availability as well as its owning switch's availability.
Link results distinguish the link's own physical state from effective segment availability
(which includes both ports and both switches). A failed switch does not mean that its fibers broke.

Existing `analyze` generic path/SPOF behavior remains unchanged. Use domain forwarding analysis to
assess ring redundancy; direct endpoint-to-endpoint path failure does not establish that a ring
switch lost communication.

The full decision and requirement mapping are in
[ADR0017](decisions/0017-turbo-ring-forwarding.md).

## Runnable example

After building LTFA, run the repository example:

```sh
./build-turbo-ring/ltfa forwarding --model-root examples/turbo-ring --input main.ltfa \
  --domain ring:main --source switch:1 --format json
./build-turbo-ring/ltfa forwarding --model-root examples/turbo-ring --input main.ltfa \
  --domain ring:main --source switch:1 --failure failure:link-1 --format json
```

The command returns `ltfa.forwarding/v1` with the normal state and, when failures are selected,
the post-failure state. Repeat `--failure` to combine declared conditions. This is an offline
scenario, not an observed outage. Results report protocol, reasons, qualifications, logical
link states and per-device reachability. A confirmed path lists its switches and links.

Failure conditions are evaluated together for the converged state; declaration `after` values
are not a measured protocol switch duration. Recovery time stays unknown.

## Output interpretation

`normal` and `post_failure` each carry `result`, `protocol`, scope evidence `qualification`,
`backup_activated` (null when uncertain), `backup_activation_unknown`, unknown/degraded physical
flags, explicit `reasons`, `links`, `devices`, and consumed work `usage`. Each link reports its
`intrinsic_physical`, `effective_physical`, `logical`, and backup designation. Device paths
are populated only for definite reachability. An unreachable result means no path under the
selected ring model and stated physical conditions; it is not a detected live fault.

An incomplete result can retain proven reachable targets, but never turns unexamined targets
into proved unreachable ones. Non-Turbo Ring protocol declarations receive an unsupported reason.
Malformed topology is rejected. Source selection, model parsing, IDs and restricted-output policy
use the normal CLI errors. Work budgets cover domain discovery, topology validation, traversal and
physical failure propagation; the model loader retains its independent file/object bounds.
