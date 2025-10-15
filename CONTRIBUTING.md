# Contributing to AiO New Dawn

Thank you for considering contributing to AiO New Dawn! This document provides guidelines for contributing to the project.

## Pull Request Process

When you submit a pull request, it will automatically receive a code review from Claude AI. This helps ensure code quality and consistency with project standards.

### What the AI Reviews

The automated review checks your code against:

1. **Architectural Standards** - Modular design, event-driven patterns, hardware abstraction
2. **Code Style** - Naming conventions, indentation, comment quality
3. **Best Practices** - KISS, DRY, single responsibility
4. **Safety** - Memory management, null checks, race conditions
5. **Project Patterns** - PGN handling, timing-critical code, pin management

### How to Respond to AI Review

The AI review is meant to be **helpful**, not **prescriptive**. Here's how to use it effectively:

✅ **Do:**
- Read the feedback carefully and consider the suggestions
- Address critical issues (bugs, security, safety)
- Ask questions if feedback is unclear
- Use your judgment on minor style points
- Feel free to disagree with suggestions (explain why)

❌ **Don't:**
- Feel obligated to implement every suggestion
- Take feedback personally (it's automated!)
- Ignore all feedback without consideration
- Delay PR submission due to fear of AI review

### When AI Review Isn't Enough

AI reviews supplement but don't replace human review. Complex changes, architectural decisions, and feature design should still get human review from maintainers.

## Code Standards

### Architecture Principles

- **Modular Design**: Each subsystem should be self-contained with clear interfaces
- **Event-Driven**: Use PGN messages for inter-module communication
- **Real-Time Safe**: Critical loops run at consistent intervals (100Hz autosteer, 10Hz sensors)
- **Hardware Abstraction**: Use pin ownership and resource management to prevent conflicts

### Code Style

- **Naming**:
  - Variables and methods: `snake_case`
  - Classes: `PascalCase`
  - Constants: `UPPER_SNAKE_CASE`
- **Indentation**: 2 spaces (no tabs)
- **Comments**: Explain "why", not "what"
- **Single quotes** for strings (unless interpolation needed)

### Best Practices

- Keep code simple and readable
- Avoid over-engineering
- Don't repeat yourself (DRY)
- One responsibility per file/class
- Update documentation with code changes

### Project-Specific

- **PGN Messages**: Never register for PGN 200/202 - use broadcast callbacks instead
- **Pin Management**: Use `INPUT_DISABLE` for analog pins
- **Version Updates**: Update `lib/aio_system/Version.h` when changing functionality
- **Timing**: Be mindful of real-time requirements in autosteer and sensor code

## Testing

Before submitting a PR:

1. Build the firmware: `~/.platformio/penv/bin/pio run -e teensy41`
2. Test on hardware if possible
3. Check that existing functionality isn't broken
4. Document any behavioral changes

## Documentation

Update relevant documentation when making changes:

- Architecture changes → `docs/ARCHITECTURE_OVERVIEW.md`
- New features → `README.md` and relevant docs
- API changes → Inline code comments
- Hardware changes → Pin assignment documentation

## Getting Help

- Check existing documentation in `docs/`
- Review `CLAUDE.md` for architecture guidance
- Ask questions in PR comments
- Consult with maintainers on complex changes

## Version Updates

When making functional changes:

1. Open `lib/aio_system/Version.h`
2. Increment the patch version (e.g., 1.0.110 → 1.0.111)
3. Include version update in your PR

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

*Remember: The goal is to maintain a high-quality, maintainable codebase while fostering a welcoming and collaborative community. AI reviews are a tool to help, not a barrier to entry.*
