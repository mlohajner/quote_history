# Quote_history 📜🚀

> **Why copy-paste when your terminal can read between the lines?**

`quote_history` is a lightweight, non-intrusive Shell enhancement that introduces a **secondary history stream** to your terminal. While `.bash_history` tracks what you *type*, `quote_history` automatically extracts what you *see* (code blocks, command examples, and quoted tips) and maps them directly to your keyboard.

Pressing `ALT+UP` / `ALT+DOWN` cycles through the high-value snippets extracted from your commands' outputs, completely eliminating mouse highlighting and copy-pasting.

---

## 💡 The Philosophy & The "Standard"

Modern terminal usage has shifted. We no longer just run scripts; we interact with rich data sources: **AI CLI tools (LLMs), `man` pages, `cheat.sh`, curl responses, and compilers.** These tools constantly output highly specific code snippets, wrapped in backticks (`` ` ``) or quotes (`"`, `'`).

Instead of reaching for the mouse or retyping, `quote_history` treats every quoted string in a command's stdout as a **potential next command**. 

### The Core
For this to become a seamless ecosystem standard, the pipeline relies on two components:
1. **qh_extension.sh** place this to your ~/.local/bin/, this is your redirector and hostory browser/consumer
2. **qt_extractor** C regex parser -> extracts quoted content into ~/.quote_history, should be executable in $PATH
   current extraction method looks for quoted text, but I see no reason why your extractor couldn't be more elaborated
3. **.bashrc** add one line to your `.bashrc` to enable the extension `. ~/.local/bin/qh_extension.sh`

---

## 🔥 Features

- ⚡ **Zero Friction:** No mouse, no clipboard managers. Just `ALT+UP`.
- 🧠 **Context-Aware:** Captures all quoted text, actions or terms you actually care about.
- 🛠 **Universal Interoperability:** Easily pipes into `man`, `curl`, custom scripts, or LLM CLI outputs.
- ❌ **On-the-Fly Purging (New!):** Instantly remove unwanted or sensitive snippets from your stream using `ALT+DELETE`.

---

## 🎹 Keyboard Shortcuts & Ergonomics

| Shortcut | Action | Description |
|---|---|---|
| **`ALT + UP`** | Browse Up | Cycle backward through high-value extracted snippets. |
| **`ALT + DOWN`** | Browse Down | Cycle forward through snippets or return to your original prompt. |
| **`ALT + DELETE`** | Purge Snippet | Permanently delete the currently viewed snippet from history in real-time. |

### Why `ALT + DELETE`?
Shortcuts with terminal ergonomics and safety in mind:
1. **Friction by Design:** The `Delete` key sits in a separate block on most keyboards. You will never accidentally trigger it while cycling with `ALT+UP/DOWN`.
2. **No Conflict with Editing:** If you want to modify a snippet before executing it, your muscle memory naturally targets `Backspace` or arrow keys. `ALT+DELETE` remains strictly reserved for permanent management.
3. **Privacy First:** If an LLM response or compiler output accidentally leaks a token, a password, or a massive wall of useless text into your stream, you can wipe it from your disk (`~/.quote_history`) instantly without opening a text editor.
