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

For file entries such as the two latest CSVs and header map, `status` also
shows each file's last modification timestamp:

- `transactions-3.csv (03-21-2025 15:18)`

The two most recently modified CSV files in the download directory are merged
for history matching. This allows last occurrences to be sourced from separate
bank-account and credit-card exports. `BUDGET_CSV` remains an explicit
single-file override.

Header mapping is stored as plain JSON and can be edited interactively:

- `headers` — show mappings
- `update header <internal> <header>` — add/update mapping
- `del header <internal>` — remove mapping

Theme config is stored in `budget_theme.json` (in the config dir):

- `themeconfig` — show loaded values
- `update theme <cat|default> <fg> <bg> <style>`
- `themes randomize [category]`
- `themes rotate`
- `themes reset`
- `themes show default`
- `del theme <cat|*> [-f]`

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
- `update last <cat> <mm-dd-yyyy>`
- `del last <cat|*>`
- `run <number>(d|m|y) [<start-date>]` — projections
- `totals <number>(d|m|y)` — totals per category
- `del exception <cat> <mm-dd-yyyy|*>`
- `del transaction <cat>`
- `del profile <name>`
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
update transaction Mortgage 1,15 -1200
update transaction Spending 2xWeek+2 -60
update transaction Utilities 2xMonth -150
update transaction Gym Tue/4 -40 r'.*Planet Fitness.*'
update transaction Registration Jun-24 -120
```

Supported repetition forms:

- Explicit weekday:
  `Fri`, `Tue/2`
- Explicit day-of-month:
  `1`, `1,15`, `1/2`, `1,15/2`
- Evenly distributed counted schedule:
  `2xWeek`, `2xWeek+2`, `2xMonth`, `2xMonth+1`
- Annual calendar date:
  `Jun-24`, `@Jun-24`

For monthly schedules, comma-separated days mean exact dates within each active
month. For example, `1,15` schedules the transaction on the 1st and 15th.

Counted schedules spread occurrences as evenly as possible across the period:

- `2xWeek` maps to `Sun, Thu`
- `2xWeek+2` rotates that to `Tue, Sat`
- `2xMonth` starts on day 1 and spreads the remaining occurrences across the
  month
- `2xMonth+1` shifts that pattern forward one day, so in a 30-day month it
  becomes `2,16`

In the `transactions` table, counted schedules are shown in readable form. For
example, `2xMonth+1` is displayed as `Twice a month (starting on 2nd)`.

### Auto Scheduling

Prefix repetition with `@` to mark as **auto**. Auto events shift off weekends
and US federal holidays to the prior business day.

```
update transaction PAYDAY @Fri 1500
```

Non-auto events always occur on the scheduled date.

Annual month names are case-insensitive. `Feb-29` occurs only in leap years.

## Exceptions

Exceptions override a single occurrence amount:

```
update exception PAYDAY 02-14-2026 1750
```

If an exception date is today or in the past, it becomes the source of the
last occurrence for that category.

## Last Occurrences

`lasts` shows the current last occurrence date and source for each category.

- `history` — observed in CSV history
- `computed` — inferred from the repetition rule
- `exception` — taken from an exception date

If a category appears overdue, the category name is shown with `*`.

- `history*` categories are overdue relative to observed history
- `computed*` categories are inferred from repetition and appear overdue

The `lasts` table also uses category theme colors when configured.

## Projections

```
run 6m
run 90d 03-01-2026
```

The run output includes:
- Event table (with monthly boundaries)
- Present-day catch-up rows for overdue categories, marked with `*`
- Duration summary
- Chokepoints table (for long runs)
- Dotchart of balances

For overdue categories, the catch-up row on today's date uses the sum of all
missed occurrences through today. This applies to both stale history-backed
categories and overdue computed categories.

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
