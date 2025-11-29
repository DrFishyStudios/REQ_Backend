# WorldServer CLI Commands Guide

## Overview
The WorldServer includes a simple command-line interface (CLI) for inspecting accounts and characters. This is useful for debugging, monitoring, and administrative tasks.

## Enabling CLI Mode

To start WorldServer with CLI enabled, use the `--cli` flag:

```bash
cd x64/Debug
.\REQ_WorldServer.exe --cli
```

The server will run in a background thread while the CLI runs on the main thread. You can enter commands interactively.

## Available Commands

### help
**Syntax:** `help` or `?`

**Description:** Display a list of available commands.

**Example:**
```
> help

=== WorldServer CLI Commands ===
  help, ?                  Show this help message
  list_accounts            List all accounts
  list_chars <accountId>   List all characters for an account
  show_char <characterId>  Show detailed character information
  quit, exit, q            Shutdown the server
===============================
```

---

### list_accounts
**Syntax:** `list_accounts`

**Description:** Lists all accounts from the account storage directory (`data/accounts/`).

**Output:** For each account, displays:
- `id` - Account ID
- `username` - Login username
- `display` - Display name
- `admin` - Admin flag (Y/N)
- `banned` - Banned flag (Y/N)

**Example:**
```
> list_accounts

[INFO] [world] Found 2 account(s):
  id=1 username=testuser display="Test User" admin=N banned=N
  id=2 username=admin display="Administrator" admin=Y banned=N
```

**Error Cases:**
- If `data/accounts/` directory is empty or doesn't exist, outputs "No accounts found"

---

### list_chars <accountId>
**Syntax:** `list_chars <accountId>`

**Description:** Lists all characters belonging to a specific account for the current world.

**Parameters:**
- `<accountId>` - Numeric account ID (from `list_accounts`)

**Output:** For each character, displays:
- `id` - Character ID
- `name` - Character name
- `race` - Character race (e.g., Human, Dwarf, HighElf)
- `class` - Character class (e.g., Warrior, Cleric, Wizard)
- `lvl` - Character level
- `zone` - Last zone ID
- `pos` - Last position (x, y, z)

**Example:**
```
> list_chars 1

[INFO] [world] Characters for accountId=1 (username=testuser):
  id=1 name=TestWarrior race=Human class=Warrior lvl=1 zone=10 pos=(0,0,0)
  id=2 name=Bethor race=Dwarf class=Cleric lvl=5 zone=10 pos=(100.5,200.0,10.0)
```

**Error Cases:**
- Invalid account ID: `[ERROR] [world] Account not found: id=999`
- No characters: `(no characters)`
- Non-numeric account ID: `[ERROR] [world] Usage: list_chars <accountId>`

---

### show_char <characterId>
**Syntax:** `show_char <characterId>`

**Description:** Display detailed information about a specific character.

**Parameters:**
- `<characterId>` - Numeric character ID (from `list_chars`)

**Output:** Comprehensive character sheet including:
- **Basic Info:** Character ID, Account ID, Name, Race, Class, Level, XP
- **World & Zone:** Home World, Last World, Last Zone
- **Position:** Current position (x, y, z) and heading (degrees)
- **Bind Point:** Bind World, Bind Zone, Bind Position
- **Vitals:** HP, Mana (current / max)
- **Stats:** STR, STA, AGI, DEX, WIS, INT, CHA

**Example:**
```
> show_char 1

=== Character Details ===
Character ID:     1
Account ID:       1
Name:             TestWarrior
Race:             Human
Class:            Warrior
Level:            1
XP:               0

Home World:       1
Last World:       1
Last Zone:        10

Position:         (0, 0, 0)
Heading:          0 degrees

Bind World:       1
Bind Zone:        10
Bind Position:    (0, 0, 0)

HP:               120 / 120
Mana:             0 / 0

Stats:
  STR: 80  STA: 80
  AGI: 75  DEX: 75
  WIS: 75  INT: 75
  CHA: 75
=========================
```

