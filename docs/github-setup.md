# Put the project on GitHub (Ubuntu)

Copy and paste each block into a terminal. Every block starts with an absolute path, so it doesn't matter which folder the terminal is in.

## Fastest way: one block

Download `solar-rover.zip` (any Downloads folder is fine, including the Windows one when you use Ubuntu in WSL, and a renamed copy like `solar-rover (1).zip` is fine too), then paste:

```bash
cd "$HOME"
sudo apt update && sudo apt install -y unzip git
ZIP="$(ls -t "$HOME"/Downloads/solar-rover*.zip /mnt/c/Users/*/Downloads/solar-rover*.zip 2>/dev/null | head -n 1)"
SRC="$(find "$HOME/Downloads" /mnt/c/Users/*/Downloads -maxdepth 4 -path '*solar-rover*/tools/publish.sh' 2>/dev/null | head -n 1)"
if [ -n "$ZIP" ]; then echo "Using $ZIP"; unzip -oq "$ZIP" -d "$HOME"
elif [ -n "$SRC" ]; then echo "Using folder ${SRC%/tools/publish.sh}"; mkdir -p "$HOME/solar-rover"; cp -r "${SRC%/tools/publish.sh}/." "$HOME/solar-rover/"
else echo "Could not find solar-rover.zip in Downloads (Linux or Windows). Download it again, then rerun this block."; fi
bash "$HOME/solar-rover/tools/publish.sh"
```

The block picks the newest `solar-rover*.zip` from `~/Downloads` or from `C:\Users\<you>\Downloads` (which WSL sees as `/mnt/c/Users/<you>/Downloads`). If your browser already unpacked it into a folder, it copies that folder instead. Either way the project ends up in `~/solar-rover`.

`tools/publish.sh` does the whole job:
- installs git and the GitHub CLI if they're missing
- logs you in to GitHub the first time; choose **GitHub.com**, **HTTPS**, **Yes**, then **Login with a web browser**
- commits as your GitHub account, using its private noreply email
- creates the public repo `solar-rover` and pushes it

Add `--private` at the end of the last line (`.../publish.sh --private`) to make the repo private instead.

Run the same block again whenever you download a newer zip. The script commits what changed and pushes it.

The steps below do the same thing by hand.

## 1. Unpack the project into `~/solar-rover`

Save `solar-rover.zip` to your **Downloads** folder first. On Ubuntu in WSL the browser saves to the Windows Downloads folder, which is `/mnt/c/Users/<you>/Downloads`, not `~/Downloads`; change the path on the `unzip` line to match.

```bash
cd "$HOME"
sudo apt update && sudo apt install -y git unzip wget
unzip -o "$HOME/Downloads/solar-rover.zip" -d "$HOME"
cd "$HOME/solar-rover" && ls
```

The last line should list `README.md`, `docs`, `firmware`, `hardware` and so on.

## 2. Install the GitHub CLI and log in

This is GitHub's official install for Ubuntu:

```bash
cd "$HOME"
sudo mkdir -p -m 755 /etc/apt/keyrings
wget -qO- https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null
sudo chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt update && sudo apt install -y gh
gh auth login
```

In `gh auth login`, pick **GitHub.com**, then **HTTPS**, then **Yes** (authenticate Git), then **Login with a web browser**, and enter the code it shows.

## 3. Make the first commit

```bash
cd "$HOME/solar-rover"
git init -b main
git config user.name "Normansrule"
git config user.email "60046480+Normansrule@users.noreply.github.com"
git add -A
git commit -m "Solar Rover v0.2: 3D parts, wiring, firmware, phone app, simulator and agent" -m "Co-Authored-By: Claude <noreply@anthropic.com>"
```

The email is your private GitHub "noreply" address, so your real email isn't published in the history.

## 4. Create the repo on GitHub and push

```bash
cd "$HOME/solar-rover"
gh repo create solar-rover --public --source="$HOME/solar-rover" --remote=origin --push
gh repo view --web
```

- Use `--private` instead of `--public` to keep it hidden for now. A public repo can also serve the phone app over https on GitHub Pages, which is what lets Bluetooth work from Chrome ([software.md](software.md) section 3).
- After the push, the **Actions** tab compiles both firmwares and runs the tests. A green tick means the firmware builds.

## If `gh` doesn't work: plain git

1. On github.com, click **+** → **New repository**, name it `solar-rover`, and leave "Add a README" **unticked**.
2. Then:

```bash
cd "$HOME/solar-rover"
git remote add origin https://github.com/Normansrule/solar-rover.git
git push -u origin main
```

When git asks for a password, paste a **personal access token** (github.com → Settings → Developer settings → Personal access tokens → Fine-grained, with "Contents: read and write" on this repo). Your normal GitHub password won't work.

## Already pushed an older zip? Update the repo with the new one

Download the new zip and paste the one block from the top again. It unpacks the new files over `~/solar-rover`, and `publish.sh` commits what changed and pushes it. Your git history stays as it is.

## Later: send changes up

```bash
cd "$HOME/solar-rover"
git add -A
git commit -m "Describe what you changed"
git push
```

## If something goes wrong

| Message | Fix |
|---|---|
| `unzip: cannot find or open …/Downloads/solar-rover.zip` | On WSL the file is in the Windows Downloads folder: run `ls /mnt/c/Users/*/Downloads/*.zip` and use that path, or use the one-block method at the top, which looks there for you |
| `fatal: not a git repository` | You skipped the `cd "$HOME/solar-rover"` line at the top of the block |
| `error: remote origin already exists` | Run `git -C "$HOME/solar-rover" remote remove origin`, then repeat step 4 |
| `GraphQL: Name already exists on this account` | The repo already exists: use the plain git commands above |
| `Author identity unknown` | Run the two `git config` lines in step 3 again |
