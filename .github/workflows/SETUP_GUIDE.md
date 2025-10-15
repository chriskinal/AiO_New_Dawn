# Quick Setup Guide for PR AI Code Review

## 1-Minute Setup

### Step 1: Get an Anthropic API Key

1. Go to https://console.anthropic.com/
2. Sign up or log in
3. Navigate to "API Keys"
4. Click "Create Key"
5. Copy the API key (starts with `sk-ant-`)

### Step 2: Add API Key to GitHub

1. Go to your repository: https://github.com/chriskinal/AiO_New_Dawn
2. Click `Settings` (repository settings, not your account)
3. In the left sidebar, click `Secrets and variables` → `Actions`
4. Click `New repository secret`
5. Name: `ANTHROPIC_API_KEY`
6. Value: Paste your API key from Step 1
7. Click `Add secret`

### Step 3: Done! 🎉

That's it! The workflow will now automatically run on all new pull requests.

## Testing the Workflow

### Option 1: Create a Test PR

1. Create a new branch with a small change
2. Open a pull request to `main`
3. Watch the "Actions" tab for the AI review to run
4. The review will be posted as a comment on the PR

### Option 2: Wait for Next PR

The workflow will automatically run on the next pull request opened against `main`, `master`, or `staging`.

## What Happens Next

When someone opens a PR:

1. GitHub Actions triggers the workflow
2. Workflow reads your code changes and architecture docs
3. Sends everything to Claude Sonnet 4.5 for analysis
4. Posts a detailed review as a PR comment

Review covers:
- ✅ Architectural compliance
- ✅ Code style matching
- ✅ Logic flaws and bugs
- ✅ Real-time timing issues
- ✅ Hardware safety

## Cost Estimate

- Per review: ~$0.10-0.50 USD
- Depends on: PR size, number of files, review depth
- Claude Sonnet 4.5 pricing: https://www.anthropic.com/pricing

For a typical firmware project with 5-10 PRs/month:
- Monthly cost: ~$2-5 USD

## Troubleshooting

### Review doesn't run?

Check that:
- GitHub Actions is enabled (Settings → Actions → General)
- PR targets `main`, `master`, or `staging`
- Workflow file is in `.github/workflows/pr-ai-review.yml`

### "ANTHROPIC_API_KEY not configured" error?

- The secret wasn't added correctly
- Double-check spelling: `ANTHROPIC_API_KEY`
- Make sure it's a repository secret, not an environment secret

### Review is too strict/lenient?

- Edit `.github/workflows/pr-ai-review.yml`
- Modify the review prompt under "Prepare review prompt" step
- Adjust severity guidelines or focus areas

## Advanced Configuration

See [README.md](README.md) for:
- Customizing review focus
- Excluding certain files
- Using different AI models
- Adjusting review criteria

## Support

- Workflow issues: Check GitHub Actions logs
- API issues: Check Anthropic status page
- Questions: Open an issue in the repository

---

**Need help?** See the full [README.md](README.md) documentation or [example reviews](EXAMPLE_REVIEW.md).
