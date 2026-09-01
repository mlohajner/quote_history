# quote_history 📜🚀

> **Why copy-paste when your terminal can read between the lines?**

`quote_history` is a lightweight, non-intrusive protocol and Shell enhancement that introduces a **secondary history stream** to your terminal. While `.bash_history` tracks what you *type*, `quote_history` automatically extracts what you *see* (code blocks, command examples, and quoted tips) and maps them directly to your keyboard.

Pressing `ALT+UP` / `ALT+DOWN` cycles through the high-value snippets extracted from your commands' outputs, completely eliminating mouse highlighting and copy-pasting.

---

## 💡 The Philosophy & The "Standard"

Modern terminal usage has shifted. We no longer just run scripts; we interact with rich data sources: **AI CLI tools (LLMs), `man` pages, `cheat.sh`, curl responses, and compilers.** These tools constantly output highly specific code snippets, wrapped in backticks (`` ` ``) or quotes (`"`, `'`).

Instead of reaching for the mouse or retyping, `quote_history` treats every quoted string in a command's stdout as a **potential next command**. 

### The Core Protocol
For this to become a seamless ecosystem standard, the pipeline relies on two components:
1. **The Context Parser:** Reads `stdout`, extracts matches via Regex, and appends unique tokens to `~/.quote_history`.
2. **The Readline Widget:** Binds `ALT+UP` / `ALT+DOWN` to instantly inject the history into your active prompt buffer.

---

## 🔥 Features

* ⚡ **Zero Friction:** No mouse, no clipboard managers. Just `ALT+UP`.
* 🧠 **Context-Aware:** Captures only the executable actions or terms you actually care about.
* 🧹 **Auto-Deduplicated:** Keeps your history clean and chronologically relevant.
* 🛠️ **Universal Interoperability:** Easily pipes into `man`, `curl`, custom scripts, or LLM CLI outputs.

---
