# Budget C++20 Rewrite Spec (Draft)

## Goals
- Provide a CLI shell experience for managing budget resources and running projections.
- Preserve existing commands and output style as a baseline, but allow incremental improvements.
- Keep behavior aligned with NFCU CSV inputs and EST time zone.
- Keep cache data opaque and stored in a user-specific, platform-appropriate location.
- Integrate dotchart for long-term balance visualization.

## Non-Goals (for first milestone)
- GUI or web interface.
- Multi-bank adapters beyond NFCU CSV format.
- Full i18n expansion beyond current string translation behavior.

## Data Model
- **Money**: decimal currency values with exact cent precision.
- **Repetition**: recurrence defined by `when` and `repeater` (e.g., `Tue/2`, `15`, `1/2`).
- **TransactionType**: category, repetition, amount, conditions (regex for description/debit fields).
- **Exception**: category, date (MM-DD-YYYY), amount.
- **Profile**: name, description, balance.
- **Event**: category, amount, datetime, balance, epoch.

## CLI Contract (Baseline Compatibility)
- Preserve current command names and behaviors as defaults:
  - `run`, `totals`, `status`, `cats`, `trans`, `transactions`, `exceptions`, `themes`, `profiles`, `lasts`, `profile`, `balance`, `update`, `del`, `copy`, `save`, `export`, `unset`, `reload`, `exit`, `quit`.
- Output formatting should retain table-like appearance with ANSI themes.
- Allow safe extensions (new flags or commands) in future increments.

## Exceptions Policy
- Exceptions are both future variance overrides and markers for last occurrence.
- At startup, keep:
  - All future-dated exceptions.
  - The single most recent **expired** exception per category.
- This avoids unbounded growth while preserving last-occurrence signals.

## Time Zone
- Use **EST** to match NFCU CSV exports.
- All date math should be performed in EST, including last-occurrence computations.

## CSV Input
- NFCU CSV format is primary input.
- Must support UTF-8 BOM in header.
- Header sample:
  - `Posting Date, Transaction Date, Amount, Credit Debit Indicator, type, Type Group, Reference, Instructed Currency, Currency Exchange Rate, Instructed Amount, Description, Category, Check Serial Number, Card Ending, Rewards Total, Rewards Type`
- Field normalization:
  - Convert headers to lower snake case with non-word removal.
  - Keep `transaction_date` as primary date for history events.

## Matching & Categorization
- A transaction matches a TransactionType if all non-empty regex conditions match corresponding fields.
- If a transaction is already categorized (`cat`), do not reassign.

## Cache Format
- Opaque, simple binary format.
- Per-record file storage in user cache directory (platform-appropriate):
  - macOS: `~/Library/Caches/budget` (preferred)
  - Linux: `$XDG_CACHE_HOME/budget` or `~/.cache/budget`
- Versioned binary header to allow future migrations.
- Records: `profiles`, `themes`, `transaction_types`, `exceptions`, `lasts`, `session`.

## Dotchart Integration
- Preserve bar/line visualization of balances for long projections.
- Prefer direct integration (linking or embedding logic) if dotchart is available.
- Allow fallback if dotchart is missing.

## Testing Plan
- Unit tests for:
  - Money parsing/formatting
  - Repetition parsing and monthly factor
  - Date recurrence generation
  - Exception application
  - Last occurrence computation
- Golden tests vs. known CSV fixtures and transaction configs.

## Milestones
1. Core data model + recurrence + projection logic.
2. CSV ingestion and regex categorization.
3. CLI shell and table rendering.
4. Dotchart integration and polish.
