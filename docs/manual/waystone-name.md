# Waystone BBS: name and compatibility

On 7 October 2026, Allen selected Waystone BBS as the identity for the continuing Talisman development fork. The lineage remains Titan / Magicka → Talisman → Waystone; this records the relationship between projects, not a claim that all three earlier products were simple renames.

The repository README, manual title, session startup/version banners and default system display name use Waystone BBS. The original Andrew Pamment and Lawrence Stockman copyright attribution remains visible. GPLv3 licensing and existing copyright notices remain in place.

This branding change retains `talisman.ini`, the `talisman` executable/build target, existing component/source directory names, persistent data formats, door interfaces and configured network identities. An installation's explicitly configured System Name remains its operator's choice. The sample QWK ID and existing QWK/network settings are not renamed automatically. No repository URL change, database migration or new numbered release is performed.

Version constants retain the inherited `0.54-dev` identity pending an explicit release/versioning decision. Historical documentation, original samples/artwork and recorded verification evidence may still say Talisman; their names identify their provenance. Follow [the historical GitLab notes](historical-gitlab-wiki.md) for older version claims and [extended-color artwork](extended-color-artwork.md) for changes made by this continuation.

Existing strings.dat customizations keyed on the original startup/version format strings need corresponding Waystone keys if operators want to customize the new banners. Other configuration filenames and keys remain compatible. Rebuild the session executable to display the new runtime branding.

This change is limited to branding/default display text and documentation. It does not claim additional functionality or new runtime acceptance evidence.
