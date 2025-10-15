# AI PR Review Setup Guide

This guide is for repository maintainers to set up the Claude AI PR review system.

## Quick Setup (5 minutes)

### Step 1: Get Anthropic API Key

1. Visit https://console.anthropic.com/
2. Sign in or create an account
3. Navigate to "API Keys" in the left sidebar
4. Click "Create Key"
5. Give it a name (e.g., "GitHub Actions - AiO New Dawn")
6. Copy the API key (you'll only see it once!)

### Step 2: Add Secret to GitHub Repository

1. Go to your GitHub repository
2. Click "Settings" tab
3. In the left sidebar, click "Secrets and variables" → "Actions"
4. Click "New repository secret"
5. Enter:
   - **Name**: `ANTHROPIC_API_KEY`
   - **Secret**: Paste your Anthropic API key
6. Click "Add secret"

### Step 3: Verify Setup

1. The workflow is already in place: `.github/workflows/ai-pr-review.yml`
2. Open a test PR to verify it works
3. Claude AI should post a review comment within 2-5 minutes

## How It Works

When a PR is opened, updated, or reopened:

1. **GitHub Actions triggers** the workflow
2. **Code changes are extracted** (diff and file list)
3. **Claude AI analyzes** the changes using GPT-3.5-sonnet
4. **Review is posted** as a comment on the PR
5. **Contributors see feedback** immediately

## Review Criteria

The AI checks for:

### 1. Architectural Compliance
- Modular design with clear interfaces
- Event-driven communication (PGN messages)
- Hardware abstraction and resource management
- Timing-critical code patterns (100Hz autosteer, 10Hz sensors)

### 2. Code Style
- Naming conventions (snake_case, PascalCase, UPPER_SNAKE_CASE)
- Indentation (2 spaces)
- Comment quality (explain "why" not "what")

### 3. Best Practices
- KISS (Keep It Simple)
- DRY (Don't Repeat Yourself)
- Single Responsibility Principle

### 4. C++ Safety
- Memory management
- Null pointer checks
- Buffer overflow prevention
- Race condition detection

### 5. Project-Specific
- PGN message handling patterns
- Pin management (INPUT_DISABLE for analog)
- Version numbering updates
- Motor driver interfaces

## Cost Management

Each review costs approximately $0.10-0.50 depending on PR size:

- **Small PR** (<100 lines): ~$0.10
- **Medium PR** (100-500 lines): ~$0.25
- **Large PR** (500+ lines): ~$0.50

**Monthly estimates:**
- 10 PRs/month: ~$2.50
- 50 PRs/month: ~$12.50
- 100 PRs/month: ~$25.00

Monitor usage at https://console.anthropic.com/

## Customization

### Modify Review Criteria

Edit `.github/workflows/ai-pr-review.yml` and update the `review_prompt` section:

```python
review_prompt = f"""You are an expert code reviewer...

[Add or modify criteria here]
"""
```

### Change AI Model

Update the model in the API call:

```python
model="claude-3-5-sonnet-20241022",  # Current model
# model="claude-3-opus-20240229",    # More capable, higher cost
# model="claude-3-haiku-20240307",   # Faster, lower cost
```

### Adjust Review Length

Modify `max_tokens` for longer/shorter reviews:

```python
max_tokens=4096,  # Current setting
# max_tokens=2048,  # Shorter reviews
# max_tokens=8192,  # Longer, more detailed reviews
```

## Troubleshooting

### Workflow doesn't run

**Check:**
- Is `ANTHROPIC_API_KEY` secret set correctly?
- Are permissions correct in workflow file?
- Check Actions tab for error messages

**Fix:**
```bash
# Verify secret exists
Settings → Secrets and variables → Actions → ANTHROPIC_API_KEY should be listed

# Check workflow permissions
.github/workflows/ai-pr-review.yml should have:
permissions:
  contents: read
  pull-requests: write
  issues: write
```

### API key errors

**Error message**: "Authentication error" or "Invalid API key"

**Fix:**
1. Regenerate API key at https://console.anthropic.com/
2. Update GitHub secret
3. Retry the workflow

### Review not posted

**Check:**
- Actions tab → workflow run → step logs
- Look for Python errors
- Check API response messages

**Common issues:**
- Rate limiting (wait and retry)
- Invalid API key (regenerate)
- Network issues (transient, retry)

### Large PR truncation

PRs with diffs >50,000 characters are truncated automatically.

**Solutions:**
1. Break large PRs into smaller ones (recommended)
2. Increase truncation limit in workflow
3. Review large PRs manually

## Security Considerations

### API Key Security

✅ **Do:**
- Store API key only in GitHub Secrets
- Use separate keys for different projects
- Rotate keys periodically
- Revoke keys if compromised

❌ **Don't:**
- Commit API keys to code
- Share API keys publicly
- Use personal API keys for shared projects

### Review Privacy

- Reviews are posted as public comments on PRs
- Sensitive code in PRs will be sent to Anthropic API
- Don't review PRs containing secrets or sensitive data

## Monitoring and Maintenance

### Monthly Checklist

- [ ] Check Anthropic API usage and costs
- [ ] Review AI feedback quality
- [ ] Update review criteria if needed
- [ ] Check for workflow failures

### Quarterly Review

- [ ] Evaluate ROI (time saved vs. cost)
- [ ] Update to latest Claude model if available
- [ ] Gather contributor feedback
- [ ] Adjust review criteria based on common issues

## Support

### Documentation
- Workflow README: `.github/workflows/README.md`
- Contributing guide: `CONTRIBUTING.md`
- This setup guide

### External Resources
- Anthropic docs: https://docs.anthropic.com/
- GitHub Actions: https://docs.github.com/en/actions
- Claude models: https://docs.anthropic.com/claude/docs/models-overview

### Getting Help
- Check workflow logs in Actions tab
- Review Anthropic API status page
- Open issue in repository
- Contact Anthropic support (for API issues)

## Success Metrics

Track these to measure effectiveness:

- **Review coverage**: % of PRs getting AI review
- **Issue detection**: Bugs caught before merge
- **Time to review**: Reduced human review time
- **Code quality**: Fewer issues in merged code
- **Contributor satisfaction**: Feedback from team

## Next Steps

1. ✅ Complete setup (API key and secret)
2. ✅ Test with a sample PR
3. ✅ Share with contributors
4. ✅ Monitor first month of usage
5. ✅ Adjust criteria based on feedback
6. ✅ Document project-specific patterns

---

*The AI review system is designed to augment, not replace, human code review. Use it as a tool to improve code quality while maintaining a collaborative and welcoming environment.*