**Error Cases:**
- Invalid character ID: `[ERROR] [world] Character not found: id=999`
- Non-numeric character ID: `[ERROR] [world] Usage: show_char <characterId>`

---

### quit / exit / q
**Syntax:** `quit`, `exit`, or `q`

**Description:** Gracefully shutdown the WorldServer and exit the CLI.

**Example:**
```
> quit

[INFO] [world] CLI quit requested - shutting down server
[INFO] [world] WorldServer shutdown requested
```

The server will stop accepting new connections and the process will exit.

---

## Typical Workflows

### Debugging Login Issues
1. Start server with CLI: `.\REQ_WorldServer.exe --cli`
2. List accounts: `list_accounts`
3. Verify account exists and is not banned
4. If needed, manually edit account JSON in `data/accounts/`

### Inspecting Character Data
1. Find account: `list_accounts`
2. List characters: `list_chars 1`
3. View details: `show_char 1`
4. Verify position, stats, zone assignment

### Monitoring Position Persistence
1. Connect with TestClient and move around
2. In WorldServer CLI: `show_char 1`
3. Note current position
4. Move character again in TestClient
5. Wait for autosave (30s) or disconnect
6. Re-run: `show_char 1`
7. Verify position updated

### Testing Character Creation
1. Register new account via TestClient
2. Create character via TestClient
3. In CLI: `list_accounts` (find new account ID)
4. In CLI: `list_chars <newAccountId>`
5. In CLI: `show_char <newCharId>`
6. Verify stats, starting position, bind point

---

## Implementation Details

### Data Sources
- **Accounts:** Loaded from `data/accounts/*.json` via `AccountStore::loadAllAccounts()`
- **Characters:** Loaded from `data/characters/*.json` via `CharacterStore`
- **Filtering:** Characters filtered by `accountId` and current `worldId`

### Performance
- All commands perform file I/O (disk reads)
- **list_accounts:** O(n) scan of all account files
- **list_chars:** O(n) scan of all character files with filtering
- **show_char:** O(1) single file load by character ID

For production with large player bases, consider:
- Adding in-memory caching
- Indexing by account ID and character ID
- Database backend instead of JSON files

### Thread Safety
- CLI runs on main thread
- Server runs on background thread
- AccountStore and CharacterStore are shared
- **Current:** Not thread-safe (single-threaded file access assumed)
- **TODO:** Add mutex protection if concurrent access needed

### Error Handling
- Malformed JSON files are logged and skipped
- Missing accounts or characters return clear error messages
- Parse errors for command arguments show usage hints

---

## Logging

All CLI commands log their actions to the standard logger with `[world]` prefix:

**Info Logs:**
- Account/character loading results
- Command execution confirmations

**Error Logs:**
- Account/character not found
- Invalid command arguments
- File I/O errors

**Example Log Output:**
```
[INFO] [world] === WorldServer CLI ===
[INFO] [world] Type 'help' for available commands, 'quit' to exit
[INFO] [world] 
[INFO] [world] Found 2 account(s):
[INFO] [world] Characters for accountId=1 (username=testuser):
[ERROR] [world] Character not found: id=999
[INFO] [world] CLI quit requested - shutting down server
```

---

## Future Enhancements

### Planned Commands
- `ban_account <accountId>` - Ban/unban account
- `set_admin <accountId> <true|false>` - Toggle admin flag
- `delete_char <characterId>` - Delete character
- `teleport_char <characterId> <zoneId> <x> <y> <z>` - Teleport character
- `grant_xp <characterId> <amount>` - Grant experience points
- `set_level <characterId> <level>` - Set character level

### Planned Features
- Command history (up/down arrow keys)
- Tab completion for commands
- Colored output (errors in red, success in green)
- Pagination for long lists
- Search/filter for accounts and characters
- Export commands (dump to CSV/JSON)

### Planned Integrations
- Online player list (from active zone connections)
- Session status (show active sessions)
- Zone server status (show running zones)
- Performance metrics (uptime, message counts, etc.)

---

## Examples

