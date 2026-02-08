#include "help.h"

namespace budget::cli {

const std::unordered_map<std::string, HelpTopic>& help_topics() {
  static const std::unordered_map<std::string, HelpTopic> topics = {
      {"headers",
       {"Show header mappings",
        "Usage: headers\n\nShows the current header mapping configuration."}},
      {"clear",
       {"Clear the screen", "Usage: clear\n\nClears the terminal display."}},
      {"themeconfig",
       {"Show loaded theme config",
        "Usage: themeconfig\n\nDisplays the currently loaded theme "
        "configuration values."}},
      {"update header",
       {"Update header mapping",
        "Usage: update header <internal> <header>\n\nMaps an internal field "
        "name (e.g., transaction_date) to a CSV header."}},
      {"del header",
       {"Delete header mapping",
        "Usage: del header <internal>\n\nRemoves the header mapping for the "
        "given internal field."}},
      {"profile",
       {"Show or set profile",
        "Usage: profile [<name>]\n\nWithout a name, shows the active profile. "
        "With a name, switches active profile."}},
      {"profiles",
       {"List profiles",
        "Usage: profiles\n\nShows all profiles and balances."}},
      {"balance",
       {"Show or set balance",
        "Usage: balance [<amount>|+=<amount>|-=<amount>]\n\nShows balance, or "
        "sets/increments/decrements it."}},
      {"update profile",
       {"Create/update profile",
        "Usage: update profile <name> <description> [<balance>]\n\nCreates or "
        "updates a profile description. Description may be quoted to include "
        "spaces. If a balance is provided, it updates the profile balance."}},
      {"update transaction",
       {"Create/update transaction",
        "Usage: update transaction <cat> <when>[/<repeat>] <amount> [<desc>] "
        "[<amount>]\n\nDefines a recurring transaction type with optional "
        "regex matchers. Prefix the repetition with '@' to mark it as auto "
        "(e.g., @1, @Fri/2)."}},
      {"update exception",
       {"Create/update exception",
        "Usage: update exception <cat> <mm-dd-yyyy> <amount>\n\nOverrides the "
        "amount for a single occurrence date."}},
      {"update last",
       {"Update last occurrence",
        "Usage: update last <cat> <mm-dd-yyyy>\n\nSets last occurrence date "
        "for a category."}},
      {"run",
       {"Run projection",
        "Usage: run <number>(d|m|y) [<start-date>]\n\nProjects future events "
        "and shows balances."}},
      {"totals",
       {"Show totals",
        "Usage: totals <number>(d|m|y)\n\nShows total amounts per category for "
        "the specified duration."}},
      {"trans",
       {"List transaction types",
        "Usage: trans | transactions\n\nShows configured transaction types."}},
      {"cats",
       {"List categories",
        "Usage: cats\n\nShows categories in a multi-column table."}},
      {"exceptions",
       {"List exceptions",
        "Usage: exceptions\n\nShows configured exceptions."}},
      {"themes",
       {"List/manage themes",
        "Usage: themes\n       themes randomize [category]\n       themes "
        "rotate\n       themes reset\n       themes show default\n\nLists or "
        "modifies category themes."}},
      {"update theme",
       {"Update theme",
        "Usage: update theme <cat|default> <fg> <bg> <style>\n\nSets theme "
        "colors and style for a category or default."}},
      {"del theme",
       {"Delete theme",
        "Usage: del theme <cat|*> [-f]\n\nDeletes a theme or all themes (with "
        "confirmation)."}},
      {"lasts",
       {"List last occurrences",
        "Usage: lasts\n\nShows last occurrence per category."}},
      {"status",
       {"Show status",
        "Usage: status\n\nShows summary of configuration and paths."}},
      {"save",
       {"Save configuration",
        "Usage: save\n\nSave is implicit; updates persist immediately."}},
      {"reload",
       {"Reload cached data",
        "Usage: reload\n\nReloads cache data and reprocesses the latest "
        "transactions CSV."}},
      {"copy",
       {"Copy profile",
        "Usage: copy profile <from> <to>\n\nCopies cache data from one "
        "profile to another and creates a new profile entry."}},
      {"del profile",
       {"Delete profile",
        "Usage: del profile <name>\n\nDeletes a profile and its cached data."}},
      {"del exception",
       {"Delete exception",
        "Usage: del exception <cat> <mm-dd-yyyy|*>\n\nDeletes one or all "
        "exceptions for a category."}},
      {"del transaction",
       {"Delete transaction",
        "Usage: del transaction <cat>\n\nDeletes a transaction type (and "
        "related exceptions/themes)."}},
      {"del last",
       {"Delete last occurrence",
        "Usage: del last <cat|*>\n\nDeletes one or all last occurrence "
        "records."}}};
  return topics;
}

}  // namespace budget::cli
