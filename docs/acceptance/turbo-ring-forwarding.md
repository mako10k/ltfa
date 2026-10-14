# Turbo Ring extension verification —2026-10-14

Owner request: extend LTFA itself for generic Turbo Ring; v2 differences excluded. Requirements and
scope are recorded in [ADR0017](../decisions/0017-turbo-ring-forwarding.md). Source core9703315,
CLIc50c0ab; pre-existing simultaneous propagation024a10a preserved. No website or production write,
remote publication, active probes, external record or release occurred.

## Observed verification

- Release build: `cmake -S . -B build-turbo-ring -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build-turbo-ring -j 4` passed with warning-as-error configuration.
- Full regression: `ctest --test-dir build-turbo-ring --output-on-failure -j 4` —57/57passed.
- Independent review: previous qualification/source-ingress/reused-port issues fixed; final review
  found no required corrections. Focused core/CLI tests independently passed.
- Local installation into `build-turbo-ring/install-check`: installed command, example, language
  guide and documentation verified. Single linkfault keeps all4example switches reachable via
  backup; faults link1+link3 leave switch2+switch3 unreachable from switch1, with backup forwarding.
- JSON/human output explicitly distinguishes intrinsic physical/effective physical/logical state;
  normal backup available/blocking, failure backup activation, reasons, qualification and paths.

## Covered dispositions

| In-scope case | Disposition and evidence |
| --- | --- |
| Normal healthy ring, one declaredbackup | Implemented/tested core andCLI; backup blocking whilephysicalavailable |
| Every singlelinkfailure inclbackup | Implemented/tested core; all survivingdevicesreachable |
| Switch/port fault andsourcefault | Implemented/tested core/CLI; sourceingress constrained, incidentsegmentsdisabled |
| Multiplefault andsame-target overlap | Implemented/tested core/CLI; actualpartitions, unavailable dominatesdegraded, deterministicmergedinputs |
| Unknown/degraded/domainunavailable | Implemented/tested core; lower/upper reachability, incompleteunknown, traversabledegraded |
| Non-ring ingressunknown | Implemented/tested regression; uncertainingress doesnot changehealthybackuppolicy |
| Repairedphysicalsnapshot | Implemented/tested core; normalblockingrestored without mutatinginput |
| Scopequalification | Implemented/tested core; worst domain/member/port/ingressquality retained |
| Missing/ambiguousbackuppolicy, nonTurbo | Rejected/qualified/tested core/CLI with explicitreasons; no inventedselection orSTPalgorithm |
| Malformedtopology, duplicate/reusedport, directed/extraattachment | Rejected/tested core/CLI; simplecyclecontractvalidated |
| Invalidsnapshotstate/dimensions/source | Rejected/tested core/CLI |
| Workbudget exhaustion andfailurepropagation exhaustion | Implemented/tested core/CLI; incomplete, no unjustifiedunreachableproof |
| Output/controlchars/discovery/restrictedpolicy | ExistingCLIcontracts plusnewCLItests; help/guide/completion andexplicitrestrictedgatepass |
| Existinggenericanalysis | Fullregressionspass; existing path/SPOF semantics retained |

Unknowns remain explicit: actual site master/backupconfiguration, forwardingobservations and
convergence duration are not established by this syntheticfixture. Version-specific backupchoice,
STP election, coupledrings andvendor timing inference are not silently supplied by this extension.