### Example Session
```
$ .\REQ_WorldServer.exe --cli

[INFO] [world] WorldServer starting: worldId=1, worldName=CazicThule
[INFO] [world] Listening on 0.0.0.0:7778
[INFO] [world] 
[INFO] [world] === WorldServer CLI ===
[INFO] [world] Type 'help' for available commands, 'quit' to exit
[INFO] [world] 

> help

=== WorldServer CLI Commands ===
  help, ?                  Show this help message
  list_accounts            List all accounts
  list_chars <accountId>   List all characters for an account
  show_char <characterId>  Show detailed character information
  quit, exit, q            Shutdown the server
===============================

> list_accounts

[INFO] [world] Found 2 account(s):
  id=1 username=testuser display="Test User" admin=N banned=N
  id=2 username=admin display="Administrator" admin=Y banned=N

> list_chars 1

[INFO] [world] Characters for accountId=1 (username=testuser):
  id=1 name=TestWarrior race=Human class=Warrior lvl=1 zone=10 pos=(0,0,0)

> show_char 1

=== Character Details ===
Character ID:     1
Account ID:       1
Name:             TestWarrior
Race:             Human
Class:            Warrior
Level:            1
XP:               0

Home World:       1
Last World:       1
Last Zone:        10

Position:         (0, 0, 0)
Heading:          0 degrees

Bind World:       1
Bind Zone:        10
Bind Position:    (0, 0, 0)

HP:               120 / 120
Mana:             0 / 0

Stats:
  STR: 80  STA: 80
  AGI: 75  DEX: 75
  WIS: 75  INT: 75
  CHA: 75
=========================

> quit

[INFO] [world] CLI quit requested - shutting down server
[INFO] [world] WorldServer shutdown requested
```

---

## Troubleshooting

### CLI Not Starting
**Problem:** Server starts but CLI doesn't appear

**Solution:**
- Make sure you used `--cli` flag
- Check that stdin is not redirected
- Verify console is not running as a service (needs interactive terminal)

### Commands Not Recognized
**Problem:** Typing commands results in "Unknown command"

**Solution:**
- Check spelling (case-sensitive in some environments)
- Use `help` to see exact command names
- Ensure no leading/trailing whitespace

### Account/Character Not Found
**Problem:** Valid IDs return "not found"

**Solution:**
- Verify JSON files exist in `data/accounts/` or `data/characters/`
- Check file naming (must be `<id>.json`)
- Inspect logs for JSON parse errors
- Verify permissions (server can read files)

### Server Crashes on Command
**Problem:** Entering command causes server to crash

**Solution:**
- Check error logs for exception details
- Verify JSON files are not corrupted
- Ensure AccountStore and CharacterStore initialized correctly
- Report bug with command input and logs

---

## Security Considerations

### Password Hashes
- CLI never displays password hashes
- Account listing only shows usernames and flags
- Password hashes remain in JSON files only

### Admin Privileges
- CLI has full access to all data
- No authentication required (assumes trusted operator)
- **Production:** Restrict CLI to console/local access only
- **Production:** Consider adding admin password for CLI commands

### Data Modification
- Current CLI is read-only (no write commands)
- Future write commands should require confirmation
- Consider audit logging for administrative actions

---

## Performance Tips

### Large Player Bases
If you have thousands of accounts/characters:

1. **Use filtering:** `list_chars` instead of `show_char` for all
2. **Cache results:** Load once, query multiple times
3. **Dedicated tools:** Consider separate admin panel instead of CLI
4. **Database migration:** Move from JSON files to PostgreSQL/MySQL

### Production Deployment
- Disable CLI on production servers (remove `--cli` flag)
- Use monitoring/admin tools instead
- Reserve CLI for emergency debugging only
- Consider web-based admin panel

---

## Quick Reference

| Command | Arguments | Description |
|---|---|---|
| `help` | None | Show command list |
| `list_accounts` | None | List all accounts |
| `list_chars` | `<accountId>` | List characters for account |
| `show_char` | `<characterId>` | Show character details |
| `quit` | None | Shutdown server |

**Tip:** Commands are case-sensitive in some terminals. Use lowercase.
