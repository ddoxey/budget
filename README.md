# Budget CLI

An interactive CLI shell for managing budgets and projecting future balances.
It ingests a transactions CSV (optimized for NFCU exports but configurable),
supports profiles, exceptions, themes, and renders projections with a
dotchart.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Run:
```bash
./build/budget
```

## Configuration

The CLI looks for `budget.json` in the current directory or your home
directory to configure paths. The status table reports the active config,
download, and cache directories.

Header mapping is stored as plain JSON and can be edited interactively:

- `headers` — show mappings
- `update header <internal> <header>` — add/update mapping
- `del header <internal>` — remove mapping

Theme config is stored in `budget_theme.json` (in the config dir):

- `themeconfig` — show loaded values
- `update theme <cat|default> <fg> <bg> <style>`
- `themes randomize [category]`

## Key Commands

- `help` or `help <topic>`
- `status` — summary of configuration
- `profile [<name>]` — show or switch profile
- `profiles` — list profiles
- `update profile <name> <description> [<balance>]`
- `copy profile <from> <to>`
- `balance [<amount>|+=<amount>|-=<amount>]`
- `trans` / `transactions` — list transaction types
- `cats` — list categories (multi-column)
- `exceptions` — list exceptions
- `lasts` — list last occurrences
- `run <number>(d|m|y) [<start-date>]` — projections
- `totals <number>(d|m|y)` — totals per category
- `reload` — reprocess CSV + cache
- `clear` — clear terminal
- `exit` / `quit`

## Transaction Types

Create or update a recurring transaction:

```
update transaction <cat> <when>[/<repeat>] <amount> [<desc-regex>] [<amount-regex>]
```

Examples:

```
update transaction PAYDAY Fri 1500
update transaction Rent 1/2 -2200
update transaction Gym Tue/4 -40 r'.*Planet Fitness.*'
```

### Auto Scheduling

Prefix repetition with `@` to mark as **auto**. Auto events shift off weekends
and US federal holidays to the prior business day.

```
update transaction PAYDAY @Fri 1500
```

Non-auto events always occur on the scheduled date.

## Exceptions

Exceptions override a single occurrence amount:

```
update exception PAYDAY 02-14-2026 1750
```

## Projections

```
run 6m
run 90d 03-01-2026
```

The run output includes:
- Event table (with monthly boundaries)
- Duration summary
- Chokepoints table (for long runs)
- Dotchart of balances

Dotchart is provided by the separate `dotchart` utility:
```
https://github.com/ddoxey/dotchart
```

## Totals

```
totals 3m
```

Totals show category sums and a net total line.

## Notes

- Cache data is stored in a user-specific cache directory.
- CSV headers are configurable so the app can adapt to format changes.
- Theme settings are user-editable in `budget_theme.json`.

## History

This project is a C++ rewrite of the original Budgeting program:
https://github.com/ddoxey/budgeting
