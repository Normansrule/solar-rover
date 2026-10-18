#!/usr/bin/env bash
# Put this folder on GitHub, or push new changes if it's already there.
# Works from any directory:
#
#   bash "$HOME/solar-rover/tools/publish.sh"            # public repo "solar-rover"
#   bash "$HOME/solar-rover/tools/publish.sh" --private  # private instead
#
# It installs git and the GitHub CLI if they're missing (asks for your sudo
# password), logs you in to GitHub once, makes the first commit with your
# private GitHub noreply email, creates the repo and pushes. Run it again any
# time to commit and push whatever changed.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="solar-rover"
VISIBILITY="--public"
for a in "$@"; do
    case "$a" in
        --private) VISIBILITY="--private" ;;
        --public)  VISIBILITY="--public" ;;
        --name=*)  NAME="${a#--name=}" ;;
        *) echo "unknown option: $a (use --private, --public or --name=NAME)"; exit 1 ;;
    esac
done
cd "$REPO_DIR"
echo "==> Project folder: $REPO_DIR"

if ! command -v git >/dev/null; then
    echo "==> Installing git"
    sudo apt-get update && sudo apt-get install -y git
fi

if ! command -v gh >/dev/null; then
    echo "==> Installing the GitHub CLI (gh) from GitHub's apt repository"
    command -v wget >/dev/null || { sudo apt-get update && sudo apt-get install -y wget; }
    sudo mkdir -p -m 755 /etc/apt/keyrings
    wget -qO- https://cli.github.com/packages/githubcli-archive-keyring.gpg \
        | sudo tee /etc/apt/keyrings/githubcli-archive-keyring.gpg >/dev/null
    sudo chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        | sudo tee /etc/apt/sources.list.d/github-cli.list >/dev/null
    sudo apt-get update && sudo apt-get install -y gh
fi

if ! gh auth status >/dev/null 2>&1; then
    echo "==> Logging in to GitHub: choose GitHub.com, HTTPS, Yes, Login with a web browser"
    gh auth login --hostname github.com --git-protocol https --web
fi
gh auth setup-git >/dev/null 2>&1 || true

USER_LOGIN="$(gh api user --jq .login)"
USER_ID="$(gh api user --jq .id)"
echo "==> GitHub account: $USER_LOGIN"

if [ ! -d .git ]; then
    echo "==> Starting a git repository"
    git init -b main >/dev/null
fi
# commit as your GitHub account, with the noreply email so your real address stays private
git config user.name >/dev/null 2>&1 || git config user.name "$USER_LOGIN"
git config user.email >/dev/null 2>&1 || git config user.email "${USER_ID}+${USER_LOGIN}@users.noreply.github.com"

git add -A
if git diff --cached --quiet; then
    echo "==> Nothing new to commit"
else
    if git rev-parse --verify HEAD >/dev/null 2>&1; then
        MSG="Update Solar Rover files"
    else
        MSG="Solar Rover v0.2: 3D parts, wiring, firmware, phone app, simulator and agent"
    fi
    git commit -q -m "$MSG" -m "Co-Authored-By: Claude <noreply@anthropic.com>"
    echo "==> Committed: $MSG"
fi

if git remote get-url origin >/dev/null 2>&1; then
    echo "==> Pushing to $(git remote get-url origin)"
    git push -u origin main
elif gh repo view "$USER_LOGIN/$NAME" >/dev/null 2>&1; then
    echo "==> $USER_LOGIN/$NAME already exists on GitHub: connecting this folder to it"
    git remote add origin "https://github.com/$USER_LOGIN/$NAME.git"
    git push -u origin main
else
    echo "==> Creating $USER_LOGIN/$NAME (${VISIBILITY#--}) and pushing"
    gh repo create "$NAME" "$VISIBILITY" --source="$REPO_DIR" --remote=origin --push
fi

echo
echo "Done: https://github.com/$USER_LOGIN/$NAME"
echo "The Actions tab there builds both firmwares and runs the tests on every push."
