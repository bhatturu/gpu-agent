# Contribution to Pensando Repo

- create a fork from the main repo to current $USER

# Setup upstream tracking (one time)
- follow the below steps to create a track upstream branch for the cloned $USER repo

```bash
git clone https://github.com/$USER/device-metrics-exporter.git
cd device-metrics-exporter
# add upstream tracking info and sync your main with upstream main
git checkout main
git remote add upstream git@github.com:pensando/device-metrics-exporter.git
git remote set-url --push upstream no_push
git remote -v show
git fetch upstream
git rebase upstream/main
git push
git submodule update --init --recursive
git push
```

# Sync upstream changes to your local repo

```bash
git checkout main
git fetch upstream
git rebase upstream/main
git push
git submodule update --init --recursive
git push
```

# Rebase local branch to Upstream branch
- Need to sync the upstream to local repo as in the above step
- Then to update a specific branch $NEW_LOCAL_REPO_BRANCH follow the below steps.

```bash
git checkout $NEW_LOCAL_REPO_BRANCH
git rebase upstream/main
git push -f
```


# Tips
- Don't develop on the same branch from upstream
- Create a local branch from the shadow copy of 
  the upstream branch you want to work on. 
  This helps rebasing to be cleaner and avoid conflict.
- If your branch has diverged from main a lot with your changes, 
  consider creating a new branch from upstream then cherry-pick commits.
