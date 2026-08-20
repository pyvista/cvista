#!/usr/bin/env bash
#
# Release preflight: every commit since the previous tag must be accounted for in
# the release notes.
#
# 9.6.2.5 was cut with notes describing one of its two commits. The other (#252,
# a vtkTriangleFilter storage change) went out undisclosed, and when PyVista's
# gate went red the same day, the undisclosed change was a live suspect for
# hours. Notes that cover the range make that impossible.
#
#   Usage: ci/check-release-notes.sh <tag> [notes-file]
#          notes-file defaults to reading the GitHub release body for <tag>.
set -euo pipefail

TAG="${1:?usage: ci/check-release-notes.sh <tag> [notes-file]}"
NOTES_FILE="${2:-}"

PREV="$(git describe --tags --abbrev=0 "${TAG}^" 2>/dev/null || true)"
if [ -z "$PREV" ]; then
    echo ">>> no previous tag before $TAG; nothing to check"
    exit 0
fi

if [ -n "$NOTES_FILE" ]; then
    NOTES="$(cat "$NOTES_FILE")"
else
    NOTES="$(gh release view "$TAG" --json body -q .body)"
fi

echo ">>> commits in ${PREV}..${TAG}"
MISSING=()
while IFS= read -r line; do
    [ -n "$line" ] || continue
    # Squash-merged commits end with (#NNN); that number is the citation we want.
    if [[ "$line" =~ \(#([0-9]+)\)$ ]]; then
        pr="${BASH_REMATCH[1]}"
        if grep -q "#${pr}\b" <<<"$NOTES"; then
            echo "  ok      #${pr}  ${line}"
        else
            echo "  MISSING #${pr}  ${line}"
            MISSING+=("#${pr} ${line}")
        fi
    else
        echo "  (no PR reference, skipped) ${line}"
    fi
done < <(git log --format='%s' "${PREV}..${TAG}")

if [ ${#MISSING[@]} -gt 0 ]; then
    echo
    echo "ERROR: ${#MISSING[@]} commit(s) in ${PREV}..${TAG} are not referenced in the release notes:"
    printf '  %s\n' "${MISSING[@]}"
    echo
    echo "Add them to the notes (an 'Also in this release' section is fine) and re-run."
    exit 1
fi
echo ">>> all commits in ${PREV}..${TAG} are accounted for"
