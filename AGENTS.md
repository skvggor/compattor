# compattor - Project Guidelines

## Testing

- Always follow TDD (Red, Green, Refactor): write a failing test first, make it pass with minimal code, then refactor. Never write implementation before the test.
- Unit tests must always achieve at least 80% coverage (statements, branches, functions, and lines) on the changed files.
- Always create tests focusing first on basic functionality and only then on edge cases.
- Never use `any` in tests. Always use proper types to keep test mocks type-safe.

## Code Quality

- Apply DRY (Don't Repeat Yourself): extract shared logic into reusable functions/classes. If you see duplication, refactor immediately.
- Apply SOLID principles: Single Responsibility (one class/function = one job), Open/Closed (extend via interfaces, not modification), Liskov Substitution (subtypes must be substitutable), Interface Segregation (prefer small, specific interfaces), Dependency Inversion (depend on abstractions, not concretions).
- Use Early Return: guard clauses at function top to handle errors/edge cases first, avoiding deep nesting. Keep the happy path as the main flow.
- Avoid abbreviations in code. Use full, descriptive names for variables, functions, parameters, and types (e.g. `document` instead of `doc`, `configuration` instead of `config`).
- No lint errors or warnings are acceptable. Always fix all warnings before committing.

## C++ Best Practices

- Prefer `const` and `constexpr` everywhere possible. Mark methods `const` if they don't modify state.
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers. Avoid `new`/`delete` in application code.
- Prefer value semantics over reference/pointer semantics when possible. Pass by value for small types, by `const&` for large objects.
- Use RAII for all resource management (files, memory, locks). No manual cleanup in destructors or error paths.
- Prefer `enum class` over plain `enum` for type safety.
- Use `std::optional` for optional values instead of sentinel values like `-1` or `nullptr`.
- Avoid `using namespace std;` in headers. In source files, prefer explicit `std::` prefix.
- Include guards via `#pragma once` for all headers.
- Organize includes: project headers first, then third-party, then standard library, separated by blank lines.
- Never use `malloc`/`free` in C++. Use `new`/`delete` only when smart pointers are not applicable.
- Prefer `<cstdint>` fixed-width types (`int32_t`, `uint64_t`) over `int`, `long` when size matters.
- Prefer `std::string_view` for read-only string parameters to avoid unnecessary copies.

## Git & Commits

- Never make commits without asking me, unless I explicitly tell you to do so.
- Never use prefixes in commits such as feat:, chore: etc.
- Always use the latest stable versions of new packages that we install in projects.

## Communication

- Do not create unnecessary comments. We always need to be as concise as possible.
- User should not have to ask you for your opinion explicitly. Always evaluate what the user is asking you to do, and voice your concerns before proceeding if you don't think it's a good idea. If possible, propose a better solution, but you can voice concerns even without one. This applies even to direct requests to revert or simplify. Still evaluate whether your original approach was better. The user may be missing important context. If there was a solid reasoning you suggested that approach, push back with reasoning instead of silently complying.
