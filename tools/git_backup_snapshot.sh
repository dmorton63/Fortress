#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "ERROR: not inside a git repository" >&2
  exit 1
fi

remote="${BACKUP_REMOTE:-origin}"
tag_prefix="${BACKUP_TAG_PREFIX:-backup}"
branch="$(git branch --show-current)"

if [[ -z "${branch}" ]]; then
  echo "ERROR: could not detect current branch" >&2
  exit 1
fi

if ! git remote get-url "${remote}" >/dev/null 2>&1; then
  echo "ERROR: remote '${remote}' is not configured" >&2
  exit 1
fi

timestamp="$(date -u +%Y%m%d-%H%M%S)"
tag_name="${tag_prefix}-${timestamp}"

# Capture both tracked and untracked changes before snapshotting.
if [[ -n "$(git status --porcelain)" ]]; then
  git add -A
  git commit -m "Backup snapshot ${timestamp}"
else
  echo "No working tree changes detected; skipping commit."
fi

git tag -a "${tag_name}" -m "Backup snapshot ${timestamp}"
git push "${remote}" "${branch}"
git push "${remote}" "${tag_name}"

echo "Backup complete"
echo "  Remote : ${remote}"
echo "  Branch : ${branch}"
echo "  Tag    : ${tag_name}"
