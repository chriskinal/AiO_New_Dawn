# PR AI Code Review Workflow

## Overview

This repository uses an automated AI code review workflow powered by Claude Sonnet 4.5 to review pull requests. The workflow analyzes code changes for:

- **Architectural Compliance**: Ensures changes follow AiO New Dawn architecture patterns
- **Code Style Matching**: Verifies code matches the project's C++ embedded systems style  
- **Logic Flaws**: Identifies potential bugs, race conditions, or incorrect implementations
- **Real-Time Constraints**: Checks for timing issues affecting critical loops
- **Hardware Safety**: Verifies pin ownership, resource conflicts, and proper abstraction usage

## Setup

### 1. Configure Anthropic API Key

The workflow requires an Anthropic API key to access Claude Sonnet 4.5:

1. Go to your repository settings: `Settings` → `Secrets and variables` → `Actions`
2. Click `New repository secret`
3. Name: `ANTHROPIC_API_KEY`
4. Value: Your Anthropic API key from https://console.anthropic.com/
5. Click `Add secret`

### 2. Enable GitHub Actions

Ensure GitHub Actions is enabled for your repository:

1. Go to `Settings` → `Actions` → `General`
2. Under "Actions permissions", select "Allow all actions and reusable workflows"
3. Under "Workflow permissions", ensure "Read and write permissions" is selected
4. Click `Save`

## How It Works

### Workflow Trigger

The AI review workflow automatically runs when:

- A pull request is opened
- New commits are pushed to an existing pull request  
- A pull request is reopened

It only runs for PRs targeting the `main`, `master`, or `staging` branches.

### Review Process

1. **Fetch Changes**: The workflow checks out the PR code and compares it to the base branch
2. **Load Context**: Reads project documentation including:
   - `CLAUDE.md` - Project-specific guidelines
   - `docs/ARCHITECTURE_OVERVIEW.md` - Architecture patterns
   - `.agent-os/standards/code-style.md` - Code style rules
   - `.agent-os/standards/best-practices.md` - Best practices
3. **Generate Diff**: Creates a complete diff of all changes
4. **AI Analysis**: Sends the diff and context to Claude Sonnet 4.5 for review
5. **Post Results**: Adds a comment to the PR with the review findings

### Review Output

The AI reviewer provides structured feedback categorized by severity:

- **CRITICAL**: Must fix before merging (e.g., breaking changes, security issues, race conditions)
- **IMPORTANT**: Should fix (e.g., style violations, missing error handling)
- **SUGGESTION**: Consider improving (e.g., clarity, refactoring opportunities)

Each issue includes:
- File and line number
- Clear description of the problem
- Specific recommendation  
- Rationale explaining why it matters

## Review Criteria

### Architectural Compliance

The reviewer checks that code follows AiO New Dawn patterns:

- **Module Registration**: Proper PGN registration (never register for PGN 200/202)
- **Hardware Management**: Uses HardwareManager, PinOwnershipManager, SharedResourceManager
- **Communication Flow**: Follows UDP → AsyncUDPHandler → PGNProcessor → Handlers pattern
- **Timing Constraints**: Respects 100Hz autosteer and 10Hz sensor update rates

### Code Style

The reviewer enforces project C++ conventions:

- Pin modes: `INPUT_DISABLE` for analog pins (not `INPUT`)
- Naming conventions for classes, methods, variables
- Proper use of motor driver interface patterns
- Consistent error handling and logging via EventLogger

### Logic & Safety

The reviewer identifies potential issues:

- Race conditions in multi-threaded code
- Resource conflicts (PWM timers, ADC, I2C, pins)
- Memory management errors
- Hardware initialization order
- Network protocol compliance

## Limitations

### What the AI Reviewer Can Do

- ✅ Identify architectural violations
- ✅ Spot common bugs and anti-patterns  
- ✅ Check code style consistency
- ✅ Suggest improvements based on project standards
- ✅ Explain the rationale behind recommendations

### What the AI Reviewer Cannot Do

- ❌ Run tests or verify functionality
- ❌ Guarantee code is bug-free
- ❌ Replace human code review
- ❌ Test hardware interactions
- ❌ Verify real-time performance

**Important**: The AI review is a helpful tool but should not replace careful human review, especially for critical changes affecting safety, timing, or hardware control.

## Customization

### Adjusting Review Focus

Edit `.github/workflows/pr-ai-review.yml` to customize the review prompt:

- Modify the "Review Guidelines" section to emphasize different concerns
- Add project-specific checks
- Adjust severity thresholds
- Include additional documentation files in context

### Changing the AI Model

To use a different Claude model, update the `model` parameter in the API call:

```yaml
"model": "claude-sonnet-4-20250514",  # Claude Sonnet 4.5
# or
"model": "claude-opus-4-20250514",    # Claude Opus 4 (more thorough, slower)
```

### Excluding Files

To exclude certain files from review, modify the `Get changed files` step to filter the diff:

```bash
git diff --name-only origin/${{ github.event.pull_request.base.ref }}...${{ github.event.pull_request.head.sha }} | \
  grep -v "lib/ArduinoJson" | \
  grep -v "^docs/" \
  > changed_files.txt
```

## Troubleshooting

### "ANTHROPIC_API_KEY not configured"

Add your Anthropic API key to repository secrets (see Setup above).

### "Claude API Error: rate_limit_error"

You've exceeded your Anthropic API rate limits. Wait a few minutes or upgrade your plan.

### Workflow doesn't run

Check that:
- GitHub Actions is enabled for your repository
- The PR targets `main`, `master`, or `staging` branch
- Workflow file is in `.github/workflows/` directory
- YAML syntax is valid

### Review is too strict/lenient

Adjust the review guidelines in the workflow prompt to change focus and severity thresholds.

## Cost Considerations

Each review costs approximately $0.10-0.50 depending on:
- Size of the diff
- Number of context files  
- Length of review response

For high-volume repositories, consider:
- Limiting reviews to certain file types
- Only reviewing files matching specific patterns
- Using Claude Sonnet instead of Opus for cost savings
- Setting a review size limit

## Version History

- **v1.0** (2025-10-15): Initial implementation with Claude Sonnet 4.5
  - Architectural compliance checking
  - Code style verification
  - Logic flaw detection
  - Real-time constraint analysis
  - Hardware safety validation
