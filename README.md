# mysh - A Custom Unix Shell

A feature-rich custom Unix shell implementation written in C, supporting built-in commands, shell variables, pipes, background processes, and a TCP-based client-server chat system.

## Features

### Built-in Commands

| Command | Usage | Description |
|---------|-------|-------------|
| `echo` | `echo [args...]` | Prints arguments to stdout, separated by spaces |
| `cd` | `cd <path>` | Changes the current working directory. Supports relative paths, absolute paths, `.` (current), and `..` (parent) |
| `ls` | `ls [path] [--rec] [--d <depth>] [--f <substring>]` | Lists directory contents with optional recursion, depth limit, and filename filtering |
| `cat` | `cat [file]` | Displays file contents. Reads from stdin if no file is provided |
| `wc` | `wc [file]` | Counts words, characters, and newlines in a file or stdin |
| `ps` | `ps` | Lists all background processes spawned by the shell |
| `kill` | `kill <pid> [signal]` | Sends a signal to a process (default: SIGTERM) |
| `exit` | `exit` | Exits the shell gracefully |

### Shell Variables

Define and use custom variables within the shell session:

```bash
mysh$ greeting=hello
mysh$ echo $greeting
hello
mysh$ name=world
mysh$ echo $greeting $name
hello world
```

Variables are expanded at runtime using the `$` prefix.

### Pipes

Chain commands together using the pipe operator `|`:

```bash
mysh$ cat file.txt | wc
mysh$ echo hello world | cat
```

Multiple pipes are supported for complex command chains.

### Background Processes

Run commands in the background using `&`:

```bash
mysh$ long_running_command &
[1] 12345
mysh$ 
```

The shell displays the job number and PID, then returns to the prompt immediately.

### TCP Client-Server Chat System

The shell includes a built-in networking system for multi-client chat functionality:

| Command | Usage | Description |
|---------|-------|-------------|
| `start-server` | `start-server <port>` | Starts a chat server on the specified port |
| `close-server` | `close-server` | Shuts down the running server |
| `start-client` | `start-client <port> <host>` | Connects to a chat server as an interactive client |
| `send` | `send <port> <host> <message>` | Sends a single message to a server |

**Example - Starting a Server:**
```bash
mysh$ start-server 8080
```

**Example - Connecting as a Client:**
```bash
mysh$ start-client 8080 127.0.0.1
```

**Example - Sending a Message:**
```bash
mysh$ send 8080 127.0.0.1 Hello everyone!
```

Connected clients can check the number of active connections:
```bash
\connected
```

## Building

Compile the shell using the provided Makefile:

```bash
make
```

This produces the `mysh` executable with debug symbols and sanitizers enabled.

**Clean build artifacts:**
```bash
make clean
```

## Running

```bash
./mysh
```

You'll be greeted with the shell prompt:
```
mysh$ 
```

## Signal Handling

| Signal | Behavior |
|--------|----------|
| `Ctrl+C` (SIGINT) | Interrupts and redisplays the prompt (doesn't exit) |
| `Ctrl+D` (EOF) | Exits the shell gracefully |
| `SIGTERM` / `SIGHUP` | Clean shutdown with server cleanup |
| `SIGCHLD` | Automatically reaps completed background processes |

## ls Command Options

The `ls` command supports several flags for flexible directory listing:

- **`--rec`**: Enable recursive listing
- **`--d <depth>`**: Limit recursion depth (requires `--rec`)
- **`--f <substring>`**: Filter results to files containing the substring

**Examples:**
```bash
mysh$ ls                          # List current directory
mysh$ ls /home                    # List specific directory
mysh$ ls --rec                    # Recursive listing (unlimited depth)
mysh$ ls --rec --d 2              # Recursive with max depth of 2
mysh$ ls --f .txt                 # Show only files containing ".txt"
mysh$ ls --rec --d 3 --f test     # Combined options
```

## Project Structure

```
├── mysh.c              # Main shell loop and command processing
├── builtins.c/h        # Built-in command implementations (echo, cd, ls, etc.)
├── commands.c/h        # Server/client command implementations
├── client_manager.c/h  # TCP socket and client management
├── variables.c/h       # Shell variable storage and expansion
├── io_helpers.c/h      # Input/output utilities and tokenization
└── Makefile            # Build configuration
```

## Limitations

- Maximum input line length: 128 characters
- Maximum username length (chat): 10 characters
- Maximum chat message length: 129 characters
- Background process tracking: Up to 1000 PIDs

## Requirements

- GCC compiler
- POSIX-compliant system (Linux/macOS)
- Standard C libraries

## License

This project is provided for educational purposes.
