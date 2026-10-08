# Skill evaluation and execution map

Evaluation date: 2026-10-07. Catalog-level triage covered **306 installed `SKILL.md` paths across 280 distinct skill-directory names**. Duplicate paths are not separate capabilities. This is not a claim that every skill body was executed: full bodies are read only when a task matches. The table below assigns every discovered directory once.

## Execution decisions

- Use the approved Superpowers subagent-driven workflow. Native CMake/C++ and Python build orchestration have separate edit ownership; specification and code-quality reviews are separate from implementation. Do not run GSD, gstack or another lifecycle manager concurrently.
- F01: test-driven-development, property-based-testing, modern-python, sharp-edges and insecure-defaults guide implementation; requesting-code-review, receiving-code-review, systematic-debugging and verification-before-completion govern review and real command execution. Agents must report missing engine/platform evidence rather than invent success.
- F02/S01–S08: typed quantities, bounded state and deterministic property tests; sanitizer/fuzzer specialists become applicable when memory-owning code and parsers actually exist. The installed dimensional-analysis skill primarily creates a whole-codebase annotation/audit pipeline; do not substitute that for the plan's typed unit API.
- B01–B06/V01/T05/R02: property-based-testing and sharp-edges for importer, pack, identity and rights boundaries; atheris/libfuzzer/harness-writing when fuzz targets exist. supply-chain-risk-auditor is conditional dependency-health analysis, not proof of license compliance.
- R01: insecure-defaults for source-only/trusted-runner separation. agentic-actions-auditor only if CI actually invokes AI agents; F01 CI does not.
- F03/G01/X04/R03: Unreal-specific camera/input/UI guidance below, plus actual native visual/input verification. Web-only frontend/React/SEO/browser-testing skills do not replace Unreal qualification.
- c-review is a separate multi-agent security-audit orchestrator, not a mandatory wrapper around the initial stateless conversion function. Reconsider at an explicit native security audit.
- Marketing, office-document production, blockchain, mobile and unrelated web deployment skills have no present implementation trigger. Re-evaluate only if their actual scope is requested.

## Additional skill research

### Selected as bounded Unreal references, not globally installed

