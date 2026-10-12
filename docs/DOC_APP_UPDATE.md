### Update System

This repository is now integrated with GitHub Release update capabilities, with the following update dependencies:

- Update entry point and status handling on the main program side

- Release check, download, installation, and startup logic for `3rdparty/github-releases-autoupdater`

- CandyLauncher update checking is no longer just "open GitHub releases in browser"
- There are multiple real update entry points:
    - startup auto-check when `pref_check_app_version` is enabled
    - settings page button `pref_check_version_update`
    - tray menu / feature launch "check update"
- These entry points should share one real update flow

### Release Format

- CandyLauncher currently publishes the latest release mainly as a portable package
- Example:
    - tag: `1.1`
    - asset: `CandyLauncher.zip`
- So update handling must support portable self-update, not only installer launch

### Update Asset Rules

- Prefer `.zip` assets first
- Fall back to `.exe` / `.msi` only if no `.zip` asset exists
- Never use the release HTML page itself as the download target

### Portable Update Flow

- Download the zip package to a temp path
- Launch a helper step such as a temporary script or future `Updater.exe`
- Wait for `CandyLauncher.exe` to exit
- Extract the zip and copy files over the current app directory
- Restart CandyLauncher

### Versioning (single source of truth)

- The app version is defined once: `project(CandyLauncher VERSION x.y.z)` in `CMakeLists.txt`
- CMake `configure_file`s `common/AppVersion.h.in` into `<build>/generated/common/AppVersion.h`, which provides:
    - `APP_VERSION_STRING` (`"1.2.0"`), `APP_VERSION_RC` (`1,2,0,0`), `APP_VERSION_MAJOR/MINOR/PATCH`, `APP_ARCH_STRING`
- Consumers of that header (never hard-code a version anywhere else):
    - `CandyLauncher.rc` and `skin_editor/SkinEditor.rc` `VERSIONINFO` blocks (file properties in Explorer)
    - `util/UpdateManager.hpp` `ExtractCurrentVersionString()` passed to the GitHub updater
    - `common/settings.json` about-page item `pref_version_name` uses `{version}` / `{arch}` placeholders, expanded in `util/MyJsonUtil.hpp`
- To release a new version:
    1. Bump `project(CandyLauncher VERSION x.y.z)` and re-run CMake generate
    2. Build, package `CandyLauncher.zip`
    3. Create the GitHub release with tag `vx.y.z` (SemVer, `v` prefix); the updater strips the `v` before comparing
- Release comparison uses natural version comparison, so mixed styles can block update prompts
    - Example that broke before: local `1.0.20260414` vs tag `1.1` => local treated as newer
    - Legacy tags (`0.0.1`, `0.1`, `1.1`, ...) are all below `1.2.0`; keep every new tag above the previous one


### For portable zip updates, the current approach is:

- Download the zip file

- Generate a temporary update script or launch a standalone updater

- Wait for the main program to exit

- Overwrite the current directory

- Automatically restart

### Recommended Direction

- If CandyLauncher keeps shipping as a portable zip package, adding a dedicated `Updater.exe` is recommended
- `Updater.exe` should eventually own:
    - waiting for the main app to exit
    - extracting the downloaded zip
    - replacing files safely
    - handling elevation if needed
    - restarting the app

## Considerations When Modifying Update Functionality

- Asset selection rules must prioritize matching CandyLauncher's actual release format.

- Do not treat release HTML pages as download assets.

- ZIP updates must consider the main program file size.

- If the program is installed in a protected directory, updates may require administrator privileges.

- If the internal directory structure of the ZIP changes, the overwrite logic must be adjusted accordingly.

## Most Important Engineering Conclusion

CandyLauncher's update functionality is essentially no longer an "installer update," but a "portable self-update."

All subsequent update-related changes should be designed primarily around this premise.
