#!/usr/bin/env bash
# One-time GitHub setup: labels and milestones.
# The 9 phase issues are created manually on github.
# Requirements: GitHub CLI (gh), logged in with `gh auth login`.
# Run from the repo folder in Git Bash:  bash tools/github_setup.sh
set -e

echo "Creating labels..."
gh label create core          --color 0E7490 --description "Core data structures and algorithms" --force
gh label create dsa           --color 6D28D9 --description "Data structure work"                  --force
gh label create relief-logic  --color B45309 --description "Requests, depots, allocation"         --force
gh label create gui           --color 0891B2 --description "raylib interface"                     --force
gh label create test          --color 15803D --description "Tests"                                --force
gh label create docs          --color 64748B --description "Documentation"                        --force
gh label create bug           --color DC2626 --description "Something is broken"                  --force
gh label create enhancement   --color 2563EB --description "New feature or improvement"           --force

echo "Creating milestones..."
gh api "repos/{owner}/{repo}/milestones" -f title="v1.0 Course submission" -f due_on="2026-11-22T18:59:59Z" \
  -f description="Everything in the approved proposal plus the raylib GUI. Due Sun Nov 22 (PKT)." >/dev/null
gh api "repos/{owner}/{repo}/milestones" -f title="v2.0 Later" \
  -f description="Inspector, smooth map, moving trucks and other upgrades." >/dev/null

echo "Done. Open your repo's Labels and Milestones pages to check."