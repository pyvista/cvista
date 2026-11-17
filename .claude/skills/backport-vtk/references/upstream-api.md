# Upstream VTK API and triage reference

Upstream is `vtk/vtk` on `gitlab.kitware.com`, **project id `13`**, default branch `master`.
cvista is frozen on tag `v9.6.2`.

Read MR history from the read-only mirror at <https://vtk-archive.pyvista.org> by default. It
is a static site on a CDN with the same paths as GitLab, so a sweep adds no load to Kitware's
server. Its snapshot date is at the top of <https://vtk-archive.pyvista.org/llms.txt>. Raw
diffs come from `github.com/Kitware/VTK`, which carries the same commit SHAs.
`gitlab.kitware.com` remains the record of truth; go there when the archive lacks an MR or is
older than the state you need, a page at a time.

## List merged MRs since the watermark

```bash
# MR list, newest iid first. Page until you pass the watermark iid.
curl -s "https://vtk-archive.pyvista.org/vtk/vtk/-/merge_requests/page/1" \
  | grep -oE '/merge_requests/[0-9]+"' | grep -oE '[0-9]+' | uniq

# Per MR: title, state, and merge date from the Markdown front matter.
curl -s "https://vtk-archive.pyvista.org/vtk/vtk/-/merge_requests/<iid>.md" \
  | sed -n '/^---$/,/^---$/p' | grep -E '^(title|state|merged):'
```

Keep the MRs with `state: merged`. The body of the `.md` is the MR description. VTK labels are
inconsistent, so **do not rely on labels** for triage; use the title and the changed files.

## Fetch the MR diff and its changed files

VTK merges each MR as a `Merge topic` commit whose message carries `Merge-request: !<iid>`.
Find that commit on GitHub and diff it against its first parent:

```bash
M=$(gh api -X GET search/commits -f q='repo:Kitware/VTK "Merge-request: !<iid>"' --jq '.items[0].sha')
curl -sL "https://github.com/Kitware/VTK/compare/$M%5E1...$M.diff" -o /tmp/mr-<iid>.diff

# Changed files:
grep '^diff --git' /tmp/mr-<iid>.diff | awk '{print substr($4, 3)}'

# Apply onto the cvista tree (no shared history, so 3-way against current paths):
git apply --3way --whitespace=nowarn /tmp/mr-<iid>.diff
```

If `git apply` rejects hunks (the 9.6.2 tree has drifted from upstream master), port the change
by hand: open each rejected file, read the MR's intent from the diff and description, and make
the equivalent edit against the 9.6.2 source. The diff is the guide, not a mechanical patch.

## Triage heuristics

Decide per MR. The cheapest, strongest signal is whether the changed files even exist in cvista.

1. **Shipped-module filter (run first).** For every changed path, `git ls-files <path>`. If all
   paths are absent from the cvista tree, the module was trimmed — **skip**. (Cross-check the trim
   lists in `cvista-config/` if a path is present but the class might be NOWRAP/NOCOMPILE.)

2. **Category from the title and diff:**
   - crash / segfault / nullptr / use-after-free / leak / overflow → **take** (correctness, high
     value).
   - wrong output / incorrect result / regression on a shipped filter or reader → **take**.
   - render / OpenGL / EGL correctness on the shipped rendering stack → **take** (validate with
     renderexact).
   - new feature / API addition on a shipped module → **case by case**; take if PyVista benefits.
   - performance / optimization → **defer** by default. cvista has its own perf line under the
     parity contract; an upstream perf change may conflict or duplicate. Flag for a human.
   - CI / docs / formatting / ThirdParty version bump / deprecation churn / build infra →
     **skip**.

3. **Parity impact.** Note whether the fix changes valid-input output (a deliberate divergence
   from stock 9.6.2 that needs a bitexact-op update + divergence ledger entry) or only handles
   degenerate input (output unchanged for valid input; add a regression test). See the
   `parity-contract` skill.

4. **When genuinely unsure**, mark `pending` in the ledger and surface it. Do not auto-land an
   ambiguous behavioral change.

## Linking back

Every backport PR body links the MR:

```
Backports https://gitlab.kitware.com/vtk/vtk/-/merge_requests/<iid>
```

This is the provenance record. The ledger `docs/upstream-backports.md` is the index; the PR link
is the per-change trail.
