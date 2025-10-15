# GitHub Actions Workflows

This directory contains automated workflows for the AiO New Dawn project.

## AI PR Review

The `ai-pr-review.yml` workflow provides automated code reviews using Claude AI (Anthropic) for all pull requests.

### What it does

When a pull request is opened, updated, or reopened, Claude AI automatically:

1. **Reviews the code changes** against project standards
2. **Checks architectural compliance** with the modular design principles
3. **Verifies code style** (naming conventions, indentation, comments)
4. **Identifies potential issues** (bugs, memory leaks, race conditions)
5. **Validates project-specific patterns** (PGN handling, motor drivers, etc.)
6. **Posts a detailed review** as a comment on the PR

### Review Focus Areas

The AI review checks:

- **Architectural Standards**: Modular design, event-driven communication, hardware abstraction
- **Code Style**: snake_case variables, PascalCase classes, proper indentation (2 spaces)
- **Best Practices**: KISS principle, DRY, single responsibility
- **C++ Safety**: Memory management, null checks, buffer overflows
- **Real-Time Concerns**: Timing-critical code (100Hz autosteer, 10Hz sensors)
- **Project Patterns**: PGN message handling, pin management, version updates

### Setup Instructions

To enable AI PR reviews, you need to add an Anthropic API key to your repository secrets:

1. **Get an Anthropic API Key**:
   - Go to https://console.anthropic.com/
   - Sign in or create an account
   - Navigate to API Keys
   - Create a new API key

2. **Add to GitHub Secrets**:
   - Go to your repository settings
   - Navigate to Secrets and variables → Actions
   - Click "New repository secret"
   - Name: `ANTHROPIC_API_KEY`
   - Value: Your Anthropic API key
   - Click "Add secret"

3. **Verify Setup**:
   - Open a test pull request
   - The workflow should trigger automatically
   - Claude AI will post a review comment within a few minutes

### Review Quality

The AI reviewer provides:

- ✅ **Constructive feedback** - Explains WHY changes are beneficial
- ✅ **Specific suggestions** - Includes code examples when helpful
- ✅ **Prioritized issues** - Critical problems highlighted over minor style points
- ✅ **Positive reinforcement** - Acknowledges good practices
- ✅ **Collaborative tone** - Suggestions, not demands

### Limitations

- **Not a replacement for human review** - Use AI feedback as a starting point
- **Token limits** - Very large PRs may have truncated diffs
- **Context awareness** - AI may not understand all project-specific nuances
- **False positives** - Some suggestions may not apply in context

### Cost Considerations

Each PR review costs approximately $0.10-0.50 depending on the size of the changes (using Claude 3.5 Sonnet). Monitor your Anthropic API usage to stay within your budget.

### Customization

To modify the review criteria, edit the `review_prompt` section in `ai-pr-review.yml`. You can:

- Add project-specific patterns to check
- Adjust the tone and style of feedback
- Focus on specific file types or areas
- Add custom validation rules

### Troubleshooting

**Workflow doesn't run:**
- Check that `ANTHROPIC_API_KEY` secret is set correctly
- Verify the workflow has required permissions (pull-requests: write)
- Check Actions tab for error messages

**Review quality issues:**
- Adjust the prompt to be more specific
- Add examples of good/bad patterns
- Increase max_tokens for longer responses

**API rate limits:**
- Anthropic has rate limits - space out PRs if hitting limits
- Consider upgrading your Anthropic plan if needed

### Support

For issues with the AI review workflow:
1. Check the Actions tab for detailed logs
2. Review this documentation
3. Open an issue in the repository
4. Consult Anthropic documentation: https://docs.anthropic.com/

---

*The AI review is designed to be helpful, not prescriptive. Always use your judgment when addressing feedback.*