Source: [quodsoler/unreal-engine-skills](https://github.com/quodsoler/unreal-engine-skills), MIT, pinned commit `f3742d7b688690810df369802b90430324e380b9` (2026-09-28). Repository/license, complete skill bodies and catalog metadata were read; examples have **not** been independently compiled here. Engine headers and native execution remain authoritative. No installer, lifecycle manager, MCP server or remote executable was run.

- [ue-gameplay-cameras](https://github.com/quodsoler/unreal-engine-skills/blob/f3742d7b688690810df369802b90430324e380b9/skills/ue-gameplay-cameras/SKILL.md): use the classic production spring-arm/player-camera stack for F03's small set of third-person/first-person/overhead modes, not the experimental Gameplay Camera System. Collision, view-target ownership, local mesh visibility and transitions are applicable. The game retains one authoritative pawn/state across views. X04 adds real vehicle seat/chase integration.
- [ue-cpp-foundations](https://github.com/quodsoler/unreal-engine-skills/blob/f3742d7b688690810df369802b90430324e380b9/skills/ue-cpp-foundations/SKILL.md): reflected UObject lifetime, TObjectPtr/UPROPERTY, generated-header ordering, subsystem ownership and game-thread boundaries for the Unreal bridge. Do not import Unreal containers/reflection into the independent core.

Related UE input, movement, module-build, testing and UMG skills are candidates to read and verify at their actual work packages, not already validated additions. Skill popularity is not correctness evidence.

### Deferred IFC/BIM candidate

[Impertio Studio Blender/Bonsai/IfcOpenShell skill package](https://github.com/Impertio-Studio/Blender-Bonsai-ifcOpenshell-Sverchok-Claude-Skill-Package), MIT, inspected commit `dc56112d81b25b64d3e1c7b765673a035981543f`. The [validation skill](https://github.com/Impertio-Studio/Blender-Bonsai-ifcOpenshell-Sverchok-Claude-Skill-Package/blob/dc56112d81b25b64d3e1c7b765673a035981543f/skills/ifcopenshell/impl/ifcos-impl-validation/SKILL.md) distinguishes schema and IDS validation and logged diagnostics. Exact compatibility with the locked IfcOpenShell 0.8.5/export pipeline has not been qualified. Do **not** adopt the collection or its arbitrary-Python Blender MCP configuration. The existing upstream APIs and official IfcOpenShell/Bonsai documentation remain primary.

## Complete catalog disposition

“Selected” means the engineering workflow applies when its trigger occurs, not that all seventeen are continuously running. “Conditional” means read the full body and verify tooling before use. “Alternative” means it overlaps the chosen workflow and is not combined with it. “Outside scope” means no trigger in this implementation request.

| Installed skill directory | Disposition |
|---|---|
| `ab-testing` | Outside current implementation scope |
| `ad-creative` | Outside current implementation scope |
| `address-sanitizer` | Conditional specialist |
| `ads` | Outside current implementation scope |
| `aflpp` | Conditional specialist |
| `agentic-actions-auditor` | Conditional specialist |
| `ai-seo` | Outside current implementation scope |
| `algorand-vulnerability-scanner` | Outside current implementation scope |
| `algorithmic-art` | Outside current implementation scope |
| `analytics` | Outside current implementation scope |
| `ask-questions-if-underspecified` | Outside current implementation scope |
| `aso` | Outside current implementation scope |
| `atheris` | Conditional specialist |
| `audit-augmentation` | Outside current implementation scope |
| `audit-context-building` | Outside current implementation scope |
| `audit-prep-assistant` | Outside current implementation scope |
| `autoplan` | Alternative workflow—not combined |
| `brainstorming` | Selected engineering workflow |
| `brand-guidelines` | Outside current implementation scope |
| `brandkit` | Outside current implementation scope |
| `burpsuite-project-parser` | Outside current implementation scope |
| `c-review` | Alternative workflow—not combined |
| `cairo-vulnerability-scanner` | Outside current implementation scope |
| `canary` | Alternative workflow—not combined |
| `canvas-design` | Outside current implementation scope |
| `careful` | Alternative workflow—not combined |
| `cargo-fuzz` | Outside current implementation scope |
| `churn-prevention` | Outside current implementation scope |
| `claude-api` | Outside current implementation scope |
| `claude-in-chrome-troubleshooting` | Outside current implementation scope |
| `co-marketing` | Outside current implementation scope |
| `code-maturity-assessor` | Conditional specialist |
| `codeql` | Conditional specialist |
| `codex` | Alternative workflow—not combined |
| `cold-email` | Outside current implementation scope |
| `community-marketing` | Outside current implementation scope |
| `competitor-profiling` | Outside current implementation scope |
| `competitors` | Outside current implementation scope |
| `constant-time-analysis` | Conditional specialist |
| `constant-time-testing` | Conditional specialist |
| `content-strategy` | Outside current implementation scope |
| `context-restore` | Alternative workflow—not combined |
| `context-save` | Alternative workflow—not combined |
| `copy-editing` | Outside current implementation scope |
| `copywriting` | Outside current implementation scope |
| `cosmos-vulnerability-scanner` | Outside current implementation scope |
| `coverage-analysis` | Conditional specialist |
| `cro` | Outside current implementation scope |
| `crypto-protocol-diagram` | Outside current implementation scope |
| `cso` | Alternative workflow—not combined |
| `customer-research` | Outside current implementation scope |
| `debug-buttercup` | Outside current implementation scope |
| `deploy-to-vercel` | Outside current implementation scope |
| `design-consultation` | Alternative workflow—not combined |
| `design-html` | Outside current implementation scope |
| `design-review` | Alternative workflow—not combined |
| `design-shotgun` | Alternative workflow—not combined |
| `design-taste-frontend` | Outside current implementation scope |
| `designing-workflow-skills` | Alternative workflow—not combined |
| `devcontainer-setup` | Conditional specialist |
| `devex-review` | Alternative workflow—not combined |
| `diagramming-code` | Outside current implementation scope |
| `differential-review` | Conditional specialist |
| `dimensional-analysis` | Outside current implementation scope |
| `directory-submissions` | Outside current implementation scope |
| `dispatching-parallel-agents` | Selected engineering workflow |
| `doc-coauthoring` | Outside current implementation scope |
| `document-generate` | Outside current implementation scope |
| `document-release` | Outside current implementation scope |
| `docx` | Outside current implementation scope |
| `dwarf-expert` | Conditional specialist |
| `emails` | Outside current implementation scope |
| `entry-point-analyzer` | Outside current implementation scope |
| `executing-plans` | Alternative workflow—not combined |
| `find-skills` | Selected engineering workflow |
| `finishing-a-development-branch` | Selected engineering workflow |
| `firebase-apk-scanner` | Outside current implementation scope |
| `fp-check` | Conditional specialist |
| `free-tools` | Outside current implementation scope |
| `freeze` | Alternative workflow—not combined |
| `frontend-design` | Outside current implementation scope |
| `fuzzing-dictionary` | Conditional specialist |
| `fuzzing-obstacles` | Conditional specialist |
| `genotoxic` | Conditional specialist |
| `gh-cli` | Conditional specialist |
| `git-cleanup` | Alternative workflow—not combined |
| `graph-evolution` | Outside current implementation scope |
| `gsd-add-tests` | Alternative workflow—not combined |
| `gsd-ai-integration-phase` | Alternative workflow—not combined |
| `gsd-audit-fix` | Alternative workflow—not combined |
| `gsd-audit-milestone` | Alternative workflow—not combined |
| `gsd-audit-uat` | Alternative workflow—not combined |
| `gsd-autonomous` | Alternative workflow—not combined |
| `gsd-capture` | Alternative workflow—not combined |
| `gsd-cleanup` | Alternative workflow—not combined |
| `gsd-code-review` | Alternative workflow—not combined |
| `gsd-complete-milestone` | Alternative workflow—not combined |
| `gsd-config` | Alternative workflow—not combined |
| `gsd-debug` | Alternative workflow—not combined |
| `gsd-discuss-phase` | Alternative workflow—not combined |
| `gsd-docs-update` | Alternative workflow—not combined |
| `gsd-eval-review` | Alternative workflow—not combined |
| `gsd-execute-phase` | Alternative workflow—not combined |
| `gsd-explore` | Alternative workflow—not combined |
| `gsd-extract-learnings` | Alternative workflow—not combined |
| `gsd-fast` | Alternative workflow—not combined |
| `gsd-forensics` | Alternative workflow—not combined |
| `gsd-graphify` | Alternative workflow—not combined |
| `gsd-health` | Alternative workflow—not combined |
| `gsd-help` | Alternative workflow—not combined |
| `gsd-import` | Alternative workflow—not combined |
| `gsd-inbox` | Alternative workflow—not combined |
| `gsd-ingest-docs` | Alternative workflow—not combined |
| `gsd-join-discord` | Alternative workflow—not combined |
| `gsd-manager` | Alternative workflow—not combined |
| `gsd-map-codebase` | Alternative workflow—not combined |
| `gsd-mempalace-capture` | Alternative workflow—not combined |
| `gsd-mempalace-recall` | Alternative workflow—not combined |
| `gsd-milestone-summary` | Alternative workflow—not combined |
| `gsd-mvp-phase` | Alternative workflow—not combined |
| `gsd-new-milestone` | Alternative workflow—not combined |
| `gsd-new-project` | Alternative workflow—not combined |
| `gsd-next` | Alternative workflow—not combined |
| `gsd-ns-context` | Alternative workflow—not combined |
| `gsd-ns-ideate` | Alternative workflow—not combined |
| `gsd-ns-manage` | Alternative workflow—not combined |
| `gsd-ns-project` | Alternative workflow—not combined |
| `gsd-ns-review` | Alternative workflow—not combined |
| `gsd-ns-workflow` | Alternative workflow—not combined |
| `gsd-onboard` | Alternative workflow—not combined |
| `gsd-pause-work` | Alternative workflow—not combined |
| `gsd-phase` | Alternative workflow—not combined |
| `gsd-plan-phase` | Alternative workflow—not combined |
| `gsd-plan-review-convergence` | Alternative workflow—not combined |
| `gsd-pr-branch` | Alternative workflow—not combined |
| `gsd-profile-user` | Alternative workflow—not combined |
| `gsd-progress` | Alternative workflow—not combined |
| `gsd-quick` | Alternative workflow—not combined |
| `gsd-quick-batch` | Alternative workflow—not combined |
| `gsd-reapply-patches` | Alternative workflow—not combined |
| `gsd-resume-work` | Alternative workflow—not combined |
| `gsd-review` | Alternative workflow—not combined |
| `gsd-review-backlog` | Alternative workflow—not combined |
| `gsd-secure-phase` | Alternative workflow—not combined |
| `gsd-settings` | Alternative workflow—not combined |
| `gsd-ship` | Alternative workflow—not combined |
| `gsd-sketch` | Alternative workflow—not combined |
| `gsd-spec-phase` | Alternative workflow—not combined |
| `gsd-spike` | Alternative workflow—not combined |
| `gsd-stats` | Alternative workflow—not combined |
| `gsd-surface` | Alternative workflow—not combined |
| `gsd-thread` | Alternative workflow—not combined |
| `gsd-ui-phase` | Alternative workflow—not combined |
| `gsd-ui-review` | Alternative workflow—not combined |
| `gsd-ultraplan-phase` | Alternative workflow—not combined |
| `gsd-undo` | Alternative workflow—not combined |
| `gsd-update` | Alternative workflow—not combined |
| `gsd-validate-phase` | Alternative workflow—not combined |
| `gsd-verify-work` | Alternative workflow—not combined |
| `gsd-workspace` | Alternative workflow—not combined |
| `gsd-workstreams` | Alternative workflow—not combined |
| `gstack-review` | Alternative workflow—not combined |
| `guard` | Alternative workflow—not combined |
| `guidelines-advisor` | Outside current implementation scope |
| `harness-writing` | Conditional specialist |
| `health` | Alternative workflow—not combined |
| `humanizer` | Outside current implementation scope |
| `image` | Outside current implementation scope |
| `impeccable` | Outside current implementation scope |
| `insecure-defaults` | Selected engineering workflow |
| `internal-comms` | Outside current implementation scope |
| `interpreting-culture-index` | Outside current implementation scope |
| `investigate` | Alternative workflow—not combined |
| `karpathy-guidelines` | Alternative workflow—not combined |
| `land-and-deploy` | Alternative workflow—not combined |
| `landing-report` | Outside current implementation scope |
| `launch` | Outside current implementation scope |
| `lead-magnets` | Outside current implementation scope |
| `learn` | Alternative workflow—not combined |
| `let-fate-decide` | Alternative workflow—not combined |
| `libafl` | Conditional specialist |
| `libfuzzer` | Conditional specialist |
| `make-pdf` | Outside current implementation scope |
| `marketing-ideas` | Outside current implementation scope |
| `marketing-psychology` | Outside current implementation scope |
| `mcp-builder` | Outside current implementation scope |
| `mermaid-to-proverif` | Outside current implementation scope |
| `modern-python` | Selected engineering workflow |
| `mutation-testing` | Conditional specialist |
| `office-hours` | Alternative workflow—not combined |
| `onboarding` | Outside current implementation scope |
| `ossfuzz` | Conditional specialist |
| `paseo` | Alternative workflow—not combined |
| `paseo-advisor` | Alternative workflow—not combined |
| `paseo-committee` | Alternative workflow—not combined |
| `paseo-handoff` | Alternative workflow—not combined |
| `paseo-help` | Alternative workflow—not combined |
| `paseo-plugin` | Alternative workflow—not combined |
| `paywalls` | Outside current implementation scope |
| `pdf` | Outside current implementation scope |
| `plan-ceo-review` | Alternative workflow—not combined |
| `plan-design-review` | Alternative workflow—not combined |
| `plan-devex-review` | Alternative workflow—not combined |
| `plan-eng-review` | Alternative workflow—not combined |
| `plan-tune` | Alternative workflow—not combined |
| `playwright` | Outside current implementation scope |
| `popups` | Outside current implementation scope |
| `pptx` | Outside current implementation scope |
| `pricing` | Outside current implementation scope |
| `product-marketing` | Outside current implementation scope |
| `programmatic-seo` | Outside current implementation scope |
| `property-based-testing` | Selected engineering workflow |
| `prospecting` | Outside current implementation scope |
| `qa` | Alternative workflow—not combined |
| `qa-only` | Alternative workflow—not combined |
| `receiving-code-review` | Selected engineering workflow |
| `referrals` | Outside current implementation scope |
| `remotion-best-practices` | Outside current implementation scope |
| `requesting-code-review` | Selected engineering workflow |
| `retro` | Alternative workflow—not combined |
| `revops` | Outside current implementation scope |
| `ruzzy` | Outside current implementation scope |
| `sales-enablement` | Outside current implementation scope |
| `sarif-parsing` | Conditional specialist |
| `schema` | Outside current implementation scope |
| `seatbelt-sandboxer` | Outside current implementation scope |
| `second-opinion` | Alternative workflow—not combined |
| `secure-workflow-guide` | Outside current implementation scope |
| `semgrep` | Conditional specialist |
| `semgrep-rule-creator` | Conditional specialist |
| `semgrep-rule-variant-creator` | Conditional specialist |
| `seo-audit` | Outside current implementation scope |
| `setup-deploy` | Alternative workflow—not combined |
| `sharp-edges` | Selected engineering workflow |
| `ship` | Alternative workflow—not combined |
| `signup` | Outside current implementation scope |
| `site-architecture` | Outside current implementation scope |
| `skill-creator` | Conditional specialist |
| `skill-improver` | Conditional specialist |
| `slack-gif-creator` | Outside current implementation scope |
| `sms` | Outside current implementation scope |
| `social` | Outside current implementation scope |
| `solana-vulnerability-scanner` | Outside current implementation scope |
| `spec` | Alternative workflow—not combined |
| `spec-to-code-compliance` | Outside current implementation scope |
| `subagent-driven-development` | Selected engineering workflow |
| `substrate-vulnerability-scanner` | Outside current implementation scope |
| `supply-chain-risk-auditor` | Conditional specialist |
| `systematic-debugging` | Selected engineering workflow |
| `template-skill` | Outside current implementation scope |
| `test-driven-development` | Selected engineering workflow |
| `testing-handbook-generator` | Outside current implementation scope |
| `theme-factory` | Outside current implementation scope |
| `token-integration-analyzer` | Outside current implementation scope |
| `ton-vulnerability-scanner` | Outside current implementation scope |
| `trailmark` | Conditional specialist |
| `trailmark-structural` | Conditional specialist |
| `trailmark-summary` | Conditional specialist |
| `unfreeze` | Alternative workflow—not combined |
| `using-git-worktrees` | Selected engineering workflow |
| `using-superpowers` | Selected engineering workflow |
| `variant-analysis` | Conditional specialist |
| `vector-forge` | Outside current implementation scope |
| `vercel-cli-with-tokens` | Outside current implementation scope |
| `vercel-composition-patterns` | Outside current implementation scope |
| `vercel-optimize` | Outside current implementation scope |
| `vercel-react-best-practices` | Outside current implementation scope |
| `vercel-react-native-skills` | Outside current implementation scope |
| `vercel-react-view-transitions` | Outside current implementation scope |
| `verification-before-completion` | Selected engineering workflow |
| `video` | Outside current implementation scope |
| `web-artifacts-builder` | Outside current implementation scope |
| `web-design-guidelines` | Outside current implementation scope |
| `webapp-testing` | Outside current implementation scope |
| `writing-plans` | Selected engineering workflow |
| `writing-skills` | Conditional specialist |
| `wycheproof` | Conditional specialist |
| `xlsx` | Outside current implementation scope |
| `yara-rule-authoring` | Outside current implementation scope |
| `zeroize-audit` | Conditional specialist |
