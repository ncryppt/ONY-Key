#!/usr/bin/env bash
# Codesigns and notarizes the macOS plugin bundles (VST3/AU/Standalone) built
# by CMake. Requires a Developer ID Application certificate installed in
# your keychain, and a notarytool keychain profile set up once via:
#   xcrun notarytool store-credentials "ONYVA-notary" \
#       --apple-id "you@example.com" --team-id TEAMID --password "app-specific-password"
#
# Usage:
#   ONYVA_SIGNING_IDENTITY="Developer ID Application: ONYVA (TEAMID)" \
#   ONYVA_NOTARY_PROFILE="ONYVA-notary" \
#   ./scripts/sign_and_notarize_macos.sh [build-dir]
#
# Both env vars are required; the script refuses to run without them rather
# than silently skipping signing.

set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="${ONYVA_BUILD_CONFIG:-Release}"

: "${ONYVA_SIGNING_IDENTITY:?Set ONYVA_SIGNING_IDENTITY to your Developer ID Application identity}"
: "${ONYVA_NOTARY_PROFILE:?Set ONYVA_NOTARY_PROFILE to your notarytool keychain profile name}"

ARTEFACTS_DIR="${BUILD_DIR}/ONYKey_artefacts/${CONFIG}"

BUNDLES=(
    "VST3/ONY Key.vst3"
    "AU/ONY Key.component"
    "Standalone/ONY Key.app"
)

for bundle in "${BUNDLES[@]}"; do
    path="${ARTEFACTS_DIR}/${bundle}"
    if [[ ! -e "$path" ]]; then
        echo "Skipping (not built): $path"
        continue
    fi

    echo "== Signing: $path =="
    codesign --deep --force --options runtime --timestamp \
        --sign "$ONYVA_SIGNING_IDENTITY" "$path"
    codesign --verify --deep --strict --verbose=2 "$path"

    zip_path="${path%.*}.zip"
    echo "== Zipping for notarization: $zip_path =="
    ditto -c -k --keepParent "$path" "$zip_path"

    echo "== Submitting for notarization =="
    xcrun notarytool submit "$zip_path" --keychain-profile "$ONYVA_NOTARY_PROFILE" --wait

    echo "== Stapling ticket =="
    xcrun stapler staple "$path"

    rm -f "$zip_path"
    echo "== Done: $path =="
done

echo "All available bundles signed and notarized."
