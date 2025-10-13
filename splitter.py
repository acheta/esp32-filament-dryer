# This is a script to parse a concatenated file containing multiple files from AI and write them to the project structure
# file divider format: ===== FILE: path/to/file1 ===== so i.e. ===== FILE: src/main.cpp =====
# run command $ python .\splitter.py .\splitter_input.txt .
"""
Sample prompt:
Generate single artifact for the project files and start content of each file with:
===== FILE: path/to/file1 =====
"""


import os
import re
import sys
from pathlib import Path

# ANSI color codes
COLOR_RESET = "\033[0m"
COLOR_GREEN = "\033[92m"
COLOR_BLUE = "\033[94m"
COLOR_YELLOW = "\033[93m"
COLOR_RED = "\033[91m"
BOLD = "\033[1m"


def is_safe_path(base_dir: str, file_path: str) -> bool:
    """
    Verify that file_path resolves within base_dir (prevents path traversal).
    """
    base = Path(base_dir).resolve()
    target = (base / file_path).resolve()
    try:
        target.relative_to(base)
        return True
    except ValueError:
        return False


def parse_concatenated_file(content: str):
    """
    Parse concatenated content and return a list of (file_path, file_content) tuples.
    """
    pattern = re.compile(r"^===== FILE:\s*(.*?)\s*=====$", re.MULTILINE)
    matches = list(pattern.finditer(content))

    if not matches:
        return []

    files = []
    for i, match in enumerate(matches):
        file_path = match.group(1).strip()
        start = match.end()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(content)

        # Preserve original content without stripping newlines
        file_content = content[start:end]
        # Only strip leading newline if present
        if file_content.startswith('\n'):
            file_content = file_content[1:]

        files.append((file_path, file_content))

    return files


def list_files_status(files, output_dir):
    """
    Print list of files with NEW/EXISTING status.
    Returns True if any unsafe paths detected.
    """
    print(f"\n{BOLD}Files detected in input:{COLOR_RESET}\n")
    has_unsafe = False

    for file_path, _ in files:
        if not is_safe_path(output_dir, file_path):
            status = f"{COLOR_RED}UNSAFE PATH{COLOR_RESET}"
            has_unsafe = True
        else:
            full_path = os.path.join(output_dir, file_path)
            if os.path.exists(full_path):
                status = f"{COLOR_BLUE}EXISTING{COLOR_RESET}"
            else:
                status = f"{COLOR_GREEN}NEW{COLOR_RESET}"

        print(f" - {file_path} [{status}]")

    print()
    return has_unsafe


def write_files(files, output_dir):
    """
    Write parsed files to output directory with error handling.
    """
    written_files = []

    try:
        for file_path, content in files:
            # Security check
            if not is_safe_path(output_dir, file_path):
                raise ValueError(f"Unsafe path detected: {file_path}")

            full_path = os.path.join(output_dir, file_path)

            # Create directory only if needed
            dir_path = os.path.dirname(full_path)
            if dir_path:  # Only create if there's a directory component
                os.makedirs(dir_path, exist_ok=True)

            with open(full_path, "w", encoding="utf-8") as f:
                f.write(content)

            written_files.append(full_path)
            print(f"Written: {full_path}")

        print(f"\n{BOLD}{COLOR_GREEN}All files written successfully.{COLOR_RESET}")

    except Exception as e:
        print(f"\n{COLOR_RED}Error during write: {e}{COLOR_RESET}")
        print(f"{COLOR_YELLOW}Successfully written files:{COLOR_RESET}")
        for path in written_files:
            print(f" - {path}")
        sys.exit(1)


def main():
    if len(sys.argv) < 2:
        print("Usage: python split_project.py <concatenated_file> [output_dir]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "."

    # Validate output directory
    if not os.path.isdir(output_dir):
        print(f"{COLOR_RED}Error: Output directory does not exist: {output_dir}{COLOR_RESET}")
        sys.exit(1)

    # Read and parse file
    try:
        with open(input_file, "r", encoding="utf-8") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"{COLOR_RED}Error: Input file not found: {input_file}{COLOR_RESET}")
        sys.exit(1)
    except Exception as e:
        print(f"{COLOR_RED}Error reading file: {e}{COLOR_RESET}")
        sys.exit(1)

    files = parse_concatenated_file(content)

    if not files:
        print(f"{COLOR_YELLOW}No file sections found. Make sure your input uses '===== FILE: path =====' separators.{COLOR_RESET}")
        sys.exit(0)

    # List files and check for unsafe paths
    has_unsafe = list_files_status(files, output_dir)

    if has_unsafe:
        print(f"{COLOR_RED}Unsafe paths detected. Import cancelled for security.{COLOR_RESET}")
        sys.exit(1)

    # Ask for confirmation
    try:
        proceed = input(f"{BOLD}Proceed with import? [Y/n]: {COLOR_RESET}").strip().lower()
    except (KeyboardInterrupt, EOFError):
        print(f"\n{COLOR_YELLOW}Import cancelled.{COLOR_RESET}")
        sys.exit(0)

    if proceed in ("n", "no"):
        print(f"{COLOR_YELLOW}Import cancelled.{COLOR_RESET}")
        sys.exit(0)

    # Write files
    write_files(files, output_dir)


if __name__ == "__main__":
    main()